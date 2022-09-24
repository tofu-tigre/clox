#include "stack.h"
#include "memory.h"

void init_stack(Stack* stack) {
    stack->data = reallocate(NULL, 0, DEFAULT_STACK_SIZE * sizeof(Value));
    stack->top = stack->data;
    stack->size = DEFAULT_STACK_SIZE;
}


void push(Stack* stack, Value value) {
    int used = stack->top - stack->data;
    if(stack->size < used + 1) {
        stack->size *= STACK_GROWTH_FACTOR;
        stack->data = reallocate(stack->data, used * sizeof(Value), stack->size * sizeof(Value));
        stack->top = stack->data + used;
    }

    *(stack->top) = value;
    stack->top += 1;
}


Value pop(Stack* stack) {
    stack->top -= 1;
    return *(stack->top);
}

void free_stack(Stack* stack) {
    stack->data = reallocate(stack->data, stack->size * sizeof(Value), 0);
    stack->top = NULL;
}

Value peek(Stack* stack, int depth) {
    return stack->top[-1 - depth];
}