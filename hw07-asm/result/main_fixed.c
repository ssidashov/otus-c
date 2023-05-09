#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>

#define ELEMENT_TYPE int64_t

const char *empty_str = "";
const char *int_format = "%ld ";
const ELEMENT_TYPE data[] = {4, 8, 15, 16, 23, 42};
const size_t data_length = sizeof(data) / sizeof(ELEMENT_TYPE);

struct list_element {
  ELEMENT_TYPE value;
  struct list_element *next;
};

//в rax остается результат выполнения fflush
int print_int(ELEMENT_TYPE value) {
  printf(int_format, value);
  return fflush(stdin); //xor rdi, rdi = 0 => fflush(0) => fflush(stdin), но зачем?
}

struct list_element *add_element(struct list_element *list, ELEMENT_TYPE value) {
  struct list_element *new_element = malloc(sizeof(struct list_element));
  if(new_element == NULL) {
    abort();
  }
  new_element->value = value;
  new_element->next = list;
  return new_element;
}

// p
bool is_odd(ELEMENT_TYPE element) {
  return element % 2 == 1;
}

// Теоретически может возвращать через rax результат вызова mapper, но в случае
// с list==0 значение не передеается, поэтому void
//m - аналог map с функтором возвращающим void
void for_each(struct list_element *list, int (*mapper)(ELEMENT_TYPE)) {
  if(list == NULL) {
    return;
  }
  mapper(list->value);
  for_each(list->next, mapper);
}

//f - аналог filter
struct list_element *filter(struct list_element *list, struct list_element *result, bool (*predicate)(ELEMENT_TYPE)) {
  if (list == NULL) {
    return result;
  }
  bool check_res = predicate(list->value);
  struct list_element *the_result;
  if(check_res) {
    the_result = add_element(result, list->value);
  } else{
    the_result = result;
  }

  return filter(
      list->next,
      the_result,
      predicate);
}

struct list_element *list_head_delete(struct list_element *list, int count) {
  if(count == 0 || list == NULL) {
    return list;
  }

  struct list_element *next = list->next;
  free(list);
  return list_head_delete(next, count - 1);
}

int main(int argc, char **argv) {
  struct list_element *list = NULL;
  for(size_t i = data_length; i != 0; i--) {
    list = add_element(list, data[i - 1]);
  };
  for_each(list, print_int);
  puts(empty_str);
  struct list_element *filtered = filter(list, NULL, is_odd);
  for_each(filtered, print_int);
  puts(empty_str);
  list_head_delete(filtered, -1);
  list_head_delete(list, -1);
  return 0;
}
