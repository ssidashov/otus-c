#ifndef UTF8_TRANSLATOR_SPI_H
#define UTF8_TRANSLATOR_SPI_H

#include "utf8translator.h"

typedef size_t (*utf8t_enc_codepoint_size_fn)(uint8_t);
typedef bool (*utf8t_encode_res_reciever_fn)(uint8_t **codepoint_data,
                                             size_t *count);
typedef bool (*utf8t_translate_character_fn)(uint8_t source_char[],
                                             size_t source_char_size,
                                             uint16_t *result_codepoint);
typedef bool (*utf8t_register_encoding_fn)(
    encoding_entry_t *encoding_registry[]);

struct encoding_entry_t {
  utf8t_encoding_id_t id;
  char *code;
  utf8t_enc_codepoint_size_fn get_codepoint_size_func;
  utf8t_translate_character_fn translate_func;
};

struct utf8t_encoding_descriptor_t {
  utf8t_register_encoding_fn register_encoding_function;
};

#endif
