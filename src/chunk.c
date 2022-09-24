#include "chunk.h"


/* Initialize an empty chunk with a default size of
   INITIAL_CHUNK_SIZE. */
void init_chunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = INITIAL_CHUNK_SIZE;

    /* Initializes the chunk code array (and line array). */
    chunk->code = GROW_ARRAY(uint8_t, NULL, 0, INITIAL_CHUNK_SIZE);
    chunk->lines = GROW_ARRAY(size_t, NULL, 0, INITIAL_CHUNK_SIZE);
    init_value_array(&chunk->constants);
}

void free_chunk(Chunk* chunk) {
    chunk->code = FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    chunk->lines = FREE_ARRAY(size_t, chunk->lines, chunk->capacity);
    chunk->count = 0;
    chunk->capacity = 0;
    free_value_array(&chunk->constants);
}

void write_chunk(Chunk* chunk, uint8_t byte, int line) {
    if(chunk->capacity < chunk->count + 1) {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(size_t, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

size_t add_constant(Chunk *chunk, Value value) {
    write_value_array(&chunk->constants, value);
    return chunk->constants.count - 1; // index of constant in values
}

void write_constant(Chunk *chunk, Value value, int line) {
    // Add value to chunk's constant array
    // Write instruction to chunk to handle value size (CONSTANT / CONSTANT_LONG)

    uint16_t index = add_constant(chunk, value);
    uint8_t left_bits = (index & 0xFF00) >> 8;
    if(left_bits > 0) { // CONST_LONG
        write_chunk(chunk, OP_CONSTANT_LONG, line);
        write_chunk(chunk, left_bits, line);
    } else {
        write_chunk(chunk, OP_CONSTANT, line);
    }
    write_chunk(chunk, index & 0x00FF, line);
}

size_t get_line(Chunk* chunk, size_t offset) {
    return chunk->lines[offset];
}