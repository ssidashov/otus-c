#include "str_normalize.h"
#include <string.h>

#if ENABLE_UNICODE_USING_LOCALE
#include <malloc.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

char *str_normalize(char *string) {
  char *ret_val = NULL;
  size_t wordlen = strlen(string);
  size_t wstring_size = sizeof(wchar_t) * (wordlen + 1);
  wchar_t *wide_string = malloc(wstring_size * sizeof(wchar_t));
  if (wide_string == NULL) {
    goto release_resources;
  }
  int wcoper_res_code = mbstowcs(wide_string, string, wstring_size);
  if (wcoper_res_code == -1) {
    perror("mbstowcs");
    goto release_resources;
  }
  for (size_t i = 0; i < wcslen(wide_string); i++) {
    wide_string[i] = towlower(wide_string[i]);
  }
  char *result_string = malloc(wstring_size);
  if (result_string == NULL) {
    goto release_resources;
  }
  wcoper_res_code = wcstombs(result_string, wide_string, wstring_size);
  if (wcoper_res_code == -1) {
    perror("wcstombs");
    goto release_resources;
  }
  ret_val = result_string;
release_resources:
  if (ret_val == NULL) {
    free(result_string);
  }
  free(wide_string);
  return ret_val;
}

#else

#include <malloc.h>

char *str_normalize(char *string) {
  char *copy = malloc(sizeof(char) * strlen(string) + 1);
  if (copy == NULL) {
    return NULL;
  }
  strcpy(copy, string);
  return copy;
}

#endif
