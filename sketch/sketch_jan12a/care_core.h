// What Zitro does when a child shares worries, sadness, conflict or pain: a short, warm answer that sends
// the child to a person it trusts, and for serious worries the children's helpline of its home country.
// The number comes from this table (picked by the home city's time zone), never from the AI's memory.
// Plain C++, tested on the host: tests/care/run.sh
#pragma once

#include <string.h>
#include <string>

struct CareHelpline {
  const char* iana;  // Home city's time zone, stands for the country
  const char* text;  // How the prompt names it
};

// Free children's helplines (checked 2026-11-15): Pro Juventute (CH), Rat auf Draht (AT), Nummer gegen Kummer (DE)
static const CareHelpline CARE_HELPLINES[] = {
  {"Europe/Zurich", "147 (Pro Juventute, gratis, rund um die Uhr)"},
  {"Europe/Vienna", "147 (Rat auf Draht, gratis, rund um die Uhr)"},
  {"Europe/Berlin", "116 111 (Nummer gegen Kummer, gratis)"},
};

inline const char* careHelpline(const char* iana) {
  if (!iana) return nullptr;
  for (const CareHelpline& h : CARE_HELPLINES) {
    if (strcmp(h.iana, iana) == 0) return h.text;
  }
  return nullptr;
}

// The rule for the main system prompt. iana: the home city's time zone ("Europe/Zurich").
inline std::string careRuleText(const char* iana) {
  std::string rule =
    "SORGEN: Erzaehlt das Kind von Sorgen, Traurigkeit, Angst, Einsamkeit, Streit, Ausgrenzung, Mobbing, "
    "Schmerzen oder davon, dass ihm jemand wehtut, antworte kurz, warm und ernst. Stelle keine Diagnose und "
    "behaupte nicht, Gefuehle zu haben oder ein Freund zu sein. Rate, mit einem Menschen zu reden, dem es "
    "vertraut, etwa Eltern, Lehrperson oder Schulsozialarbeit, bei Schmerzen mit den Eltern oder einer Aerztin. ";
  const char* helpline = careHelpline(iana);
  if (helpline) {
    rule += "Klingt es ernst (jemand tut ihm weh, es hat grosse Angst, will sich verletzen oder nicht mehr leben), "
            "nenne die Telefonnummer ";
    rule += helpline;
    rule += ", in akuter Gefahr den Notruf 112. ";
  } else {
    rule += "Klingt es ernst (jemand tut ihm weh, es hat grosse Angst, will sich verletzen oder nicht mehr leben), "
            "rate, sofort einen Erwachsenen zu holen. ";
  }
  rule += "Nenne in solchen Antworten keine anderen Telefonnummern. Setze dabei image_query leer und create_image auf false.";
  return rule;
}
