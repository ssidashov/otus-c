#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "utf8translator.h"
#include "utf8translator_spi.h"

#define REGISTRY_SIZE UINT8_MAX

static encoding_entry_t *encodings_registry[REGISTRY_SIZE];

void utf8t_register_encoding(utf8t_encoding_descriptor_t *descriptor) {
   descriptor->register_encoding_function(encodings_registry);
}

void utf8t_close() {
   for(size_t i = 0; i < REGISTRY_SIZE; i++) {
      encodings_registry[i] = NULL;
   }
}

bool utf8t_get_aval_enc_names(names_list_t *names, size_t *count) {
    size_t counter = 0;
    char* temp_array[REGISTRY_SIZE];
    for(size_t i = 0; i < REGISTRY_SIZE; i++) {
        encoding_entry_t *pentry = encodings_registry[i];
        if(pentry == NULL) continue;
        temp_array[counter++] = pentry->code;
    }
    char** list = malloc(sizeof(char*)*counter);
    if(list == NULL) {
        return false;
    }
    memcpy(list, temp_array, sizeof(char*) * counter);
    *names = list;
    *count = counter;
    return true;
}

bool utf8t_names_release(names_list_t* names) {
  if(names == NULL) {
    return false;
  }
  free(*names);
  return true;
}

bool utf8t_get_encoding_by_code(char *code, utf8t_encoding_id_t *encoding_id) {
  for(size_t i = 0; i < REGISTRY_SIZE; i++) {
    encoding_entry_t *pentry = encodings_registry[i];
    if(pentry == NULL) continue;
    if(strcmp(pentry->code, code) == 0) {
      *encoding_id = i;
      return true;
    }
  }
  return false;
}

size_t utf8t_get_bytes_in_char(utf8t_encoding_id_t encoding_id,
                               uint8_t start_byte) {
   encoding_entry_t* pencoding = encodings_registry[encoding_id];
   if(pencoding == NULL) return 0;
   return pencoding->get_codepoint_size_func(start_byte);
}

static bool translate_unicode_to_utf8(uint16_t unicode_codepoint,
        uint8_t buffer[MAX_BYTES_IN_UTF8_CODEPOINT],
        size_t *result_bytes_size) {
    if(unicode_codepoint <= 0x7F) {
        buffer[0] = (uint8_t)unicode_codepoint;
        *result_bytes_size = 1;
    } else if(unicode_codepoint <= 0x07FF) {
        buffer[0] = (uint8_t)(((unicode_codepoint >> 6) & 0x1F) | 0xC0);
        buffer[1] = (uint8_t)(((unicode_codepoint >> 0) & 0x3F) | 0x80);
        *result_bytes_size = 2;
    } else {
       buffer[0] = (uint8_t) (((unicode_codepoint >> 12) & 0x0F) | 0xE0);
       buffer[1] = (uint8_t) (((unicode_codepoint >>  6) & 0x3F) | 0x80);
       buffer[2] = (uint8_t) (((unicode_codepoint >>  0) & 0x3F) | 0x80);
       *result_bytes_size = 3;
    };
    return true;
}

bool utf8t_encode(utf8t_encoding_id_t encoding_id,
                    uint8_t source_char[],
                    size_t source_char_size_bytes,
                    uint8_t buffer[MAX_BYTES_IN_UTF8_CODEPOINT],
                    size_t *result_bytes_size) {
  encoding_entry_t* pencoding = encodings_registry[encoding_id];
  if(pencoding == NULL) return false;
  uint16_t unicode_codepoint_result = 0;
  bool result = pencoding->translate_func(source_char,
          source_char_size_bytes,
          &unicode_codepoint_result);
  if(!result) return false;
  return translate_unicode_to_utf8(unicode_codepoint_result, buffer, result_bytes_size);
}
