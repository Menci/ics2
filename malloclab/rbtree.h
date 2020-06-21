typedef uint8_t *rb_node_t;

typedef enum {
    RB_RED = 0,
    RB_BLACK = 1
} rb_color_t;

typedef enum {
    RB_LEFT = 0,
    RB_RIGHT = 1
} rb_which_t;

#define rb_parent(node) \
    ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 1)))

#define rb_child(node, child) \
    ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (child))))

#define rb_color(node) \
    ((rb_color_t)read_block_header_bit((block_t)(node), 2))

#define rb_value(node) \
    ((size_t)get_block_length((block_t)(node)))

#define rb_set_parent_unchecked(node, value) \
    write_block_header_integer((block_t)(node), 1, from_pointer(value))
#define rb_set_parent(node, value) \
    ((node) == rbnull ? NULL : rb_set_parent_unchecked(node, value))

#define rb_set_child_unchecked(node, child, value) \
    write_block_header_integer((block_t)(node), 2 + (child), from_pointer(value))
#define rb_set_child(node, child, value) \
    ((node) == rbnull ? NULL : rb_set_child_unchecked(node, child, value))

#define rb_set_color_unchecked(node, value) \
    write_block_header_bit((block_t)(node), 2, value)
#define rb_set_color(node, value) \
    ((node) == rbnull ? NULL : rb_set_color_unchecked(node, value))

#define rb_is_black(node) (rb_get_color(node) == RB_BLACK)
#define rb_is_red(node) (rb_get_color(node) == RB_RED)
#define rb_set_black(node) rb_set_color(node, RB_BLACK)
#define rb_set_red(node) rb_set_color(node, RB_RED)

#define rb_which(node, parent) \
    ((rb_which_t)(((node) == rb_child(parent, RB_LEFT)) ? RB_LEFT : RB_RIGHT))

// Store the root pointer in the first 4 bytes
#define rbroot (*(rb_node_t *)mem_heap_lo())
// Store the null node in the following 12 bytes
#define rbnull ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))

void *rb_init() {
    // The rbnull must also be a block, without payload, it has the minimal length but without footer.
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

void rb_init_node(rb_node_t node) {
    // A node not in the tree is indicated by having itself as its parent.
    rb_set_parent(node, node);
}

void rb_rotate(rb_node_t node, rb_which_t which, rb_node_t old_parent, rb_which_t old_parent_which, rb_node_t old_grandparent) {
    rb_set_parent(node, old_grandparent);
    if (old_grandparent != rbnull) rb_set_child(old_grandparent, old_parent_which, node);

    rb_node_t child = rb_child(node, which ^ 1);
    if (child != rbnull) rb_set_parent(child, old_parent);
    rb_set_child(old_parent, which, child);

    rb_set_parent(old_parent, node);
    rb_set_child(node, which ^ 1, old_parent);

    if (old_grandparent == rbnull) rbroot = node;
}

void rb_insert_fixup(rb_node_t node) {
    while (true) {
        if (node == rbroot) {
            rb_set_color(node, RB_BLACK);
            break;
        }

        rb_node_t parent = rb_parent(node);
        if (rb_color(parent) == RB_BLACK) break;

        rb_node_t grandparent = rb_parent(parent);
        rb_which_t parent_which = rb_which(parent, grandparent);
        rb_node_t uncle = rb_child(grandparent, parent_which ^ 1);
        if (rb_color(uncle) == RB_RED) {
            rb_set_color(parent, RB_BLACK);
            rb_set_color(uncle, RB_BLACK);
            rb_set_color(grandparent, RB_RED);
            node = grandparent;
        } else {
            rb_which_t which = rb_which(node, parent);
            rb_node_t tmp;
            if (which != parent_which) {
                rb_rotate(node, which, parent, parent_which, grandparent);
                tmp = node;
            } else {
                tmp = parent;
            }

            rb_set_color(tmp, RB_BLACK);

            rb_node_t tmp_parent = rb_parent(tmp);
            rb_set_color(tmp_parent, RB_RED);
            rb_node_t tmp_grandparent = rb_parent(tmp_parent);
            rb_rotate(tmp, rb_which(tmp, tmp_parent), tmp_parent, rb_which(tmp_parent, tmp_grandparent), tmp_grandparent);
            
            break;
        }
    }
}

void rb_insert(rb_node_t node) {
#ifdef DEBUG
    fprintf(stderr, "rb_insert(%p)\n", node);
#endif
    rb_set_child(node, RB_LEFT, rbnull);
    rb_set_child(node, RB_RIGHT, rbnull);

    if (rbroot == rbnull) {
        rbroot = node;
        rb_set_color(node, RB_BLACK);
        rb_set_parent(node, rbnull);
        return;
    }

    size_t value = rb_value(node);

    rb_node_t current = rbroot;
    rb_which_t which_child;
    while (1) {
        which_child = value <= rb_value(current) ? RB_LEFT : RB_RIGHT;

        rb_node_t child = rb_child(current, which_child);
        if (child != rbnull) {
            current = child;
        } else break;
    }

    rb_set_child(current, which_child, node);
    rb_set_parent(node, current);
    rb_set_color(node, RB_RED);

    rb_insert_fixup(node);
}

void rb_delete_fixup(rb_node_t node, rb_node_t parent, rb_which_t which) {
    while (parent != rbnull) {
        // assert(rb_parent(node) == parent);
        // assert(rb_which(node, parent) == which);

        rb_node_t brother = rb_child(parent, which ^ 1);
        if (rb_color(brother) == RB_RED) {
            rb_set_color(brother, RB_BLACK);
            rb_set_color(parent, RB_RED);

            rb_node_t grandparent = rb_parent(parent);
            rb_which_t parent_which = rb_which(parent, grandparent);
            rb_rotate(brother, which ^ 1, parent, parent_which, grandparent);

            brother = rb_child(parent, which ^ 1);
        }

        rb_node_t brother_left_child = rb_child(brother, RB_LEFT), brother_right_child = rb_child(brother, RB_RIGHT);
        rb_color_t brother_left_child_color = rb_color(brother_left_child),
                   brother_right_child_color = rb_color(brother_right_child);
        if (brother_left_child_color == RB_BLACK && brother_right_child_color == RB_BLACK) {
            rb_set_color(brother, RB_RED);
            // rb_set_color(parent, RB_BLACK);

            if (rb_color(parent) == RB_RED) {
                rb_set_color(parent, RB_BLACK);
                break;
            }

            rb_node_t grandparent = rb_parent(parent);
            rb_which_t parent_which = rb_which(parent, grandparent);
            node = parent;
            parent = grandparent;
            which = parent_which;
        } else {
            rb_node_t brother_same_side_child = which == RB_LEFT ? brother_left_child : brother_right_child;
            // rb_node_t brother_other_side_child = which == RB_LEFT ? brother_right_child : brother_left_child;
            // rb_color_t brother_same_side_child_color = which == RB_LEFT ? brother_left_child_color : brother_right_child_color;
            rb_color_t brother_other_side_child_color = which == RB_LEFT ? brother_right_child_color : brother_left_child_color;
            if (brother_other_side_child_color == RB_BLACK) {
                rb_set_color(brother, RB_RED);
                rb_set_color(brother_same_side_child, RB_BLACK);

                rb_rotate(brother_same_side_child, which, brother, which ^ 1, parent);

                brother = brother_same_side_child;
            }

            rb_set_color(brother, rb_color(parent));
            rb_set_color(parent, RB_BLACK);
            rb_set_color(rb_child(brother, which ^ 1), RB_BLACK);

            rb_node_t grandparent = rb_parent(parent);
            rb_which_t parent_which = rb_which(parent, grandparent);
            rb_rotate(brother, which ^ 1, parent, parent_which, grandparent);

            rb_set_color(rbroot, RB_BLACK);
            break;
        }
    }
}

void rb_delete_with_at_most_one_child(rb_node_t child, rb_node_t parent, rb_which_t which, rb_color_t color) {
    rb_set_parent(child, parent);
    rb_set_child(parent, which, child);

    if (color == RB_RED) {
        return;
    }

    rb_color_t child_color = rb_color(child);
    if (child_color == RB_RED) {
        rb_set_color(child, RB_BLACK);
        return;
    }

    rb_delete_fixup(child, parent, which);
}

void rb_delete(rb_node_t node) {
#ifdef DEBUG
    fprintf(stderr, "rb_delete(%p)\n", node);
#endif
    rb_node_t parent = rb_parent(node);
    assert(parent != node);

    rb_node_t left_child = rb_child(node, RB_LEFT);
    rb_node_t right_child = rb_child(node, RB_RIGHT);
    rb_color_t color = rb_color(node);
    if (left_child == rbnull && right_child == rbnull) {
        rb_delete_with_at_most_one_child(rbnull, parent, rb_which(node, parent), color);
        if (node == rbroot) rbroot = rbnull;
    } else if (left_child == rbnull) {
        rb_delete_with_at_most_one_child(right_child, parent, rb_which(node, parent), color);
        if (node == rbroot) rbroot = right_child;
    } else if (right_child == rbnull) {
        rb_delete_with_at_most_one_child(left_child, parent, rb_which(node, parent), color);
        if (node == rbroot) rbroot = left_child;
    } else {
        rb_which_t which = rb_which(node, parent);

        // Find node's successor
        rb_node_t successor = right_child;
        while (true) {
            rb_node_t tmp = rb_child(successor, RB_LEFT);
            if (tmp == rbnull) break;
            successor = tmp;
        }

        // The successor always have no left child
        rb_node_t successor_left_child = rbnull;
        rb_node_t successor_right_child = rb_child(successor, RB_RIGHT);
        rb_color_t successor_color = rb_color(successor);

        // Swap the deleting node with its successor

        if (color != successor_color) {
            rb_set_color(successor, color);
            rb_set_color(node, successor_color);
        }

        if (node == rbroot) rbroot = successor;
        if (successor == right_child) {
            rb_set_child(parent, which, successor);
            rb_set_parent(successor, parent);

            rb_set_child(successor, RB_LEFT, left_child);
            rb_set_parent(left_child, successor);

            rb_set_child(successor, RB_RIGHT, node);
            rb_set_parent(node, successor);

            rb_set_child(node, RB_LEFT, rbnull);

            rb_set_child(node, RB_RIGHT, successor_right_child);
            rb_set_parent(successor_right_child, node);

            rb_delete_with_at_most_one_child(successor_right_child, successor, RB_RIGHT, successor_color);
        } else {
            rb_node_t successor_parent = rb_parent(successor);
            rb_which_t successor_which = rb_which(successor, successor_parent);

            rb_set_child(parent, which, successor);
            rb_set_parent(successor, parent);

            rb_set_child(successor_parent, successor_which, node);
            rb_set_parent(node, successor_parent);

            rb_set_child(successor, RB_LEFT, left_child);
            rb_set_parent(left_child, successor);

            rb_set_child(successor, RB_RIGHT, right_child);
            rb_set_parent(right_child, successor);

            rb_set_child(node, RB_LEFT, successor_left_child);
            rb_set_parent(successor_left_child, node);

            rb_set_child(node, RB_RIGHT, successor_right_child);
            rb_set_parent(successor_right_child, node);

            rb_delete_with_at_most_one_child(successor_right_child, successor_parent, RB_LEFT, successor_color);
        }
    }

    rb_set_parent(node, node);
}

rb_node_t rb_lower_bound(size_t value) {
    rb_node_t node = rbroot, result = rbnull;
    while (node != rbnull) {
        if (rb_value(node) >= value) {
            result = node;
            node = rb_child(node, RB_LEFT);
        } else {
            node = rb_child(node, RB_RIGHT);
        }
    }
    return result;
}

void rb_traversal_indent(size_t depth) {
    for (size_t i = 0; i < depth * 2; i++) putchar(' ');
}

#ifdef DEBUG
size_t rb_traversal(rb_node_t node, size_t depth, bool print, bool validate, size_t **values) {
    if (node == rbnull) return 0;

    rb_color_t color = rb_color(node);

    rb_node_t left_child = rb_child(node, RB_LEFT),
              right_child = rb_child(node, RB_RIGHT);
    rb_color_t left_child_color = rb_color(left_child),
               right_child_color = rb_color(right_child);
    if (color == RB_RED && validate) {
        assert(left_child_color == RB_BLACK);
        assert(right_child_color == RB_BLACK);
    }

    size_t right_black_height = rb_traversal(right_child, depth + 1, print, validate, values) + (right_child_color == RB_BLACK ? 1 : 0);

    if (print) {
        rb_traversal_indent(depth);
        putchar(color == RB_BLACK ? 'B' : 'R');
        putchar(' ');
        printf("%u\n", (unsigned)(rb_value(node) / 4 - 4));
    }
    if (values) *(*values)++ = rb_value(node);

    size_t left_black_height = rb_traversal(left_child, depth + 1, print, validate, values) + (left_child_color == RB_BLACK ? 1 : 0);
    if (validate) assert(left_black_height == right_black_height);

    return left_black_height;
}

bool rb_in_tree(rb_node_t node, rb_node_t root) {
    return root != rbnull && (node == root || rb_in_tree(node, rb_child(root, RB_LEFT)) || rb_in_tree(node, rb_child(root, RB_RIGHT)));
}

void rb_print() {
    rb_traversal(rbroot, 0, true, false, NULL);
}
#endif
