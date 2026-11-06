// Pure logic of the crystal view: which crystal a name means, and building its atoms from the
// published unit cell. Plain C++ with no Arduino includes, tested on the host: tests/protein/run.sh
// The closed list lives in crystal_table.h, made by scripts/make_crystal_table.py.
#pragma once

#include "protein_core.h"

#define CRYSTAL_MAX_ATOMS 200
#define CRYSTAL_MAX_BONDS 400
#define CRYSTAL_EDGE_TOLERANCE 0.001f  // In cell fractions: atoms on the far faces of the block belong to it

struct CrystalSite {
  float x, y, z;  // Fractions of the cell
  uint8_t element;
};

struct CrystalBondRule {
  uint8_t a, b;
  float longest;  // In Angstrom
};

struct CrystalEntry {
  const char* key;
  const char* names;  // Separated by ';', matched after proteinNormalise
  const char* de;
  const char* en;
  const char* source;
  float a, b, c, alpha, beta, gamma;
  uint8_t repeatX, repeatY, repeatZ;
  const CrystalSite* sites;
  uint16_t siteCount;
  const CrystalBondRule* bonds;
  uint8_t bondRuleCount;
};

#include "crystal_table.h"

struct CrystalAtom {
  float x, y, z;
  uint8_t element;
};

// Index in the closed list, or -1. An exact name only: anything else gets the picture.
inline int crystalResolve(const char* query) {
  char wanted[PROTEIN_NORMALISED_SIZE];
  char name[PROTEIN_NORMALISED_SIZE];
  if (!proteinNormalise(query, wanted, sizeof(wanted))) return -1;
  for (int i = 0; i < CRYSTAL_COUNT; i++) {
    const char* p = CRYSTAL_ENTRIES[i].names;
    while (*p) {
      const char* semi = strchr(p, ';');
      const size_t len = semi ? (size_t)(semi - p) : strlen(p);
      if (len > 0 && len < sizeof(name)) {
        memcpy(name, p, len);
        name[len] = 0;
        char normal[PROTEIN_NORMALISED_SIZE];
        if (proteinNormalise(name, normal, sizeof(normal)) && strcmp(normal, wanted) == 0) return i;
      }
      p += len;
      if (*p == ';') p++;
    }
  }
  return -1;
}

// Columns are the cell's axes in Angstrom. The usual crystallographic setting: a along x, b in the xy plane.
inline bool crystalCellMatrix(const CrystalEntry& e, float m[9]) {
  const double toRad = 3.14159265358979323846 / 180.0;
  const double ca = cos(e.alpha * toRad), cb = cos(e.beta * toRad), cg = cos(e.gamma * toRad), sg = sin(e.gamma * toRad);
  if (sg < 1e-3) return false;
  const double cy = (ca - cb * cg) / sg;
  const double square = 1.0 - cb * cb - cy * cy;
  if (square <= 0) return false;
  const double cells[9] = {e.a, e.b * cg, e.c * cb, 0.0, e.b * sg, e.c * cy, 0.0, 0.0, e.c * sqrt(square)};
  for (int i = 0; i < 9; i++) m[i] = (float)cells[i];
  return true;
}

// Builds the block of repeated cells, with atoms on its far faces, and its bonds as adjacency:
// the neighbours of atom i are adj[adjStart[i]] up to adj[adjStart[i + 1] - 1].
// Centred, with the farthest atom at radius 1; angstromPerUnit gives back the scale. False if it doesn't fit.
inline bool crystalBuild(const CrystalEntry& e, CrystalAtom* atoms, uint16_t atomCapacity, uint16_t* adjStart,
                         uint16_t* adj, uint16_t adjCapacity, uint16_t& atomCount, uint16_t& bondCount,
                         float& angstromPerUnit) {
  atomCount = bondCount = 0;
  angstromPerUnit = 0;
  float m[9];
  if (!atoms || !adjStart || !adj || !crystalCellMatrix(e, m)) return false;
  const int rx = e.repeatX, ry = e.repeatY, rz = e.repeatZ;
  uint16_t n = 0;
  float sx = 0, sy = 0, sz = 0;
  for (uint16_t s = 0; s < e.siteCount; s++) {
    const CrystalSite& site = e.sites[s];
    for (int i = 0; i <= rx; i++) {
      for (int j = 0; j <= ry; j++) {
        for (int k = 0; k <= rz; k++) {
          const float fx = site.x + i, fy = site.y + j, fz = site.z + k;
          if (fx > rx + CRYSTAL_EDGE_TOLERANCE || fy > ry + CRYSTAL_EDGE_TOLERANCE || fz > rz + CRYSTAL_EDGE_TOLERANCE) continue;
          if (n >= atomCapacity) return false;
          CrystalAtom& a = atoms[n++];
          a.x = m[0] * fx + m[1] * fy + m[2] * fz;
          a.y = m[3] * fx + m[4] * fy + m[5] * fz;
          a.z = m[6] * fx + m[7] * fy + m[8] * fz;
          a.element = site.element;
          sx += a.x; sy += a.y; sz += a.z;
        }
      }
    }
  }
  if (n < 2) return false;
  sx /= n; sy /= n; sz /= n;
  float maxR2 = 0;
  for (uint16_t i = 0; i < n; i++) {
    atoms[i].x -= sx; atoms[i].y -= sy; atoms[i].z -= sz;
    const float r2 = atoms[i].x * atoms[i].x + atoms[i].y * atoms[i].y + atoms[i].z * atoms[i].z;
    if (r2 > maxR2) maxR2 = r2;
  }
  if (!(maxR2 > 0)) return false;

  // Two passes over all pairs: count each atom's neighbours, then fill them in
  memset(adjStart, 0, sizeof(uint16_t) * (n + 1));
  uint32_t half = 0;
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      for (uint16_t i = 0; i < n; i++) adjStart[i + 1] += adjStart[i];
      half = adjStart[n];
      if (half > adjCapacity) return false;
    }
    for (uint16_t i = 0; i < n; i++) {
      for (uint16_t j = i + 1; j < n; j++) {
        bool applies = false;
        float longest = 0;
        for (uint8_t r = 0; r < e.bondRuleCount; r++) {
          const CrystalBondRule& rule = e.bonds[r];
          if ((rule.a == atoms[i].element && rule.b == atoms[j].element) ||
              (rule.b == atoms[i].element && rule.a == atoms[j].element)) {
            applies = true;
            longest = rule.longest;
          }
        }
        if (!applies) continue;
        const float dx = atoms[i].x - atoms[j].x, dy = atoms[i].y - atoms[j].y, dz = atoms[i].z - atoms[j].z;
        if (dx * dx + dy * dy + dz * dz > longest * longest) continue;
        if (pass == 0) {
          adjStart[i + 1]++;
          adjStart[j + 1]++;
        } else {
          // adjStart[i] walks forward while filling, and is put back below
          adj[adjStart[i]++] = j;
          adj[adjStart[j]++] = i;
        }
      }
    }
  }
  for (uint16_t i = n; i > 0; i--) adjStart[i] = adjStart[i - 1];
  adjStart[0] = 0;
  if (half / 2 > CRYSTAL_MAX_BONDS) return false;

  const float radius = sqrtf(maxR2);
  const float scale = 1.0f / radius;
  for (uint16_t i = 0; i < n; i++) {
    atoms[i].x *= scale; atoms[i].y *= scale; atoms[i].z *= scale;
  }
  atomCount = n;
  bondCount = (uint16_t)(half / 2);
  angstromPerUnit = radius;
  return true;
}

// Fewer atoms can be drawn larger: big clear shapes
inline uint8_t crystalAtomRadius(uint16_t atoms, uint8_t element, uint8_t zoomStep) {
  uint8_t r = atoms <= 30 ? 8 : (atoms <= 80 ? 6 : 4);
  if (element <= 10 && r > 3) r--;  // Light atoms such as C and O are smaller than Na or Cl
  if (zoomStep > 0) r = (uint8_t)(r + zoomStep);
  return r;
}
