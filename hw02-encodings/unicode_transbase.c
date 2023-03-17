#include "unicode_transbase.h"

bool utf8t_translate_by_onebyte_table(uint8_t source_character,
                                      uint16_t *result,
                                      const uint16_t translate_table[]) {
  if (source_character < 0x80) {
    *result = source_character;
    return true;
  }
  uint8_t index = source_character - 0x80;
  *result = translate_table[index];
  return true;
}
