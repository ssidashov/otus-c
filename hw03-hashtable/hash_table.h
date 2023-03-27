#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>

struct hash_table;
typedef struct hash_table hash_table;
typedef struct {
  void *key;
  void *value;
} hash_table_entry;

typedef unsigned int (*hash_table_hash_fn)(const void *);
typedef bool (*hash_table_key_equals_fn)(const void *, const void *);
typedef void (*table_entry_consumer_fn)(const void *, const void *);

hash_table *hash_table_init(hash_table_hash_fn hash_function,
                            hash_table_key_equals_fn key_equals_function,
                            size_t initial_size);
int hash_table_put(hash_table *table, const void *key, size_t key_size,
                   const void *value, size_t value_size, void **prev_value);
void *hash_table_get(hash_table *table, const void *key);
bool hash_table_delete(hash_table *table, const void *key);
void hash_table_free(hash_table *table);
size_t hash_table_get_size(hash_table *table);
size_t hash_table_for_each_entry(hash_table *table,
                                 table_entry_consumer_fn consumer_function);

#endif
