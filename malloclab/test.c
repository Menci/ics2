#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>
#include <time.h>

#include "block.h"

void *malloc8() {
    char *p = malloc(1024);
    unsigned x = (unsigned)p;
    if (x & 0x111) x = (x & ~0x111) + 0x1000;
    return (void *)x;
}

void *malloc4() {
    return (void *)(4 + (char *)malloc8());
}

void print_binary_25(uint32_t x) {
    for (int i = 24; i >= 0; i--) putchar('0' + !!(x & (1 << i)));
    putchar('\n');
}

block_t block;

uint32_t rand23() {
    struct {
        uint32_t x : 23;
    } s;
    s.x = rand();
    return ((uint32_t)s.x) << 2;
}

void test_0() {
    static uint32_t value = 1;
    if (value != 1) assert(read_block_header_integer(block, 1) == value);
    value = rand23();
    write_block_header_integer(block, 1, value);
}

void test_1() {
    static uint32_t value = 1;
    if (value != 1) assert(read_block_header_integer(block, 2) == value);
    value = rand23();
    write_block_header_integer(block, 2, value);
}

void test_2() {
    static uint32_t value = 1;
    if (value != 1) assert(read_block_header_integer(block, 3) == value);
    value = rand23();
    write_block_header_integer(block, 3, value);
    assert(read_block_header_integer(block, 3) == value);
}

void test_3() {
    static uint32_t value = 2;
    if (value != 2) assert(read_block_header_bit(block, 1) == value);
    value = rand() & 1;
    write_block_header_bit(block, 1, value);
}

void test_4() {
    static uint32_t value = 2;
    if (value != 2) assert(read_block_header_bit(block, 2) == value);
    value = rand() & 1;
    write_block_header_bit(block, 2, value);
}

inline void set_block_is_free(block_t block, bool is_free) {
    if (get_block_is_free(block) == is_free) return;

    size_t second_part_length = get_block_type(block) == BLOCK_8_ALIGNED ? 4 : 8;

    size_t length = get_block_length(block);
    // Free blocks have footer while allocated blocks don't have.
    size_t current_footer_length = is_free ? 0 : 3;
    size_t target_footer_length = is_free ? 3 : 0;
    uint8_t *block_end = block + length;
    memmove(
        block_end - target_footer_length - second_part_length,
        block_end - current_footer_length - second_part_length,
        second_part_length
    );
    set_block_is_free_flag_only(block, is_free);

    // block_t next_block = block + length;
    // set_previous_is_free(next_block, is_free);

    if (is_free) set_block_length_footer(block, length);
}

int main() {
    srand(time(NULL));
    block = malloc4();
    init_block_header_type_bit(block);

    void *payload = get_block_payload(block);

    assert(get_payload_block(payload) == block);

    set_block_length(block, 128);
    set_block_is_free(block, 0);
    // write_block_header_integer(block, 1, 0b1010101010011101110101000);
    // write_block_header_integer(block, 2, 0b1010101010011101110101000);
    // write_block_header_integer(block, 3, 0b1010101010011101110101000);
    // write_block_header_bit(block, 1, true);
    // write_block_header_bit(block, 2, true);

    // printf("%u\n", (unsigned)get_block_length(block));
    // print_binary_25(read_block_header_integer(block, 1));
    // print_binary_25(read_block_header_integer(block, 2));
    // print_binary_25(read_block_header_integer(block, 3));

    unsigned i = 0;
    while (1) {
        switch (rand() % 7) {
        case 0: test_0(); break;
        case 1: test_1(); break;
        case 2: test_2(); break;
        case 3: test_3(); break;
        case 4: test_4(); break;
        case 5: assert(get_payload_block(payload) == block); break;
        case 6: set_block_is_free(block, rand() & 1); break;
        default: break;
        }

        if (++i % 100000 == 0) printf("%u\n", i);
    }

    // printf("%u\n", (unsigned)get_block_is_free(block));
    // printf("%u\n", (unsigned)read_block_header_bit(block, 1));
    // printf("%u\n", (unsigned)read_block_header_bit(block, 2));

    // uint8_t *p = get_block_header_second_part_offseted(block);
    // printf("%u\n", (unsigned)(&p[12] - (uint8_t *)block));
}
