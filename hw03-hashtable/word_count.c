#include <ctype.h>
#include <locale.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hash_table.h"
#include "str_normalize.h"

#define BUFFER_SIZE 1000
#define INITIAL_WORD_BUFFER_SIZE 100
#define INITIAL_TABLE_SIZE 1

#define HASH_SEED 42

size_t word_counter = 0;

hash_table *words_hash_table = NULL;

void print_usage(const char *app_name) {
  printf("Usage: %s [in_file]\n", app_name);
}

void print_word_stat(const void *word, const void *count) {
  char *the_word = (char *)word;
  size_t *the_count = (size_t *)count;
  printf("%s: %zu\n", the_word, *the_count);
}

void print_word_stats(void) {
  printf("Total word count: %zu\n", hash_table_get_size(words_hash_table));
  printf("\n");
  hash_table_for_each_entry(words_hash_table, print_word_stat);
}

unsigned int word_hash(const void *data) {
  const char *word = data;
  unsigned int hash = HASH_SEED;
  while (*word) {
    hash = hash * 101 + *word++;
  }
  return hash;
}

bool word_equals(const void *key1, const void *key2) {
  const char *word1 = key1;
  const char *word2 = key2;
  return strcmp(word1, word2) == 0;
}

int add_and_increment_word(const char *word) {
  size_t *count = hash_table_get(words_hash_table, word);
  if (count == NULL) {
    size_t count_val = 1;
    int put_res = hash_table_put(words_hash_table, word,
                                 (strlen(word) + 1) * sizeof(char), &count_val,
                                 sizeof(size_t), NULL);
    if (put_res != 0) {
      return put_res;
    }
  } else {
    (*count)++;
  }
  return 0;
}

int process_word(char *word, size_t counter) {
  int ret_val = 0;
  char *normalized_string = str_normalize(word);
  if (normalized_string == NULL) {
    fprintf(stderr, "Cannot normalize word '%s'", word);
    ret_val = -1;
    goto resources_release;
  }
  int increment_res = add_and_increment_word(normalized_string);
  if (increment_res != 0) {
    fprintf(stderr, "Cannot count word #%zu %s, code: %d", counter, word,
            increment_res);
    ret_val = -2;
    goto resources_release;
  };
resources_release:
  free(normalized_string);
  return ret_val;
}

int tokenize_by_words(FILE *infile, int (*word_processor_fn)(char *, size_t)) {
  int ret_val = 0;
  size_t bytes_read = 0;
  size_t bytes = 0;
  size_t words = 0;
  char buf[BUFFER_SIZE + 1];
  size_t current_word_buffer_size = INITIAL_WORD_BUFFER_SIZE;
  size_t current_word_size = 0;
  size_t buf_word_start_pos = 0;
  char *current_word_buffer = malloc(sizeof(char) * INITIAL_WORD_BUFFER_SIZE);
  if (current_word_buffer == NULL) {
    return 2;
  }
  while (!feof(infile)) {
    bytes_read = fread(buf, sizeof(uint8_t), BUFFER_SIZE, infile);
    if (ferror(infile)) {
      perror("Error reading file");
      ret_val = -1;
      goto resources_release;
    }
    bytes += bytes_read;
    size_t buffer_index = 0;
    buf_word_start_pos = 0;
    bool last_symbol_is_delimiter = false;
    while (buffer_index < bytes_read) {
      while (buffer_index < bytes_read &&
             !(last_symbol_is_delimiter = (ispunct(buf[buffer_index]) ||
                                           isspace(buf[buffer_index])))) {
        buffer_index++;
      }
      size_t bytes_to_add_to_word = buffer_index - buf_word_start_pos;
      if (bytes_to_add_to_word > 0) {
        if (current_word_size + bytes_to_add_to_word + 1 >
            current_word_buffer_size) {
          char *new_current_word_buffer =
              realloc(current_word_buffer,
                      bytes_to_add_to_word * 2 + current_word_buffer_size + 1);
          if (new_current_word_buffer == NULL) {
            ret_val = -2;
            goto resources_release;
          }
          current_word_buffer = new_current_word_buffer;
        }
        strncpy(current_word_buffer + current_word_size,
                buf + buf_word_start_pos, bytes_to_add_to_word);
        current_word_size += bytes_to_add_to_word;
      }
      if (buffer_index == bytes_read && !last_symbol_is_delimiter) {
        continue; // Прочитали весь буфер но конец слова не нашли
      }
      // Нашли конец слова
      if (current_word_size > 0) {
        current_word_buffer[current_word_size] = '\0';
        words++;
        int process_res = word_processor_fn(current_word_buffer, words);
        if (process_res != 0) {
          ret_val = -3;
          goto resources_release;
        }
      }
      buf_word_start_pos = buffer_index + 1;
      current_word_size = 0;
      buffer_index++;
    }
  }
  if (current_word_size > 0) {
    current_word_buffer[current_word_size] = '\0';
    word_processor_fn(current_word_buffer, words);
    words++;
  }
  printf("\n");
  printf("Bytes read: %zu\n", bytes);
  printf("Words recognized: %zu\n", words);
resources_release:
  free(current_word_buffer);
  return ret_val;
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  int ret_val = 0;
  char *infile_name = argc < 2 ? NULL : argv[1];
  bool in_file_is_stdin = argc < 2 || strcmp(argv[1], "-") == 0;
  FILE *infile = NULL;
  if (argc > 1 && strcmp(argv[1], "-h") == 0) {
    print_usage(argv[0]);
    goto release_resources;
  }

  if (in_file_is_stdin) {
    infile = stdin;
  } else {
    infile = fopen(infile_name, "rb");
  }
  if (infile == NULL) {
    fprintf(stderr, "Cannot open file %s: ",
            infile_name == NULL ? "STDIN" : infile_name);
    perror(NULL);
    ret_val = 1;
    goto release_resources;
  }
  words_hash_table =
      hash_table_init(word_hash, word_equals, INITIAL_TABLE_SIZE);
  if (words_hash_table == NULL) {
    fprintf(stderr, "Error creating hash table\n");
    ret_val = 3;
    goto release_resources;
  }
  if (tokenize_by_words(infile, process_word) != 0) {
    fprintf(stderr, "Error founding word sizes in file");
    ret_val = 2;
    goto release_resources;
  };
  print_word_stats();
release_resources:
  if (infile != NULL && !in_file_is_stdin && fclose(infile) < 0) {
    fprintf(stderr, "Error closing in file %s", infile_name);
  }
  if (words_hash_table != NULL) {
    hash_table_free(words_hash_table);
  }
  return ret_val;
}
