#include "hash_table.h"
#include <malloc.h>
#include <string.h>
#include <strings.h>

#define LOAD_FACTOR 0.5f
#define MULTIPLIER 2
#define LINEAR_STEP 1

struct hash_table {
  hash_table_entry **entries;
  size_t allocated_size;
  size_t size;
  hash_table_hash_fn hash_function;
  hash_table_key_equals_fn key_equals_function;
};

hash_table_entry deleted_entry;

hash_table *hash_table_init(hash_table_hash_fn hash_function,
                            hash_table_key_equals_fn key_equals_function,
                            size_t initial_size) {
  hash_table *created_table = malloc(sizeof(hash_table));
  if (created_table == NULL)
    return NULL;
  created_table->size = 0;
  created_table->allocated_size = initial_size;
  hash_table_entry **entry_array =
      malloc(sizeof(hash_table_entry *) * initial_size);
  if (entry_array == NULL && initial_size != 0) {
    free(created_table);
    return NULL;
  }
  memset(entry_array, 0,
         sizeof(hash_table_entry *) * created_table->allocated_size);
  created_table->entries = entry_array;
  created_table->hash_function = hash_function;
  created_table->key_equals_function = key_equals_function;
  return created_table;
}

int probe_add(hash_table *table, const void *key) {
  unsigned int hash = table->hash_function(key);
  size_t index = hash % table->allocated_size;
  size_t iteration_count = 0;
  while (table->entries[index] != NULL &&
         (table->entries[index] != &deleted_entry) &&
         !table->key_equals_function(table->entries[index]->key, key)) {
    index += LINEAR_STEP;
    if (index >= table->allocated_size) {
      index = 0;
    }
    if (++iteration_count > table->allocated_size) {
      return -3; // bad hashtable config - no free entries or bad search
                 // algorithm;
    }
  }
  return index;
}

int probe_get(hash_table *table, const void *key) {
  unsigned int hash = table->hash_function(key);
  size_t index = hash % table->allocated_size;
  while (table->entries[index] != NULL) {
    if (table->entries[index] == &deleted_entry ||
        !table->key_equals_function(table->entries[index]->key, key)) {
      index++;
      if (index >= table->allocated_size) {
        index = 0;
      }
    } else {
      return index;
    }
  }
  return -1;
}

int recalc_table(hash_table *table) {
  size_t new_allocation_size = table->allocated_size * MULTIPLIER;
  size_t old_allocation_size = table->allocated_size;
  hash_table_entry **old_entries = table->entries;
  hash_table_entry **new_entries =
      malloc(sizeof(hash_table_entry *) * new_allocation_size);
  if (new_entries == NULL) {
    return 2;
  }
  table->entries = new_entries;
  table->allocated_size = new_allocation_size;
  for (size_t idx = 0; idx < old_allocation_size; idx++) {
    if (old_entries[idx] == NULL || old_entries[idx] == &deleted_entry) {
      continue;
    }
    int founded_index = probe_add(table, old_entries[idx]->key);
    if (founded_index < 0) {
      table->entries = old_entries;
      table->allocated_size = old_allocation_size;
      return 3;
    }
    table->entries[founded_index] = old_entries[idx];
  }
  free(old_entries);
  return 0;
}

int hash_table_put(hash_table *table, const void *key, size_t key_size,
                   const void *value, size_t value_size, void **prev_value) {
  int index = probe_add(table, key);
  if (index < 0) {
    return index;
  }
  void *value_copy = malloc(value_size);
  if (value_copy == NULL) {
    return -1;
  }
  memcpy(value_copy, value, value_size);
  hash_table_entry *prev_entry = table->entries[index];
  if (prev_entry == NULL || prev_entry == &deleted_entry) {
    void *key_copy = malloc(key_size);
    if (key_copy == NULL) {
      return -1;
    }
    memcpy(key_copy, key, key_size);
    hash_table_entry *entry = malloc(sizeof(hash_table_entry));
    if (entry == NULL) {
      return -1;
    }
    entry->key = key_copy;
    entry->value = value_copy;
    table->entries[index] = entry;
    table->size++;
    if (prev_value != NULL) {
      *prev_value = NULL;
    }
    if (table->size >= table->allocated_size * LOAD_FACTOR) {
      int recal_ret_val = recalc_table(table);
      if (recal_ret_val != 0) {
        return recal_ret_val;
      }
    }
  } else {
    if (prev_value != NULL) {
      *prev_value = prev_entry->value;
    }
    prev_entry->value = value_copy;
  }
  return 0;
}

void *hash_table_get(hash_table *table, const void *key) {
  int index = probe_get(table, key);
  if (index < 0) {
    return NULL;
  }
  return table->entries[index]->value;
}

void hash_table_free(hash_table *table) {
  if (table == NULL) {
    return;
  }
  if (table->entries == NULL) {
    free(table);
    return;
  }
  for (size_t i; i < table->allocated_size; i++) {
    hash_table_entry *entry = table->entries[i];
    if (entry == NULL || entry == &deleted_entry) {
      continue;
    }
    free(entry->key);
    free(entry->value);
    free(entry);
  }
  free(table->entries);
  free(table);
}

size_t hash_table_get_size(hash_table *table) { return table->size; }

size_t hash_table_for_each_entry(hash_table *table,
                                 table_entry_consumer_fn consumer_function) {
  size_t counter = 0;
  for (size_t idx = 0; idx < table->allocated_size; idx++) {
    if (table->entries[idx] == NULL || table->entries[idx] == &deleted_entry) {
      continue;
    }
    consumer_function(table->entries[idx]->key, table->entries[idx]->value);
    counter++;
  }
  return counter;
}

bool hash_table_delete(hash_table *table, const void *key) {
  int index = probe_get(table, key);
  if (index < 0) {
    return false;
  }
  free(table->entries[index]->key);
  free(table->entries[index]->value);
  free(table->entries[index]);
  table->entries[index] = &deleted_entry;
  table->size--;
  return true;
}
