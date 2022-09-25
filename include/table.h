#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

#define DEFAULT_TABLE_SIZE 8
#define TABLE_MAX_LOAD 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
void table_add_all(Table* from, Table* to);
bool table_set(Table* table, ObjString* key, Value value);
bool table_get(Table* table, ObjString* key, Value* value);
bool table_delete(Table* table, ObjString* key);
ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash);

#endif