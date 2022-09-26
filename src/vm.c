#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm;
static uint8_t read_byte();
static Value read_constant();
static Value read_constant_long();
static InterpretResult run();
static void runtime_error(const char* format, ...);
static void concatenate();
static void string_multiply();


#define READ_SHORT() \
    (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))

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

#define READ_CONSTANT() (vm.chunk->constants.values[read_byte()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

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

            case OP_ADD: {
                Value b = peek(&vm.stack, 0);
                Value a = peek(&vm.stack, 1);

                if (IS_STRING(a) && IS_STRING(b)) {
                    concatenate();
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    double b_num = AS_NUMBER(pop(&vm.stack));
                    double a_num = AS_NUMBER(pop(&vm.stack));
                    push(&vm.stack, NUMBER_VAL(a_num + b_num));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: {
                Value b = peek(&vm.stack, 0);
                Value a = peek(&vm.stack, 1);

                if (IS_STRING(a) && IS_NUMBER(b)) {
                    string_multiply();
                } else if (IS_NUMBER(a) && IS_STRING(b)) {
                    string_multiply();
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    double b_num = AS_NUMBER(pop(&vm.stack));
                    double a_num = AS_NUMBER(pop(&vm.stack));
                    push(&vm.stack, NUMBER_VAL(a_num * b_num));
                } else {
                    runtime_error("Operands must be two numbers or a string and a number.");
                }
                break;
            }
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;

            case OP_NIL: {
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
            case OP_POP: {
                pop(&vm.stack);
                break;
            }
            case OP_PRINT: {
                print_value(pop(&vm.stack));
                printf("\n");
                break;
            }
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, peek(&vm.stack, 0));
                pop(&vm.stack);
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(&vm.stack, value);
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, peek(&vm.stack, 0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = read_byte();
                vm.stack.data[slot] = peek(&vm.stack, 0);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = read_byte();
                push(&vm.stack, vm.stack.data[slot]);
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(&vm.stack, 0)))
                    vm.ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                vm.ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                vm.ip -= offset;
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

#undef READ_STRING
#undef READ_CONSTANT
#undef READ_SHORT
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
    init_table(&vm.strings);
    init_table(&vm.globals);
    vm.chunk = NULL;
    vm.objects = NULL;
}


void free_vm() {
    free_stack(&vm.stack);
    free_table(&vm.strings);
    free_table(&vm.globals);
    free_objects();
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

static void concatenate() {
    ObjString* b = AS_STRING(pop(&vm.stack));
    ObjString* a = AS_STRING(pop(&vm.stack));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    push(&vm.stack, OBJ_VAL(result));
}

static void string_multiply() {
    double b_num;
    ObjString* a_str;
    Value b_val = pop(&vm.stack);
    Value a_val = pop(&vm.stack);
    if(IS_NUMBER(b_val)) {
        b_num = AS_NUMBER(b_val);
        a_str = AS_STRING(a_val);
    } else {
        b_num = AS_NUMBER(a_val);
        a_str = AS_STRING(b_val);
    }

    int length = b_num * a_str->length;
    char* chars = ALLOCATE(char, length + 1);
    int i = 0;
    for (i = 0; i < b_num; i++) {
        memcpy(chars + (a_str->length * i), a_str->chars, a_str->length);
    }
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    push(&vm.stack, OBJ_VAL(result));
}