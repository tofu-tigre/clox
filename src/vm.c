#include <stdarg.h>
#include <stdio.h>
#include "vm.h"
#include "value.h"
#include "compiler.h"

static VM vm;
static uint8_t read_byte();
static Value read_constant();
static Value read_constant_long();
static InterpretResult run();
static void runtime_error(const char* format, ...);



#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(&vm.stack, 0)) || !IS_NUMBER(peek(&vm.stack, 1))) { \
        runtime_error("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop(&vm.stack)); \
      double a = AS_NUMBER(pop(&vm.stack)); \
      push(&vm.stack, valueType(a op b)); \
    } while (false)


/* Starts up the virtual machine.
 * First it creates a chunk and then writes bytecode
 * to the chunk from the source file using the 
 * scanner and parser. Finally, it passes the bytecode
 * chunk into the virtual machine for interpretation.
 */
InterpretResult interpret(const char* source) {
    Chunk chunk;
    init_chunk(&chunk);
    if(!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }
    
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();
    free_chunk(&chunk);
    return result;
}


/* The heart of the virtual machine.
 * Reads the instruction byte code byte-by-byte
 * and evaluates using a stack.
 */
static InterpretResult run() {
    #ifdef DEBUG_TRACE_EXECUTION
        printf("\n===== stack trace =====");
    #endif
    for(;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("    ");
        for(Value *slot = vm.stack.data; slot < vm.stack.top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
        #endif

        uint8_t instruction;
        switch(instruction = read_byte()) {
            case OP_CONSTANT: {
                Value constant = read_constant();
                push(&vm.stack, constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value constant = read_constant_long();
                push(&vm.stack, constant);
                break;
            }
            case OP_NEGATE: {
                if(!IS_NUMBER(peek(&vm.stack, 0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(&vm.stack, NUMBER_VAL(-AS_NUMBER(pop(&vm.stack))));
                break;
            }

            case OP_ADD: BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;

            case OP_NULL: {
                push(&vm.stack, NIL_VAL);
                break;
            }
            case OP_TRUE: {
                push(&vm.stack, BOOL_VAL(true));
                break;
            }
            case OP_FALSE: {
                push(&vm.stack, BOOL_VAL(false));
                break;
            }
            case OP_NOT: {
                push(&vm.stack, BOOL_VAL(is_falsey(pop(&vm.stack))));
                break;
            }
            case OP_EQUAL: {
                Value b = pop(&vm.stack);
                Value a = pop(&vm.stack);
                push(&vm.stack, BOOL_VAL(values_equal(a, b)));
                break;
            }
            case OP_RETURN: {
                print_value(pop(&vm.stack));
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }
}


static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = get_line(vm.chunk, instruction);
    fprintf(stderr, "[line %d] in script\n", line);
    free_stack(&vm.stack);
    init_stack(&vm.stack);
}

void init_vm() {
    init_stack(&vm.stack);
    vm.chunk = NULL;
}


void free_vm() {
    free_stack(&vm.stack);
    if(vm.chunk == NULL) return;
    free_chunk(vm.chunk);
}


static uint8_t read_byte() {
    return *vm.ip++;
}


static Value read_constant() {
    uint8_t index = read_byte();
    return vm.chunk->constants.values[index];
}


static Value read_constant_long() {
    uint16_t index = (read_byte() << 8) | read_byte();
    return vm.chunk->constants.values[index];
}