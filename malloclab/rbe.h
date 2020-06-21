# 1 "<stdin>"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 1 "<command-line>" 2
# 1 "<stdin>"
typedef uint8_t *rb_node_t;

typedef enum {
    RB_RED = 0,
    RB_BLACK = 1
} rb_color_t;

typedef enum {
    RB_LEFT = 0,
    RB_RIGHT = 1
} rb_which_t;
# 53 "<stdin>"
void rb_rotate(rb_node_t node, rb_which_t which, rb_node_t old_parent, rb_which_t old_parent_which, rb_node_t old_grandparent) {
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 1, from_pointer(old_grandparent)));
    if (old_grandparent != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) ((old_grandparent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(old_grandparent), 2 + (old_parent_which), from_pointer(node)));

    rb_node_t child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (which ^ 1))));
    if (child != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) ((child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(child), 1, from_pointer(old_parent)));
    ((old_parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(old_parent), 2 + (which), from_pointer(child)));

    ((old_parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(old_parent), 1, from_pointer(node)));
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (which ^ 1), from_pointer(old_parent)));

    if (old_grandparent == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) (*(rb_node_t *)mem_heap_lo()) = node;
}

void rb_insert_fixup(rb_node_t node) {
    assert(node != (*(rb_node_t *)mem_heap_lo()));

    rb_node_t parent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 1)));
    if (((rb_color_t)read_block_header_bit((block_t)(parent), 1)) == RB_BLACK) return;

    rb_node_t grandparent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 1)));
    rb_which_t parent_which = ((rb_which_t)(((parent) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(grandparent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));
    rb_node_t uncle = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(grandparent), 2 + (parent_which ^ 1))));
    if (((rb_color_t)read_block_header_bit((block_t)(uncle), 1)) == RB_RED) {
        ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(parent), 1, RB_BLACK));
        ((uncle) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(uncle), 1, RB_BLACK));
        ((grandparent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(grandparent), 1, RB_RED));
        rb_insert_fixup(grandparent);
    } else {
        rb_which_t which = ((rb_which_t)(((node) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));
        rb_node_t tmp;
        if (which != parent_which) {
            rb_rotate(node, which, parent, parent_which, grandparent);
            tmp = node;
        } else {
            tmp = parent;
        }

        ((tmp) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(tmp), 1, RB_BLACK));

        rb_node_t tmp_parent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(tmp), 1)));
        ((tmp_parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(tmp_parent), 1, RB_RED));
        rb_node_t tmp_grandparent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(tmp_parent), 1)));
        rb_rotate(tmp, ((rb_which_t)(((tmp) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(tmp_parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT)), tmp_parent, ((rb_which_t)(((tmp_parent) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(tmp_grandparent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT)), tmp_grandparent);
    }
}

void rb_insert(rb_node_t node) {
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_LEFT), from_pointer(((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())))));
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_RIGHT), from_pointer(((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())))));

    if ((*(rb_node_t *)mem_heap_lo()) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) {
        (*(rb_node_t *)mem_heap_lo()) = node;
        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(node), 1, RB_BLACK));
        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 1, from_pointer(((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())))));
        return;
    }

    size_t value = ((size_t)get_block_length((block_t)(node)));

    rb_node_t current = (*(rb_node_t *)mem_heap_lo());
    int which_child;
    while (1) {
        which_child = value <= ((size_t)get_block_length((block_t)(current))) ? RB_LEFT : RB_RIGHT;

        rb_node_t child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (which_child))));
        if (child != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) {
            current = child;
        } else break;
    }

    ((current) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(current), 2 + (which_child), from_pointer(node)));
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 1, from_pointer(current)));
    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(node), 1, RB_RED));

    rb_insert_fixup(node);
}

void rb_delete_fixup(rb_node_t node, rb_node_t parent, rb_which_t which) {
    rb_color_t color = RB_BLACK;
    while (parent != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) && color == RB_BLACK) {
        rb_node_t brother = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (which ^ 1))));
        if (((rb_color_t)read_block_header_bit((block_t)(brother), 1)) == RB_RED) {
            ((brother) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(brother), 1, RB_BLACK));
            ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(parent), 1, RB_RED));

            rb_node_t grandparent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 1)));
            rb_which_t parent_which = ((rb_which_t)(((parent) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(grandparent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));
            rb_rotate(node, which, parent, parent_which, grandparent);

            parent = grandparent;
            which = parent_which;
            brother = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (which ^ 1))));
        }

        rb_node_t brother_left_child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(brother), 2 + (RB_LEFT)))), brother_right_child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(brother), 2 + (RB_RIGHT))));
        rb_color_t brother_left_child_color = ((rb_color_t)read_block_header_bit((block_t)(brother_left_child), 1)),
                   brother_right_child_color = ((rb_color_t)read_block_header_bit((block_t)(brother_right_child), 1));
        if (brother_left_child_color == RB_BLACK && brother_right_child_color == RB_BLACK) {
            ((brother) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(brother), 1, RB_RED));


            rb_node_t grandparent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 1)));
            rb_which_t parent_which = ((rb_which_t)(((parent) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(grandparent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));
            node = parent;
            parent = grandparent;
            which = parent_which;
        } else {
            assert(brother_right_child != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())));

            rb_node_t brother_same_side_child = which == RB_LEFT ? brother_left_child : brother_right_child;


            rb_color_t brother_other_side_child_color = which == RB_LEFT ? brother_right_child_color : brother_left_child_color;
            if (brother_other_side_child_color == RB_BLACK) {
                ((brother) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(brother), 1, RB_RED));
                ((brother_same_side_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(brother_same_side_child), 1, RB_BLACK));

                rb_rotate(brother_same_side_child, which, brother, which ^ 1, parent);

                brother = brother_same_side_child;
            }

            ((brother) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(brother), 1, ((rb_color_t)read_block_header_bit((block_t)(parent), 1))));
            ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(parent), 1, RB_BLACK));
            ((((rb_node_t)to_pointer(read_block_header_integer((block_t)(brother), 2 + (which ^ 1))))) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(((rb_node_t)to_pointer(read_block_header_integer((block_t)(brother), 2 + (which ^ 1))))), 1, RB_BLACK));

            rb_node_t grandparent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 1)));
            rb_which_t parent_which = ((rb_which_t)(((parent) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(grandparent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));
            rb_rotate(node, which, parent, parent_which, grandparent);

            (((*(rb_node_t *)mem_heap_lo())) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)((*(rb_node_t *)mem_heap_lo())), 1, RB_BLACK));
            return;
        }
    }

    ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(node), 1, RB_BLACK));
}

void rb_delete_with_only_child(rb_node_t child, rb_node_t parent, rb_which_t which, rb_color_t color) {
    ((child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(child), 1, from_pointer(parent)));
    ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(parent), 2 + (which), from_pointer(child)));

    if (color == RB_RED) {
        return;
    }

    rb_color_t child_color = ((rb_color_t)read_block_header_bit((block_t)(child), 1));
    if (child_color == RB_RED) {
        ((child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(child), 1, RB_BLACK));
        return;
    }

    rb_delete_fixup(child, parent, which);
}

void rb_delete(rb_node_t node) {
    rb_node_t left_child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (RB_LEFT))));
    rb_node_t right_child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (RB_RIGHT))));
    rb_node_t parent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 1)));
    rb_color_t color = ((rb_color_t)read_block_header_bit((block_t)(node), 1));
    if (left_child == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) {
        rb_delete_with_only_child(right_child, parent, ((rb_which_t)(((node) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT)), color);
        return;
    } else if (right_child == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) {
        rb_delete_with_only_child(left_child, parent, ((rb_which_t)(((node) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT)), color);
        return;
    }

    rb_which_t which = ((rb_which_t)(((node) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));


    rb_node_t successor = right_child;
    while (true) {
        rb_node_t tmp = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(successor), 2 + (RB_LEFT))));
        if (tmp == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) break;
        successor = tmp;
    }


    rb_node_t successor_left_child = ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()));
    rb_node_t successor_right_child = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(successor), 2 + (RB_RIGHT))));
    rb_color_t successor_color = ((rb_color_t)read_block_header_bit((block_t)(successor), 1));



    if (color != successor_color) {
        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(successor), 1, color));
        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_bit((block_t)(node), 1, successor_color));
    }

    if (successor == right_child) {
        ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(parent), 2 + (which), from_pointer(successor)));
        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 1, from_pointer(parent)));

        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 2 + (RB_LEFT), from_pointer(left_child)));
        ((left_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(left_child), 1, from_pointer(successor)));

        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 2 + (RB_RIGHT), from_pointer(node)));
        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 1, from_pointer(successor)));

        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_LEFT), from_pointer(((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())))));

        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_RIGHT), from_pointer(successor_right_child)));
        ((successor_right_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor_right_child), 1, from_pointer(node)));

        rb_delete_with_only_child(successor_right_child, successor, RB_RIGHT, successor_color);
    } else {
        rb_node_t successor_parent = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(successor), 1)));
        rb_which_t successor_which = ((rb_which_t)(((successor) == ((rb_node_t)to_pointer(read_block_header_integer((block_t)(successor_parent), 2 + (RB_LEFT))))) ? RB_LEFT : RB_RIGHT));

        ((parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(parent), 2 + (which), from_pointer(successor)));
        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 1, from_pointer(parent)));

        ((successor_parent) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor_parent), 2 + (successor_which), from_pointer(node)));
        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 1, from_pointer(successor_parent)));

        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 2 + (RB_LEFT), from_pointer(left_child)));
        ((left_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(left_child), 1, from_pointer(successor)));

        ((successor) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor), 2 + (RB_RIGHT), from_pointer(right_child)));
        ((right_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(right_child), 1, from_pointer(successor)));

        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_LEFT), from_pointer(successor_left_child)));
        ((successor_left_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor_left_child), 1, from_pointer(node)));

        ((node) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(node), 2 + (RB_RIGHT), from_pointer(successor_right_child)));
        ((successor_right_child) == ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo())) ? NULL : write_block_header_integer((block_t)(successor_right_child), 1, from_pointer(node)));

        rb_delete_with_only_child(successor_right_child, successor_parent, RB_LEFT, successor_color);
    }
}

rb_node_t rb_lower_bound(size_t value) {
    rb_node_t node = (*(rb_node_t *)mem_heap_lo()), result = ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()));
    while (node != ((rb_node_t)(sizeof(rb_node_t) + (uint8_t *)mem_heap_lo()))) {
        if (((size_t)get_block_length((block_t)(node))) >= value) {
            result = node;
            node = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (RB_LEFT))));
        } else {
            node = ((rb_node_t)to_pointer(read_block_header_integer((block_t)(node), 2 + (RB_RIGHT))));
        }
    }
    return result;
}
