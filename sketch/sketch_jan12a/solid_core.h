// Geometric solids, computed: corner points from exact formulas, faces from the convex hull.
// Plain C++ with no Arduino includes, tested on the host: tests/physics/run.sh
//
// Polyhedra keep their logical faces (a cube has 6 squares, not 12 triangles), so the face,
// edge and vertex counts on screen are the shape's true counts. Curved solids are drawn as
// many small faces and have no counts at all: school books differ on them.
#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SOLID_MAX_VERTICES 220
#define SOLID_MAX_FACES 220  // Below 256: the depth sort works with uint8_t indices
#define SOLID_MAX_INDICES 1000
#define SOLID_ROUND_STEPS 24
#define SOLID_SPHERE_RINGS 10
#define SOLID_SPHERE_STEPS 16
#define SOLID_TORUS_STEPS 20
#define SOLID_TORUS_TUBE 10

static const float SOLID_PI = 3.14159265358979f;
static const float SOLID_GOLDEN = 1.61803398875f;

enum SolidShape : uint8_t {
  SOLID_TETRAHEDRON,
  SOLID_CUBE,
  SOLID_OCTAHEDRON,
  SOLID_DODECAHEDRON,
  SOLID_ICOSAHEDRON,
  SOLID_CUBOID,
  SOLID_PRISM,
  SOLID_PYRAMID,
  SOLID_CYLINDER,
  SOLID_CONE,
  SOLID_SPHERE,
  SOLID_TORUS
};

struct SolidInfo {
  const char* id;
  const char* de;
  const char* en;
  SolidShape shape;
  uint8_t sides;   // Prisms and pyramids: corners of the base
  uint16_t color;  // RGB565, lit in the view
};

static const SolidInfo SOLID_TABLE[] = {
    {"tetrahedron", "Tetraeder", "Tetrahedron", SOLID_TETRAHEDRON, 0, 0xF5A0},
    {"cube", "W\xC3\xBC" "rfel", "Cube", SOLID_CUBE, 0, 0x2D7F},
    {"octahedron", "Oktaeder", "Octahedron", SOLID_OCTAHEDRON, 0, 0x4F2A},
    {"dodecahedron", "Dodekaeder", "Dodecahedron", SOLID_DODECAHEDRON, 0, 0xFB2C},
    {"icosahedron", "Ikosaeder", "Icosahedron", SOLID_ICOSAHEDRON, 0, 0xB39F},
    {"cuboid", "Quader", "Cuboid", SOLID_CUBOID, 0, 0x2D7F},
    {"triangular-prism", "Dreiecksprisma", "Triangular prism", SOLID_PRISM, 3, 0x4F2A},
    {"pentagonal-prism", "F\xC3\xBC" "nfeckprisma", "Pentagonal prism", SOLID_PRISM, 5, 0x4F2A},
    {"hexagonal-prism", "Sechseckprisma", "Hexagonal prism", SOLID_PRISM, 6, 0x4F2A},
    {"triangular-pyramid", "Dreieckspyramide", "Triangular pyramid", SOLID_PYRAMID, 3, 0xF5A0},
    {"square-pyramid", "Quadratische Pyramide", "Square pyramid", SOLID_PYRAMID, 4, 0xF5A0},
    {"pentagonal-pyramid", "F\xC3\xBC" "nfeckpyramide", "Pentagonal pyramid", SOLID_PYRAMID, 5, 0xF5A0},
    {"hexagonal-pyramid", "Sechseckpyramide", "Hexagonal pyramid", SOLID_PYRAMID, 6, 0xF5A0},
    {"cylinder", "Zylinder", "Cylinder", SOLID_CYLINDER, 0, 0x2D7F},
    {"cone", "Kegel", "Cone", SOLID_CONE, 0, 0xFB2C},
    {"sphere", "Kugel", "Sphere", SOLID_SPHERE, 0, 0xF5A0},
    {"torus", "Torus", "Torus", SOLID_TORUS, 0, 0xB39F},
};
static const int SOLID_TABLE_SIZE = sizeof(SOLID_TABLE) / sizeof(SOLID_TABLE[0]);

// Added to the AI prompt. Every id in it is in SOLID_TABLE and the other way round (tested).
static const char SOLID_PROMPT_RULE[] =
    "GEOMETRISCHE KOERPER: Setze 'model' auf 'solid:<id>' NUR wenn der Nutzer einen dieser Koerper sehen will, "
    "fragt wie er aussieht oder ihn in 3D will (z.B. 'Zeig mir einen Torus', 'Visualisiere einen Wuerfel', "
    "'Wie sieht ein Ikosaeder aus?'). id genau eine von: tetrahedron cube octahedron dodecahedron icosahedron "
    "cuboid triangular-prism pentagonal-prism hexagonal-prism triangular-pyramid square-pyramid "
    "pentagonal-pyramid hexagonal-pyramid cylinder cone sphere torus. Ohne genauere Angabe ergibt Pyramide "
    "square-pyramid und Prisma triangular-prism. Berechnungen ('Volumen eines Torus mit "
    "Radius 5 cm') bleiben is_wolfram mit leerem model. Nenne nur die id, die Zahlen berechnet das Geraet.";

struct SolidFace {
  uint16_t start;  // Into SolidMesh::indices
  uint8_t count;
  float nx, ny, nz;  // Outward, unit length
  float cx, cy, cz;  // Middle of the face
};

struct SolidMesh {
  uint16_t vertexCount, faceCount, indexCount;
  bool curved;
  // Polyhedra only; 0 on curved solids. From the logical shape, never from drawn triangles.
  uint16_t faces, edges, vertices;
  float v[SOLID_MAX_VERTICES][3];
  SolidFace f[SOLID_MAX_FACES];
  uint8_t indices[SOLID_MAX_INDICES];
};

// Index into SOLID_TABLE, or -1. Case and spaces don't matter; anything else must match exactly.
inline int solidFindId(const char* query) {
  if (!query) return -1;
  char clean[32];
  size_t n = 0;
  for (const char* p = query; *p; p++) {
    if (n + 1 >= sizeof(clean)) return -1;
    char c = *p;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == ' ' || c == '_') c = '-';
    clean[n++] = c;
  }
  clean[n] = 0;
  for (int i = 0; i < SOLID_TABLE_SIZE; i++) {
    if (strcmp(SOLID_TABLE[i].id, clean) == 0) return i;
  }
  return -1;
}

inline bool solidAddVertex(SolidMesh& m, float x, float y, float z) {
  if (m.vertexCount >= SOLID_MAX_VERTICES) return false;
  m.v[m.vertexCount][0] = x;
  m.v[m.vertexCount][1] = y;
  m.v[m.vertexCount][2] = z;
  m.vertexCount++;
  return true;
}

// Adds a face from its corner indices, counter-clockwise seen from outside. Normal and middle
// come from the corners (Newell's method), so bent or backwards faces show up in the tests.
inline bool solidAddFace(SolidMesh& m, const uint8_t* corners, uint8_t count) {
  if (m.faceCount >= SOLID_MAX_FACES || m.indexCount + count > SOLID_MAX_INDICES || count < 3) return false;
  SolidFace& f = m.f[m.faceCount];
  f.start = m.indexCount;
  f.count = count;
  float nx = 0, ny = 0, nz = 0, cx = 0, cy = 0, cz = 0;
  for (uint8_t i = 0; i < count; i++) {
    const float* a = m.v[corners[i]];
    const float* b = m.v[corners[(i + 1) % count]];
    nx += (a[1] - b[1]) * (a[2] + b[2]);
    ny += (a[2] - b[2]) * (a[0] + b[0]);
    nz += (a[0] - b[0]) * (a[1] + b[1]);
    cx += a[0]; cy += a[1]; cz += a[2];
    m.indices[m.indexCount++] = corners[i];
  }
  const float length = sqrtf(nx * nx + ny * ny + nz * nz);
  if (length < 1e-9f) {
    m.indexCount = f.start;
    return false;
  }
  f.nx = nx / length; f.ny = ny / length; f.nz = nz / length;
  f.cx = cx / count; f.cy = cy / count; f.cz = cz / count;
  m.faceCount++;
  return true;
}

inline void solidPlaceAndScale(SolidMesh& m) {
  float sx = 0, sy = 0, sz = 0;
  for (uint16_t i = 0; i < m.vertexCount; i++) {
    sx += m.v[i][0]; sy += m.v[i][1]; sz += m.v[i][2];
  }
  sx /= m.vertexCount; sy /= m.vertexCount; sz /= m.vertexCount;
  float maxR2 = 0;
  for (uint16_t i = 0; i < m.vertexCount; i++) {
    m.v[i][0] -= sx; m.v[i][1] -= sy; m.v[i][2] -= sz;
    const float r2 = m.v[i][0] * m.v[i][0] + m.v[i][1] * m.v[i][1] + m.v[i][2] * m.v[i][2];
    if (r2 > maxR2) maxR2 = r2;
  }
  const float s = maxR2 > 0 ? 1.0f / sqrtf(maxR2) : 1.0f;
  for (uint16_t i = 0; i < m.vertexCount; i++) {
    m.v[i][0] *= s; m.v[i][1] *= s; m.v[i][2] *= s;
  }
}

// Finds the true faces of a convex polyhedron from its corners: three corners with all others
// on one side span a face, and every corner on that plane belongs to it. No hand-typed face
// lists to get wrong. The corners must be centred first. False if the shape doesn't close.
inline bool solidHullFaces(SolidMesh& m) {
  const float tolerance = 1e-3f;
  const int n = m.vertexCount;
  if (n > 32) return false;  // Only polyhedra come here; the search grows quickly
  for (int a = 0; a < n; a++) {
    for (int b = a + 1; b < n; b++) {
      for (int c = b + 1; c < n; c++) {
        const float* A = m.v[a];
        const float* B = m.v[b];
        const float* C = m.v[c];
        const float ux = B[0] - A[0], uy = B[1] - A[1], uz = B[2] - A[2];
        const float vx = C[0] - A[0], vy = C[1] - A[1], vz = C[2] - A[2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float length = sqrtf(nx * nx + ny * ny + nz * nz);
        if (length < tolerance) continue;  // In a straight line
        nx /= length; ny /= length; nz /= length;
        int above = 0, below = 0;
        uint8_t on[16];
        int onCount = 0;
        for (int i = 0; i < n; i++) {
          const float d = (m.v[i][0] - A[0]) * nx + (m.v[i][1] - A[1]) * ny + (m.v[i][2] - A[2]) * nz;
          if (d > tolerance) above++;
          else if (d < -tolerance) below++;
          else if (onCount < 16) on[onCount++] = (uint8_t)i;
        }
        if (above && below) continue;
        // Each face is found from its three lowest corners once
        if (on[0] != a || on[1] != b || on[2] != c) continue;
        if (above) { nx = -nx; ny = -ny; nz = -nz; }  // Point the normal outward
        float cx = 0, cy = 0, cz = 0;
        for (int i = 0; i < onCount; i++) {
          cx += m.v[on[i]][0]; cy += m.v[on[i]][1]; cz += m.v[on[i]][2];
        }
        cx /= onCount; cy /= onCount; cz /= onCount;
        // Sort the corners by angle around the middle, seen from outside
        float ax = m.v[on[0]][0] - cx, ay = m.v[on[0]][1] - cy, az = m.v[on[0]][2] - cz;
        const float ah = sqrtf(ax * ax + ay * ay + az * az);
        ax /= ah; ay /= ah; az /= ah;
        const float bx = ny * az - nz * ay, by = nz * ax - nx * az, bz = nx * ay - ny * ax;
        float angle[16];
        for (int i = 0; i < onCount; i++) {
          const float* p = m.v[on[i]];
          const float dx = p[0] - cx, dy = p[1] - cy, dz = p[2] - cz;
          angle[i] = atan2f(dx * bx + dy * by + dz * bz, dx * ax + dy * ay + dz * az);
        }
        for (int i = 1; i < onCount; i++) {
          const float ta = angle[i];
          const uint8_t to = on[i];
          int j = i - 1;
          while (j >= 0 && angle[j] > ta) {
            angle[j + 1] = angle[j];
            on[j + 1] = on[j];
            j--;
          }
          angle[j + 1] = ta;
          on[j + 1] = to;
        }
        if (!solidAddFace(m, on, (uint8_t)onCount)) return false;
      }
    }
  }
  return m.faceCount >= 4;
}

// Counts from the logical faces: each edge is shared by two faces and counted once
inline void solidCountLogical(SolidMesh& m) {
  uint16_t edges = 0;
  for (uint16_t i = 0; i < m.faceCount; i++) {
    const SolidFace& f = m.f[i];
    for (uint8_t k = 0; k < f.count; k++) {
      const uint8_t a = m.indices[f.start + k];
      const uint8_t b = m.indices[f.start + (k + 1) % f.count];
      if (a < b) edges++;  // The neighbouring face walks the same edge from b to a
    }
  }
  m.faces = m.faceCount;
  m.edges = edges;
  m.vertices = m.vertexCount;
}

inline void solidRingCorners(SolidMesh& m, int sides, float radius, float y) {
  for (int i = 0; i < sides; i++) {
    const float angle = 2 * SOLID_PI * i / sides;
    solidAddVertex(m, radius * cosf(angle), y, radius * sinf(angle));
  }
}

inline bool solidBuildPolyhedron(const SolidInfo& info, SolidMesh& m) {
  const float g = SOLID_GOLDEN, h = 1.0f / SOLID_GOLDEN;
  switch (info.shape) {
    case SOLID_TETRAHEDRON:
      solidAddVertex(m, 1, 1, 1);
      solidAddVertex(m, 1, -1, -1);
      solidAddVertex(m, -1, 1, -1);
      solidAddVertex(m, -1, -1, 1);
      break;
    case SOLID_CUBE:
    case SOLID_CUBOID: {
      const float dy = info.shape == SOLID_CUBE ? 1.0f : 0.7f;
      const float dz = info.shape == SOLID_CUBE ? 1.0f : 0.5f;
      for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
          for (int z = -1; z <= 1; z += 2) solidAddVertex(m, (float)x, y * dy, z * dz);
        }
      }
      break;
    }
    case SOLID_OCTAHEDRON:
      for (int s = -1; s <= 1; s += 2) {
        solidAddVertex(m, (float)s, 0, 0);
        solidAddVertex(m, 0, (float)s, 0);
        solidAddVertex(m, 0, 0, (float)s);
      }
      break;
    case SOLID_DODECAHEDRON:
      for (int a = -1; a <= 1; a += 2) {
        for (int b = -1; b <= 1; b += 2) {
          for (int c = -1; c <= 1; c += 2) solidAddVertex(m, (float)a, (float)b, (float)c);
          solidAddVertex(m, 0, a * h, b * g);
          solidAddVertex(m, a * h, b * g, 0);
          solidAddVertex(m, a * g, 0, b * h);
        }
      }
      break;
    case SOLID_ICOSAHEDRON:
      for (int a = -1; a <= 1; a += 2) {
        for (int b = -1; b <= 1; b += 2) {
          solidAddVertex(m, 0, (float)a, b * g);
          solidAddVertex(m, (float)a, b * g, 0);
          solidAddVertex(m, a * g, 0, (float)b);
        }
      }
      break;
    case SOLID_PRISM: {
      // Regular base, side walls as squares: edge = 2 * r * sin(pi / sides)
      const float edge = 2.0f * sinf(SOLID_PI / info.sides);
      solidRingCorners(m, info.sides, 1.0f, -edge / 2);
      solidRingCorners(m, info.sides, 1.0f, edge / 2);
      break;
    }
    case SOLID_PYRAMID:
      solidRingCorners(m, info.sides, 1.0f, 0);
      solidAddVertex(m, 0, 1.4f, 0);
      break;
    default:
      return false;
  }
  solidPlaceAndScale(m);
  if (!solidHullFaces(m)) return false;
  solidCountLogical(m);
  return true;
}

// Cylinder and cone: round wall and flat circles. topRadius 0 is a cone with one point on top.
inline bool solidBuildRound(SolidMesh& m, float topRadius) {
  const int n = SOLID_ROUND_STEPS;
  solidRingCorners(m, n, 1.0f, -1.0f);
  if (topRadius > 0) solidRingCorners(m, n, topRadius, 1.0f);
  else solidAddVertex(m, 0, 1.0f, 0);
  solidPlaceAndScale(m);
  for (int i = 0; i < n; i++) {
    const uint8_t a = (uint8_t)i, b = (uint8_t)((i + 1) % n);
    if (topRadius > 0) {
      const uint8_t wall[4] = {a, (uint8_t)(n + i), (uint8_t)(n + (i + 1) % n), b};
      if (!solidAddFace(m, wall, 4)) return false;
    } else {
      const uint8_t wall[3] = {a, (uint8_t)n, b};
      if (!solidAddFace(m, wall, 3)) return false;
    }
  }
  uint8_t ring[SOLID_ROUND_STEPS];
  for (int i = 0; i < n; i++) ring[i] = (uint8_t)i;
  if (!solidAddFace(m, ring, (uint8_t)n)) return false;
  if (topRadius > 0) {
    for (int i = 0; i < n; i++) ring[i] = (uint8_t)(n + (n - 1 - i));
    if (!solidAddFace(m, ring, (uint8_t)n)) return false;
  }
  return true;
}

inline bool solidBuildSphere(SolidMesh& m) {
  const int rings = SOLID_SPHERE_RINGS, steps = SOLID_SPHERE_STEPS;
  solidAddVertex(m, 0, 1, 0);
  for (int r = 1; r < rings; r++) {
    const float angle = SOLID_PI * r / rings;
    solidRingCorners(m, steps, sinf(angle), cosf(angle));
  }
  solidAddVertex(m, 0, -1, 0);
  const uint8_t bottom = (uint8_t)(m.vertexCount - 1);
  for (int s = 0; s < steps; s++) {
    const int t = (s + 1) % steps;
    const uint8_t top[3] = {0, (uint8_t)(1 + t), (uint8_t)(1 + s)};
    if (!solidAddFace(m, top, 3)) return false;
    for (int r = 0; r < rings - 2; r++) {
      const uint8_t q[4] = {(uint8_t)(1 + r * steps + s), (uint8_t)(1 + r * steps + t),
                            (uint8_t)(1 + (r + 1) * steps + t), (uint8_t)(1 + (r + 1) * steps + s)};
      if (!solidAddFace(m, q, 4)) return false;
    }
    const uint8_t low[3] = {bottom, (uint8_t)(1 + (rings - 2) * steps + s), (uint8_t)(1 + (rings - 2) * steps + t)};
    if (!solidAddFace(m, low, 3)) return false;
  }
  return true;
}

inline bool solidBuildTorus(SolidMesh& m) {
  const int steps = SOLID_TORUS_STEPS, tube = SOLID_TORUS_TUBE;
  const float ring = 0.68f, thick = 0.32f;  // Together 1: the outer edge is at radius 1
  for (int s = 0; s < steps; s++) {
    const float a = 2 * SOLID_PI * s / steps;
    for (int t = 0; t < tube; t++) {
      const float b = 2 * SOLID_PI * t / tube;
      const float r = ring + thick * cosf(b);
      solidAddVertex(m, r * cosf(a), thick * sinf(b), r * sinf(a));
    }
  }
  for (int s = 0; s < steps; s++) {
    const int s2 = (s + 1) % steps;
    for (int t = 0; t < tube; t++) {
      const int t2 = (t + 1) % tube;
      const uint8_t q[4] = {(uint8_t)(s * tube + t), (uint8_t)(s * tube + t2), (uint8_t)(s2 * tube + t2),
                            (uint8_t)(s2 * tube + t)};
      if (!solidAddFace(m, q, 4)) return false;
    }
  }
  return true;
}

// Builds SOLID_TABLE[index] into m. False for an unknown index or if it doesn't fit.
inline bool solidBuild(int index, SolidMesh& m) {
  m.vertexCount = m.faceCount = m.indexCount = 0;
  m.faces = m.edges = m.vertices = 0;
  m.curved = false;
  if (index < 0 || index >= SOLID_TABLE_SIZE) return false;
  const SolidInfo& info = SOLID_TABLE[index];
  switch (info.shape) {
    case SOLID_CYLINDER:
      m.curved = true;
      return solidBuildRound(m, 1.0f);
    case SOLID_CONE:
      m.curved = true;
      return solidBuildRound(m, 0.0f);
    case SOLID_SPHERE:
      m.curved = true;
      return solidBuildSphere(m);
    case SOLID_TORUS:
      m.curved = true;
      return solidBuildTorus(m);
    default:
      return solidBuildPolyhedron(info, m);
  }
}

// How bright a face is, from its turned normal. 0.3 is the light from everywhere, so a face
// turned away from the lamp stays visible. The lamp sits top left, in front of the screen.
inline float solidBrightness(float nx, float ny, float nz) {
  const float lx = -0.40825f, ly = 0.40825f, lz = 0.81650f;  // Unit length
  const float d = nx * lx + ny * ly + nz * lz;
  return 0.3f + 0.7f * (d > 0 ? d : 0);
}

// Scales an RGB565 colour by 0..1
inline uint16_t solidShade565(uint16_t color, float brightness) {
  if (brightness < 0) brightness = 0;
  if (brightness > 1) brightness = 1;
  const uint16_t r = (uint16_t)(((color >> 11) & 0x1F) * brightness + 0.5f);
  const uint16_t g = (uint16_t)(((color >> 5) & 0x3F) * brightness + 0.5f);
  const uint16_t b = (uint16_t)((color & 0x1F) * brightness + 0.5f);
  return (uint16_t)((r << 11) | (g << 5) | b);
}
