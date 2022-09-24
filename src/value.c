#include <stdio.h>
#include "value.h"
#include "string.h"
#include "object.h"
#include "memory.h"



void init_value_array(ValueArray *array) {
    array->count = 0;
    array->capacity = INITIAL_VAL_ARRAY_SIZE;
    array->values = GROW_ARRAY(Value, NULL, 0, INITIAL_VAL_ARRAY_SIZE);
}

void free_value_array(ValueArray *array) {
    array->values = FREE_ARRAY(Value, array->values, array->capacity);
    array->count = 0;
    array->capacity = 0;
}

void write_value_array(ValueArray *array, Value value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void print_value(Value value) {
    switch(value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJ:
            print_object(value);
            break;
    }
    
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type)
        return false;
    
    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: {
            ObjString* a_obj = AS_STRING(a);
            ObjString* b_obj = AS_STRING(b);
            return a_obj->length == b_obj->length &&
                memcmp(a_obj->chars, b_obj->chars, a_obj->length) == 0;
        }
        default:
            return false;
    }
}

bool is_falsey(Value value) {
    return IS_NIL(value) || 
        (IS_BOOL(value) && !AS_BOOL(value)) ||
        (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}