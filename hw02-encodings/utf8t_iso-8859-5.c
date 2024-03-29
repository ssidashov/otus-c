#include "utf8t_iso-8859-5.h"
#include "unicode_transbase.h"

static const uint16_t iso8859_5_unicode_codepoint_by_char_position[] = {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088,
    0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F, 0x0090, 0x0091,
    0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A,
    0x009B, 0x009C, 0x009D, 0x009E, 0x009F, 0x00A0, 0x0401, 0x0402, 0x0403,
    0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409, 0x040A, 0x040B, 0x040C,
    0x00AD, 0x040E, 0x040F, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415,
    0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
    0x041F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F, 0x0430,
    0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439,
    0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442,
    0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B,
    0x044C, 0x044D, 0x044E, 0x044F, 0x2116, 0x0451, 0x0452, 0x0453, 0x0454,
    0x0455, 0x0456, 0x0457, 0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x00A7,
    0x045E, 0x045F,

};

static size_t get_iso8859_5_char_size(uint8_t first_symbol) {
  return sizeof(first_symbol);
}

static bool
translate_iso8859_5_char_to_unicode_codepoint(uint8_t *source_codepoint,
                                              size_t codepoint_size,
                                              uint16_t *result_codepoint) {
  if (codepoint_size != 1) {
    return false;
  }
  return utf8t_translate_by_onebyte_table(
      source_codepoint[0], result_codepoint,
      iso8859_5_unicode_codepoint_by_char_position);
}

static encoding_entry_t iso8859_5_encoding = {
    .id = UTF8T_ISO_8859_5_ENCODING_ID,
    .code = "iso-8859-5",
    .get_codepoint_size_func = get_iso8859_5_char_size,
    .translate_func = translate_iso8859_5_char_to_unicode_codepoint};

static bool register_iso8859_5(encoding_entry_t *encoding_registry[]) {
  encoding_registry[UTF8T_ISO_8859_5_ENCODING_ID] = &iso8859_5_encoding;
  return true;
}

static utf8t_encoding_descriptor_t UTF8T_ISO_8859_5_DESCRIPTOR = {
    .register_encoding_function = register_iso8859_5};

utf8t_encoding_descriptor_t *utf8t_encoding_get_iso8859_5() {
  return &UTF8T_ISO_8859_5_DESCRIPTOR;
}
