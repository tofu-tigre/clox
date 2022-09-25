#ifndef vm_h
#define vm_h

#include "object.h"
#include "table.h"
#include "value.h"
#include "stack.h"


typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
    Chunk *chunk;
    uint8_t *ip;
    Stack stack;
    Table strings;
    Table globals;
    Obj* objects;
} VM;

extern VM vm;

void init_vm();
void free_vm();
InterpretResult interpret(const char* source);
#endif