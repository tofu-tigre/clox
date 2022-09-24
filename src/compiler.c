#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

static Parser parser;

static void advance();
static void expression();
static void error_at_current(const char* message);
static void error(const char* message);
static void error_at(Token* token, const char* message);
static void consume(TokenType token, const char* message);
static void end_compiler();
static void emit_byte(uint8_t byte);
static Chunk* current_chunk();
static void emit_return();
static void emit_constant(Value value);
static uint16_t make_constant(Value value);
static void number();
static void grouping();
static void binary();
static void unary();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static void literal();

ParseRule rules[] = {
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

/* Compiler. Takes the scanned tokens from the scanner
 * and interprets their symbols into bytecode.
 */
bool compile(const char* source, Chunk* chunk) {
    init_scanner(source);
    parser.compiling_chunk = chunk;
    parser.had_error = parser.panic_mode = false;
    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    end_compiler();
    return !parser.had_error;
}


static void consume(TokenType type, const char* message) {
    if(parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}


static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if(prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    prefix_rule();
    while(precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}


static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}


static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}


static void binary() {
    TokenType operator_type = parser.previous.type;
    ParseRule* rule = get_rule(operator_type);
    parse_precedence((Precedence) (rule->precedence + 1));

    switch(operator_type) {
        case TOKEN_PLUS:            emit_byte(OP_ADD); break;
        case TOKEN_MINUS:           emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emit_byte(OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL:      emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:     emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER:         emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:   emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:            emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:      emit_bytes(OP_GREATER, OP_NOT); break;
        default: return;
    }
}


static void unary() {
    TokenType operator_type = parser.previous.type;

    /* Compile the operand. */
    parse_precedence(PREC_UNARY);

    switch(operator_type) {
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        default: return;
    }
}


static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}


static void emit_byte(uint8_t byte) {
    write_chunk(current_chunk(), byte, parser.previous.line);
}


static void end_compiler() {
    emit_return();
    #ifdef DEBUG_PRINT_CODE
    if(!parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    } else {
        fprintf(stderr, "Error: Could not disassemble chunk due to error.\n");
    }
    #endif
}


static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}


static void emit_constant(Value value) {
    uint16_t index = make_constant(value);
    if(index > UINT8_MAX) {
        /* Write 16-bit index. */
        uint8_t left_bits = (index & 0xFF00) >> 8;
        emit_bytes(OP_CONSTANT_LONG, left_bits);
        emit_byte((uint8_t) (index & 0x00FF));
    } else {
        emit_bytes(OP_CONSTANT, (uint8_t) (index & 0x00FF));
    }
    
}


static uint16_t make_constant(Value value) {
    int constant = add_constant(current_chunk(), value);
    if(constant > UINT16_MAX) {
        error("Too many constants in one chunk.");
    }

    return (uint16_t) constant;
}

static void emit_return() {
    emit_byte(OP_RETURN);
}


static Chunk* current_chunk() {
    return parser.compiling_chunk;
}


static void advance() {
    parser.previous = parser.current;

    for(;;) {
        parser.current = scan_token();
        if(parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}


static void error_at_current(const char* message) {
    error_at(&parser.current, message);
}


static void error(const char* message) {
    error_at(&parser.previous, message);
}


static void error_at(Token* token, const char* message) {
    if(parser.panic_mode) return;
    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if(token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if(token->type == TOKEN_ERROR) {
        // Do nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}


static void literal() {
    switch(parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL:  emit_byte(OP_NULL);  break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE);  break;
        default: return;
    }
}