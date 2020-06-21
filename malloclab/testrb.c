#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <assert.h>
#include <time.h>

void *mem_heap_lo_value;
void *mem_heap_lo() {
    if (!mem_heap_lo_value) mem_heap_lo_value = malloc(20 * 1024 * 1024);
    return mem_heap_lo_value;
}

#include "block.h"
#include "rbtree.h"

void *init() {
    // The rbnull must also be a block, without payload, it has the minimal length.
    set_block_length((block_t)rbnull, 12);
    // Set it to "allocated" to have no footer.
    set_block_is_free_flag_only((block_t)rbnull, false);

    init_block_header_type_bit((block_t)rbnull);

    rb_set_color_unchecked(rbnull, RB_BLACK);
    rb_set_parent_unchecked(rbnull, rbnull);
    rb_set_child_unchecked(rbnull, RB_LEFT, rbnull);
    rb_set_child_unchecked(rbnull, RB_RIGHT, rbnull);
    
    rbroot = rbnull;

    return (void *)(12 + (uint8_t *)(rbnull + 12));
}

void init_block(block_t block, size_t length) {
    set_block_length(block, length);
    set_block_is_free_flag_only(block, true);
    init_block_header_type_bit(block);
}

void swap(size_t *a, size_t *b) {
    size_t x = *a;
    *a = *b;
    *b = x;
}

int main() {
    // srand(time(NULL));
    srand(1);
    
    size_t n = 10;

    size_t sizes[n];
    for (size_t length = 16, i = 0; i < n; length += 4, i++) {
        sizes[i] = length;

        if (i > 0) {
            // swap(&sizes[i], &sizes[rand() % i]);
        }
    }

    rb_node_t nodes[n], deleted_nodes[n];
    size_t count = 0, deleted_count = 0;

    uint8_t *current = init();
    printf("init address = %p\n", current);
    for (size_t i = 0; i < n; i++) {
        size_t length = sizes[i];

        block_t block = (block_t)current;
        init_block(block, length);
        rb_node_t node = (rb_node_t)block;
        rb_insert(node);

        nodes[count++] = node;

        current += length;

        printf("i bh = %u\n", rb_traversal(rbroot, 0, false, true, NULL));
    }

    // // Delete 10% of nodes
    // for (size_t j = 0; j < n / 10; j++) {
    // }

    for (size_t j = 0; j <= 15 || true; j++) {
        size_t values[n], *values_end = values;
        if (((rand() & 1) && rbroot != rbnull) || !deleted_count) {
            size_t i = rand() % count;
            rb_delete(nodes[i]);
            deleted_nodes[deleted_count++] = nodes[i];

            size_t value = rb_value(nodes[i]);

            count--;
            for (size_t k = i; k < count; k++) nodes[k] = nodes[k + 1];

            printf("%u d(%u) bh = %u\n", j, (unsigned)(value / 4 - 4), rb_traversal(rbroot, 0, true, true, &values_end));
        } else {
            size_t i = rand() % deleted_count;
            rb_insert(deleted_nodes[i]);
            nodes[count++] = deleted_nodes[i];

            size_t value = rb_value(deleted_nodes[i]);

            deleted_count--;
            for (size_t k = i; k < deleted_count; k++) deleted_nodes[k] = deleted_nodes[k + 1];

            printf("%u i(%u) bh = %u\n", j, (unsigned)(value / 4 - 4), rb_traversal(rbroot, 0, true, true, &values_end));
        }

        assert((size_t)(values_end - values) == count);
        for (size_t *p = values; p != values_end; p++) {
            bool found = false;
            for (size_t i = 0; i < count; i++) {
                if (rb_value(nodes[i]) == *p) {
                    found = true;
                    break;
                }
            }
            assert(found);
        }
    }

    // rb_print();
}
