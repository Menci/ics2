typedef uint8_t *block_t;

typedef enum {
    BLOCK_8_ALIGNED = 0,
    BLOCK_4_ALIGNED = 1
} block_type_t;

#define minimal_block_size \
    (12 + 3)

#define get_block_type(block) \
    ((block_type_t)!!(((intptr_t)(block)) & 0b100))

#define read_16(p16) \
    ((uint32_t)*(uint16_t *)(p16))

#define read_8(p8) \
    ((uint32_t)*(uint8_t *)(p8))

#define read_bit(pb) \
    ((bool)((*((uint8_t *)(pb)) & 1)))

#define make_high_16_low_8(p16, p8) \
    ((read_16(p16) << 8) | read_8(p8))

#define make_high_8_low_16(p8, p16) \
    ((read_8(p8) << 16) | read_16(p16))

#define make_high_16_low_8_transformed(p16, p8) \
    ((make_high_16_low_8(p16, p8) >> 1) << 2)

#define make_high_8_low_16_transformed(p8, p16) \
    ((make_high_8_low_16(p8, p16) >> 1) << 2)

#define to_pointer(x) \
    ((uint8_t *)mem_heap_lo() + (x))

#define from_pointer(p) \
    (size_t)((p) - (uint8_t *)mem_heap_lo())

#define replace_high_8_low_16_preserve_low_bit(p8, p16, value) \
    ((*(uint8_t *)(p8) = (uint8_t)(((value) << 1) >> 16)), \
     (*(uint16_t *)(p16) = (uint16_t)((((value) << 1)) | read_bit(p16))))

#define replace_high_16_low_8_preserve_low_bit(p16, p8, value) \
    ((*(uint16_t *)(p16) = (uint16_t)(((value) << 1) >> 8)), \
     (*(uint8_t *)(p8) = (uint8_t)(((value) << 1) | read_bit(p8))))

#define write_bit(pb, value) \
    ((*(uint8_t *)(pb)) = ((*(uint8_t *)(pb)) & ~1) | (value))

inline size_t get_block_length(block_t block) {
    return make_high_8_low_16_transformed(&block[2], &block[0]);
}

inline bool get_block_is_free(block_t block) {
    return read_bit(&block[0]);
}

inline void set_block_length_footer(block_t block, size_t value) {
    uint32_t transformed_value = value >> 2;
    replace_high_16_low_8_preserve_low_bit(&block[value - 2], &block[value - 3], transformed_value);
}

inline void set_block_length(block_t block, size_t value) {
    assert(value != 0);

    uint32_t transformed_value = value >> 2;
    replace_high_8_low_16_preserve_low_bit(&block[2], &block[0], transformed_value);
    
    if (get_block_is_free(block) == true) {
        set_block_length_footer(block, value);
    }
}

inline void set_block_is_free_flag_only(block_t block, bool value) {
    write_bit(&block[0], value);
}

inline void init_block_header_type_bit(block_t block) {
    if (get_block_type(block) == BLOCK_8_ALIGNED) {
        write_bit(&block[7], 0);
    } else {
        write_bit(&block[11], 1);
    }
}

inline void *get_block_payload(block_t block) {
    if (get_block_type(block) == BLOCK_8_ALIGNED) {
        return (void *)(8 + (uint8_t *)block);
    } else {
        return (void *)(12 + (uint8_t *)block);
    }
}

inline block_t get_payload_block(void *payload) {
    uint8_t *p = (uint8_t *)payload;
    uint8_t type = read_bit(p - 1);
    if (type == 0) {
        return (block_t)(p - 8);
    } else {
        return (block_t)(p - 12);
    }
}

// We access the second part with the offset from the logical block header's first byte.
// So the "second part address" should be offseted.
inline uint8_t *get_block_header_second_part_offseted(block_t block) {
    bool is_free = get_block_is_free(block);
    size_t length = get_block_length(block);
    uint8_t *block_end = (uint8_t *)block + length;
    uint8_t *block_end_skip_footer = block_end - (is_free ? 3 : 0);
    return block_end_skip_footer - 12;
}

// index: 1 ~ 3
inline uint32_t read_block_header_integer(block_t block, size_t index) {
    if (get_block_type(block) == BLOCK_8_ALIGNED) {
        uint8_t *second_part = get_block_header_second_part_offseted(block);
        
        // 8-aligned
        switch (index) {
        case 1: // 3 ~ 5
            return make_high_16_low_8_transformed(&block[4], &block[3]);
        case 2: { // 6 ~ 7, 8 ~ 8
            uint32_t x0 = *(uint8_t *)&block[6];
            uint32_t x1 = *(uint8_t *)&block[7];
            uint32_t x2 = *(uint8_t *)&second_part[8];
            bool t = read_bit(&second_part[9]);
            x1 = (x1 & ~1) | t;
            uint32_t result = (x2 << 16) | (x1 << 8) | x0;
            return (result >> 1) << 2;
        }
        case 3: // 9 ~ 11
            return make_high_16_low_8_transformed(&second_part[10], &second_part[9]);
        }
    } else {
        // 4-aligned
        switch (index) {
        case 1: // 3 ~ 5
            return make_high_16_low_8_transformed(&block[4], &block[3]);
        case 2: // 6 ~ 8
            return make_high_8_low_16_transformed(&block[8], &block[6]);
        case 3: { // 9 ~ 11
            uint32_t x0 = *(uint8_t *)&block[9];
            uint32_t x1 = *(uint8_t *)&block[10];
            uint32_t x2 = *(uint8_t *)&block[11];
            bool t = x0 & 1;
            x2 = (x2 & ~1) | t;
            uint32_t result = (x2 << 16) | (x1 << 8) | x0;
            return (result >> 1) << 2;
        }
        }
    }
    return -1;
}

// index: 1 ~ 3
inline void write_block_header_integer(block_t block, size_t index, uint32_t value) {
    uint32_t transformed_value = value >> 2;
    if (get_block_type(block) == BLOCK_8_ALIGNED) {
        uint8_t *second_part = get_block_header_second_part_offseted(block);
        
        // 8-aligned
        switch (index) {
        case 1: // 3 ~ 5
            replace_high_8_low_16_preserve_low_bit(&block[5], &block[3], transformed_value);
            return;
        case 2: { // 6 ~ 7, 8 ~ 8
            uint32_t write_value = (transformed_value << 1) | read_bit(&block[6]);
            uint32_t x0 = (uint8_t)write_value;
            uint32_t x1 = (uint8_t)(write_value >> 8);
            uint32_t x2 = (uint8_t)(write_value >> 16);
            bool t = x1 & 1;
            x1 &= ~1; // type = 0
            *(uint8_t *)&block[6] = x0;
            *(uint8_t *)&block[7] = x1;
            *(uint8_t *)&second_part[8] = x2;
            write_bit(&second_part[9], t);
            return;
        }
        case 3: // 9 ~ 11
            replace_high_8_low_16_preserve_low_bit(&second_part[11], &second_part[9], transformed_value);
            return;
        }
    } else {
        // 4-aligned
        switch (index) {
        case 1: // 3 ~ 3, 4 ~ 5
            replace_high_16_low_8_preserve_low_bit(&block[4], &block[3], transformed_value);
            return;
        case 2: // 6 ~ 8
            replace_high_8_low_16_preserve_low_bit(&block[8], &block[6], transformed_value);
            return;
        case 3: { // 9 ~ 11
            uint32_t write_value = transformed_value << 1;
            uint32_t x0 = (uint8_t)write_value;
            uint32_t x1 = (uint8_t)(write_value >> 8);
            uint32_t x2 = (uint8_t)(write_value >> 16);
            bool t = x2 & 1;
            x2 |= 1; // type = 1
            *(uint8_t *)&block[9] = (x0 & ~1) | t;
            *(uint8_t *)&block[10] = x1;
            *(uint8_t *)&block[11] = x2;
            return;
        }
        }
    }
}

// index: 1 ~ 2
inline uint32_t read_block_header_bit(block_t block, size_t index) {
    switch (index) {
    case 1: // 3
        return read_bit(&block[3]);
    case 2: // 6 ~ 8
        return read_bit(&block[6]);
    }
    return -1;
}

// index: 1 ~ 2
inline void write_block_header_bit(block_t block, size_t index, bool value) {
    // 8-aligned
    switch (index) {
    case 1: // 3
        write_bit(&block[3], value);
        return;
    case 2: // 6 ~ 8
        write_bit(&block[6], value);
        return;
    }
}
