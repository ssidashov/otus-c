#ifndef UTF8T_UTF8TRANSLATOR_H
#define UTF8T_UTF8TRANSLATOR_H

#define MAX_BYTES_IN_UTF8_CODEPOINT 4
#define MAX_BYTES_IN_CODEPAGES 4

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pulic API and SPI of converter
 */

typedef uint8_t utf8t_encoding_id_t;

typedef struct encoding_entry_t encoding_entry_t;

typedef char **names_list_t;

typedef struct utf8t_encoding_descriptor_t utf8t_encoding_descriptor_t;

void utf8t_register_encoding(utf8t_encoding_descriptor_t *);
void utf8t_close();
bool utf8t_get_aval_enc_names(names_list_t *names, size_t *count);
bool utf8t_names_release(names_list_t *names);
bool utf8t_get_encoding_by_code(char *code, utf8t_encoding_id_t *encoding_id);

size_t utf8t_get_bytes_in_char(utf8t_encoding_id_t encoding_id,
                               uint8_t start_byte);
bool utf8t_encode(utf8t_encoding_id_t encoding_id, uint8_t source_char[],
                  size_t source_char_size_bytes,
                  uint8_t buffer[MAX_BYTES_IN_UTF8_CODEPOINT],
                  size_t *result_bytes_size);

#endif
