#ifndef UTF8T_UNICODE_TRANSBASE_H
#define UTF8T_UNICODE_TRANSBASE_H

#include "utf8translator.h"
#include <stdint.h>

bool utf8t_translate_by_onebyte_table(uint8_t source_character,
                                      uint16_t *result,
                                      const uint16_t translate_table[]);

#endif
