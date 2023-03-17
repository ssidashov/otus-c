#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "utf8translator.h"
#include "utf8t_cp1251.h"

void register_encodings();
void print_usage(const char* app_name);
void show_avaliable_encodings();

bool encode_file(FILE *infile, FILE *outfile, utf8t_encoding_id_t encoding) {
  size_t current_byte_counter = 0;
  //size_t bytes_written = 0;
  uint8_t first_byte_in_char;
  while(1) {
    size_t readed = fread(&first_byte_in_char, sizeof(first_byte_in_char), 1, infile);
    if(ferror(infile)) {
      perror("Error reading file");
      return false;
    }
    if(readed == 0) {
      break;
    }
    size_t char_size = utf8t_get_bytes_in_char(encoding, first_byte_in_char);
    if (char_size == 0) {
      fprintf(stderr, "Cannot find char size at position %zu", current_byte_counter);
      return false;
    }
    uint8_t read_buffer[MAX_BYTES_IN_CODEPAGES];
    read_buffer[0] = first_byte_in_char;
    size_t more_to_read_in_char = char_size - 1;
    if(more_to_read_in_char != 0) {
      readed = fread(read_buffer + 1, sizeof(uint8_t), more_to_read_in_char, infile);
      if(ferror(infile)) {
        perror("Error reading file");
        return false;
      }
      if(readed == 0) {
        break;
      }
    }
    uint8_t write_buffer[MAX_BYTES_IN_UTF8_CODEPOINT];
    size_t result_codepoint_size;
    bool result = utf8t_encode(encoding, read_buffer, char_size, write_buffer, &result_codepoint_size);
    if(!result) {
      fprintf(stderr, "Cannot encode character at position %zu with size %zu", current_byte_counter, char_size);
      return false;
    }
    size_t written_bytes = fwrite(write_buffer, sizeof(uint8_t), result_codepoint_size, outfile);
    if(written_bytes != result_codepoint_size || ferror(outfile)) {
      perror("Error writing file");
      return false;
    }
    current_byte_counter += char_size;
    //bytes_written += result_codepoint_size;
  }
  return true;
}

int main(int argc, char **argv) {
  register_encodings();
  int ret_val = 0;
  char *infile_name = argc < 3 ? NULL : argv[2];
  bool in_file_is_stdin = argc < 3 || strcmp(argv[2], "-") == 0;
  FILE *infile = NULL;
  char *outfile_name = argc < 4 ? NULL : argv[3];
  bool out_file_is_stdout = argc < 4 || strcmp(argv[3], "-") == 0;
  FILE *outfile = NULL;
  if (argc > 1 && strcmp(argv[1], "-h") == 0) {
    print_usage(argv[0]);
    goto release_resources;
  }

  if(argc < 2) {
    fprintf(stderr, "Encoding must be specified!");
    print_usage(argv[0]);
    ret_val = 1;
    goto release_resources;
  }

  utf8t_encoding_id_t selected_encoding_id;
  char* specified_encoding_code = argv[1];
  bool call_result = utf8t_get_encoding_by_code(specified_encoding_code, &selected_encoding_id);
  if(!call_result) {
    fprintf(stderr, "Cannot find encoding %s", specified_encoding_code);
    show_avaliable_encodings();
    ret_val = 2;
    goto release_resources;
  }

  if(in_file_is_stdin) {
    infile = freopen(NULL, "rb", stdin);
  } else {
    infile = fopen(infile_name, "rb");
  }
  if (infile == NULL) {
    fprintf(stderr, "Cannot open file %s: ", infile_name == NULL ? "STDIN" : infile_name);
    perror(NULL);
    ret_val = 3;
    goto release_resources;
  }

  if(out_file_is_stdout) {
    outfile = stdout;//freopen(NULL, "rb", stdin);
  } else {
    outfile = fopen(outfile_name, "wb");
  }
  if (outfile == NULL) {
    fprintf(stderr, "Cannot open file %s: ", outfile_name == NULL ? "STDOUT" : outfile_name);
    perror(NULL);
    ret_val = 3;
    goto release_resources;
  }

  bool endcode_result = encode_file(infile, outfile, selected_encoding_id);
  if(!endcode_result) {
    ret_val = 4;
    goto release_resources;
  }
release_resources:
  if (infile != NULL && !in_file_is_stdin && fclose(infile) < 0) {
    fprintf(stderr, "Error closing in file %s", infile_name);
  }
  if(outfile != NULL && !out_file_is_stdout && fclose(outfile) < 0) {
    fprintf(stderr, "Error closing out file %s", outfile_name);
  }
  utf8t_close();
  return ret_val;
}

void register_encodings() {
    utf8t_register_encoding(utf8t_encoding_get_cp1251());
}

void show_avaliable_encodings() {
  names_list_t names;
  size_t names_size = 0;
  bool result = utf8t_get_aval_enc_names(&names, &names_size);
  if(!result) {
    return;
  }
  printf("Avaliable encodings:\n");
  for(size_t i = 0; i < names_size; i++) {
    printf("%s", names[i]);
  }
  utf8t_names_release(&names);
}

void print_usage(const char *app_name) {
  printf("Usage: %s <from_encoding> [in_file] [out_file]\n", app_name);
  show_avaliable_encodings();
}
