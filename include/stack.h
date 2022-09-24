#ifndef STACK_H
#define STACK_H



#define STACK_MAX 0xFFFF
#define DEFAULT_STACK_SIZE 8
#define STACK_GROWTH_FACTOR 2

#include "value.h"

typedef struct Stack {
    Value *data;
    Value *top;
    int size;
} Stack;

void init_stack(Stack* stack);
void free_stack(Stack* stack);
void push(Stack* stack, Value value);
Value pop(Stack* stack);
Value peek(Stack *stack, int depth);

#endif