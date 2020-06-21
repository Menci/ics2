#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fuse.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <setjmp.h>
#include "disk.h"

#define inline static inline
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))

#define DIRMODE (0040000 | 0755)
#define REGMODE (0100000 | 0644)

#define INODE_NUM                    32768
#define BLOCK_BITMAP_BLOCK_START     (block_id_t)0
#define BLOCK_BITMAP_BLOCK_NUM       (BLOCK_NUM / BLOCK_SIZE / 8)
#define BLOCK_BITMAP_BLOCK_END       (BLOCK_BITMAP_BLOCK_START + BLOCK_BITMAP_BLOCK_NUM)
#define INODE_BITMAP_BLOCK_START     (block_id_t)BLOCK_BITMAP_BLOCK_END
#define INODE_BITMAP_BLOCK_NUM       (INODE_NUM / BLOCK_SIZE / 8)
#define INODE_BITMAP_BLOCK_END       (block_id_t)(INODE_BITMAP_BLOCK_START + INODE_BITMAP_BLOCK_NUM)
#define INODE_BLOCK_START            (block_id_t)INODE_BITMAP_BLOCK_END
#define INODE_BLOCK_NUM              (INODE_NUM * sizeof(inode_t) / BLOCK_SIZE)
#define INODE_BLOCK_END              (block_id_t)(INODE_BLOCK_START + INODE_BLOCK_NUM)
#define DATA_AND_POINTER_BLOCK_START (block_id_t)INODE_BLOCK_END
#define DATA_AND_POINTER_BLOCK_END   (block_id_t)(BLOCK_NUM - 1)
#define DATA_AND_POINTER_BLOCK_NUM   (DATA_AND_POINTER_BLOCK_END - DATA_AND_POINTER_BLOCK_START)
#define POINTER_BLOCK_ITEM_COUNT     (BLOCK_SIZE / sizeof(block_id_t))

typedef char block_t[BLOCK_SIZE];
typedef uint16_t block_id_t;
typedef uint16_t inode_id_t;

// inode: 32 bytes
typedef struct {
    time_t atime, mtime, ctime;
    uint32_t size;
    bool is_directory;
    block_id_t pointer_block;
} inode_t;

jmp_buf jmp_error;

inline void block_read(block_id_t block_id, block_t block) {
    disk_read(block_id, block);
}

inline void block_write(block_id_t block_id, block_t block) {
    disk_write(block_id, block);
}

inline void block_empty(block_id_t block_id) {
    static char empty[BLOCK_SIZE];
    block_write(block_id, empty);
}

inline void block_read_many(block_id_t block_id_start, block_id_t block_id_end, block_t block_addr) {
    for (block_id_t i = block_id_start; i != block_id_end; i++)
        block_read(i, block_addr + (i - block_id_start) * sizeof(block_t));
}

inline void block_write_many(block_id_t block_id_start, block_id_t block_id_end, block_t block_addr) {
    for (block_id_t i = block_id_start; i != block_id_end; i++)
        block_write(i, block_addr + (i - block_id_start) * sizeof(block_t));
}

typedef uint64_t inode_bitmap_t[INODE_BITMAP_BLOCK_NUM * BLOCK_SIZE / sizeof(uint64_t)];
typedef uint64_t block_bitmap_t[BLOCK_BITMAP_BLOCK_NUM * BLOCK_SIZE / sizeof(uint64_t)];

#define inode_bitmap_read(inode_bitmap) block_read_many(INODE_BITMAP_BLOCK_START, INODE_BITMAP_BLOCK_END, (void *)(inode_bitmap))
#define inode_bitmap_write(inode_bitmap) block_write_many(INODE_BITMAP_BLOCK_START, INODE_BITMAP_BLOCK_END, (void *)(inode_bitmap))
#define block_bitmap_read(block_bitmap) block_read_many(BLOCK_BITMAP_BLOCK_START, BLOCK_BITMAP_BLOCK_END, (void *)(block_bitmap))
#define block_bitmap_write(block_bitmap) block_write_many(BLOCK_BITMAP_BLOCK_START, BLOCK_BITMAP_BLOCK_END, (void *)(block_bitmap))

inline bool bitmap_get_member(uint64_t *bitmap, size_t i) {
    return (bitmap[i / 64] >> (i % 64)) & 1;
}

inline void bitmap_set_member(uint64_t *bitmap, size_t i, bool value) {
    if (value)
        bitmap[i / 64] |= (1ULL << (i % 64));
    else
        bitmap[i / 64] &= ~(1ULL << (i % 64));
}

inline size_t bitmap_count_impl(uint64_t *bitmap, size_t size) {
    size_t result = 0;
    for (size_t i = 0; i < size; i++)
        if (bitmap_get_member(bitmap, i) == true)
            result++;
    return result;
}

#define bitmap_count(bitmap) bitmap_count_impl(bitmap, sizeof(bitmap) * 8)

inline block_id_t block_new() {
    block_bitmap_t bitmap;
    block_bitmap_read(bitmap);
    for (block_id_t i = DATA_AND_POINTER_BLOCK_START; i != DATA_AND_POINTER_BLOCK_END; i++)
        if (bitmap_get_member(bitmap, i) == false) {
            bitmap_set_member(bitmap, i, true);
            block_bitmap_write(bitmap);
            return i;
        }

    longjmp(jmp_error, -ENOSPC);
}

inline void block_new_many(size_t n, block_id_t buffer[n]) {
    if (n == 0) return;

    block_bitmap_t bitmap;
    block_bitmap_read(bitmap);

    size_t current = 0;
    for (block_id_t i = DATA_AND_POINTER_BLOCK_START; i != DATA_AND_POINTER_BLOCK_END; i++)
        if (bitmap_get_member(bitmap, i) == false) {
            buffer[current++] = i;
            if (current == n) break;
        }

    if (current < n)
        longjmp(jmp_error, -ENOSPC);
    
    for (size_t i = 0; i < n; i++) {
        bitmap_set_member(bitmap, buffer[i], true);
    }
    block_bitmap_write(bitmap);
}

// Get if there're enough available blocks
inline bool block_seek(size_t n) {
    if (n == 0) return true;

    block_bitmap_t bitmap;
    block_bitmap_read(bitmap);
    for (block_id_t i = DATA_AND_POINTER_BLOCK_START; i != DATA_AND_POINTER_BLOCK_END; i++)
        if (bitmap_get_member(bitmap, i) == false && --n == 0)
            return true;
    return false;
}

inline void block_delete(block_id_t *block_id_array, size_t count) {
    if (count == 0) return;

    block_bitmap_t bitmap;
    block_bitmap_read(bitmap);
    for (size_t i = 0; i < count; i++) {
        bitmap_set_member(bitmap, block_id_array[i], false);
        block_empty(block_id_array[i]);
        block_id_array[i] = 0;
    }
    block_bitmap_write(bitmap);
}

inline size_t block_count_from_size(size_t size) {
    return size / BLOCK_SIZE + !!(size % BLOCK_SIZE);
}

inline void inode_resolve(inode_id_t inode_id, block_id_t *block_id, size_t *offset_in_block) {
    size_t offset = (size_t)inode_id * sizeof(inode_t);
    *block_id = INODE_BLOCK_START + offset / BLOCK_SIZE;
    *offset_in_block = offset - (*block_id - INODE_BLOCK_START) * BLOCK_SIZE;
}

inline inode_t inode_read(inode_id_t inode_id) {
    block_id_t block_id;
    size_t offset_in_block;
    inode_resolve(inode_id, &block_id, &offset_in_block);

    block_t block;
    block_read(block_id, block);

    return *(inode_t *)(block + offset_in_block);
}

inline void inode_write(inode_id_t inode_id, inode_t inode) {
    block_id_t block_id;
    size_t offset_in_block;
    inode_resolve(inode_id, &block_id, &offset_in_block);

    block_t block;
    block_read(block_id, block);

    *(inode_t *)(block + offset_in_block) = inode;
    block_write(block_id, block);
}

#define ROOT_INODE (inode_id_t)0

inline inode_id_t inode_new() {
    inode_bitmap_t bitmap;
    inode_bitmap_read(bitmap);
    for (inode_id_t i = 1; i != INODE_NUM; i++)
        if (bitmap_get_member(bitmap, i) == false) {
            bitmap_set_member(bitmap, i, true);
            inode_bitmap_write(bitmap);
            return i;
        }

    longjmp(jmp_error, -ENOSPC);
}

// Get if there're enough available inodes
inline bool inode_seek(size_t n) {
    if (n == 0) return true;

    inode_bitmap_t bitmap;
    inode_bitmap_read(bitmap);
    for (inode_id_t i = 1; i != INODE_NUM; i++)
        if (bitmap_get_member(bitmap, i) == false && --n == 0)
            return true;
    return false;
}

inline void inode_delete(inode_id_t inode_id) {
    inode_bitmap_t bitmap;
    inode_bitmap_read(bitmap);
    bitmap_set_member(bitmap, inode_id, false);
    inode_bitmap_write(bitmap);
}

inline void inode_initialize(inode_id_t inode_id, bool is_directory) {
    inode_t inode;
    inode.atime = inode.mtime = inode.ctime = time(NULL);
    inode.size = 0;
    inode.is_directory = is_directory;
    inode.pointer_block = block_new();
    inode_write(inode_id, inode);
}

inline void inode_finalize(inode_id_t inode_id) {
    inode_t inode = inode_read(inode_id);
    block_t pointer_block;
    block_read(inode.pointer_block, pointer_block);
    block_delete((block_id_t *)pointer_block, block_count_from_size(inode.size));
    block_delete(&inode.pointer_block, 1);
    inode_delete(inode_id);
}

#define ENTRY_NAME_LENGTH 24

typedef struct {
    char name[ENTRY_NAME_LENGTH];
    inode_id_t inode_id;
} entry_t;

inline entry_t *entries_read(inode_t directory_inode, entry_t *buffer) {
    static entry_t entries[INODE_NUM];
    block_t pointer_block;
    block_read(directory_inode.pointer_block, pointer_block);
    block_id_t *block_ids = (block_id_t *)pointer_block;

    if (!buffer) buffer = entries;

    char *p = (char *)buffer;
    for (size_t i = 0; i < block_count_from_size(directory_inode.size); i++) {
        block_read(block_ids[i], p);
        p += BLOCK_SIZE;
    }
    return buffer;
}

// Make sure there're enough blocks to store a new entry
inline void entries_extend(inode_t directory_inode) {
    size_t old_count = block_count_from_size(directory_inode.size),
           new_count = block_count_from_size(directory_inode.size + 1);
    
    if (old_count == new_count) return;

    block_t pointer_block;
    block_read(directory_inode.pointer_block, pointer_block);
    block_id_t *block_ids = (block_id_t *)pointer_block;
    
    block_ids[new_count - 1] = block_new();
    block_write(directory_inode.pointer_block, pointer_block);
}

inline void entries_write(inode_t directory_inode, entry_t *entries) {
    block_t pointer_block;
    block_read(directory_inode.pointer_block, pointer_block);
    block_id_t *block_ids = (block_id_t *)pointer_block;

    char *p = (char *)entries;
    size_t i;
    for (i = 0; i < block_count_from_size(directory_inode.size); i++) {
        block_write(block_ids[i], p);
        p += BLOCK_SIZE;
    }

    // Delete blocks not used
    size_t t = i, blocks_to_delete = 0;
    for (; i < POINTER_BLOCK_ITEM_COUNT && block_ids[i]; i++) {
        blocks_to_delete++;
    }
    block_delete(&block_ids[t], blocks_to_delete);
    block_write(directory_inode.pointer_block, pointer_block);
}

inline size_t entries_count(inode_t directory_inode) {
    return directory_inode.size / sizeof(entry_t);
}

inline void entry_normalize_name(const char *name, size_t length, char buffer[ENTRY_NAME_LENGTH]) {
    memset(buffer, 0, ENTRY_NAME_LENGTH);
    memcpy(buffer, name, length);
}

inline entry_t *entries_find(entry_t *entires, size_t entry_count, const char *name, size_t name_length) {
    char normalized_name[ENTRY_NAME_LENGTH];
    entry_normalize_name(name, name_length, normalized_name);
    
    for (entry_t *entry = entires; entry != entires + entry_count; entry++) {
        if (memcmp(entry->name, normalized_name, ENTRY_NAME_LENGTH) == 0) {
            return entry;
        }
    }

    return NULL;
}

inline inode_t resolve_path(const char *path, inode_id_t *inode_id) {
    inode_t inode = inode_read(ROOT_INODE);
    if (inode_id) *inode_id = ROOT_INODE;
    while (true) {
        if (*path == '/') path++;
        if (!*path) break;

        size_t name_length = strchrnul(path, '/') - path;

        if (!inode.is_directory) longjmp(jmp_error, -ENOTDIR);

        entry_t *entry = entries_find(entries_read(inode, NULL), entries_count(inode), path, name_length);
        if (entry) {
            inode = inode_read(entry->inode_id);
            if (inode_id) *inode_id = entry->inode_id;
            path += name_length;
        } else longjmp(jmp_error, -ENOENT);
    }

    return inode;
}

inline inode_t resolve_parent_path(const char *path, inode_id_t *directory_inode_id, char basename_buffer[ENTRY_NAME_LENGTH + 1], bool *exists) {
    inode_t directory_inode, inode = inode_read(ROOT_INODE);
    inode_id_t inode_id = ROOT_INODE;

    if (exists) *exists = true;
    bool found_non_exist = false;
    while (true) {
        if (*path == '/') path++;
        if (!*path) break;
        if (found_non_exist) longjmp(jmp_error, -ENOENT);

        size_t name_length = strchrnul(path, '/') - path;

        if (!inode.is_directory) longjmp(jmp_error, -ENOTDIR);

        entry_t *entry = entries_find(entries_read(inode, NULL), entries_count(inode), path, name_length);
        directory_inode = inode;
        if (directory_inode_id) *directory_inode_id = inode_id;
        if (entry) {
            inode = inode_read(entry->inode_id);
            if (directory_inode_id) inode_id = entry->inode_id;
        } else found_non_exist = true;
        memset(basename_buffer, 0, ENTRY_NAME_LENGTH + 1);
        strncpy(basename_buffer, path, name_length);
        basename_buffer[name_length] = '\0';
        path += name_length;
    }

    return directory_inode;
}

// Format the virtual block device in the following function
int mkfs() {
    inode_initialize(ROOT_INODE, true);
    return 0;
}

// Filesystem operations that you need to implement
int fs_getattr(const char *path, struct stat *attr) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_t inode = resolve_path(path, NULL);
        attr->st_mode = inode.is_directory ? DIRMODE : REGMODE;
        attr->st_nlink = 1;
        attr->st_uid = getuid();
        attr->st_gid = getgid();
        attr->st_size = inode.size;
        attr->st_atime = inode.atime;
        attr->st_mtime = inode.mtime;
        attr->st_ctime = inode.ctime;
        return 0;
    } else {
        return error;
    }
}

int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t inode_id;
        inode_t inode = resolve_path(path, &inode_id);
        if (!inode.is_directory)
            return -ENOTDIR;

        entry_t *entires = entries_read(inode, NULL);
        for (entry_t *entry = entires; entry != entires + entries_count(inode); entry++) {
            static char name_buffer[ENTRY_NAME_LENGTH + 1];
            memcpy(name_buffer, entry->name, ENTRY_NAME_LENGTH);
            filler(buffer, name_buffer, NULL, 0);
        }

        inode.atime = time(NULL);
        inode_write(inode_id, inode);

        return 0;
    } else {
        return error;
    }
    return 0;
}

inline int make_directory_or_file(const char *path, bool is_directory) {
    int error = setjmp(jmp_error);
    if (!error) {
        if (!inode_seek(1) || !block_seek(2))
            return -ENOSPC;

        inode_id_t directory_inode_id;
        char basename[ENTRY_NAME_LENGTH + 1];
        bool exists;
        inode_t directory_inode = resolve_parent_path(path, &directory_inode_id, basename, &exists);

        size_t count = entries_count(directory_inode);
        entry_t *entries = entries_read(directory_inode, NULL);
        entries_extend(directory_inode);

        entries[count].inode_id = inode_new();
        memcpy(entries[count].name, basename, ENTRY_NAME_LENGTH);

        inode_initialize(entries[count].inode_id, is_directory);

        directory_inode.mtime = directory_inode.ctime = time(NULL);
        directory_inode.size += sizeof(entry_t);
        inode_write(directory_inode_id, directory_inode);
        entries_write(directory_inode, entries);
    } else {
        return error;
    }
    return 0;
}

int fs_mknod(const char *path, mode_t mode, dev_t dev) {
    return make_directory_or_file(path, false);
}

int fs_mkdir(const char *path, mode_t mode) {
    return make_directory_or_file(path, true);
}

int remove(const char *path) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t directory_inode_id;
        char basename[ENTRY_NAME_LENGTH + 1];
        inode_t directory_inode = resolve_parent_path(path, &directory_inode_id, basename, NULL);

        entry_t *entries = entries_read(directory_inode, NULL);
        entry_t *entry = entries_find(entries, entries_count(directory_inode), basename, strlen(basename));
        if (!entry)
            return -ENOENT;
        
        inode_finalize(entry->inode_id);

        size_t entries_to_move = (entries + entries_count(directory_inode)) - (entry + 1);
        memmove(entry, entry + 1, entries_to_move * sizeof(entry_t));
        directory_inode.size -= sizeof(entry_t);
        directory_inode.atime = directory_inode.mtime = time(NULL);
        entries_write(directory_inode, entries);
        inode_write(directory_inode_id, directory_inode);
    } else {
        return error;
    }
    return 0;
}

int fs_rmdir(const char *path) {
    return remove(path);
}

int fs_unlink(const char *path) {
    return remove(path);
}

#define FOR_DATA_BLOCKS(size_remain) \
    for ( \
            size_t i = offset / BLOCK_SIZE, offset_in_block = offset % BLOCK_SIZE, size_remain = size, size_in_block; \
            (size_in_block = min(BLOCK_SIZE - offset_in_block, size_remain)) != 0; \
            i++, size_remain -= size_in_block, offset_in_block = 0 \
        )

int fs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t inode_id;
        inode_t inode = resolve_path(path, &inode_id);
        if (inode.is_directory)
            return -EISDIR;

        block_t pointer_block;
        block_read(inode.pointer_block, pointer_block);
        block_id_t *block_ids = (block_id_t *)pointer_block;

        FOR_DATA_BLOCKS(size_remain) {
            block_t block;
            block_read(block_ids[i], block);
            memcpy(buffer, &block[offset_in_block], size_in_block);
            buffer += size_in_block;
        }

        inode.atime = time(NULL);
        inode_write(inode_id, inode);

        return size;
    } else {
        return error;
    }
    return 0;
}

int fs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t inode_id;
        inode_t inode = resolve_path(path, &inode_id);
        if (inode.is_directory)
            return -EISDIR;

        block_t pointer_block;
        block_read(inode.pointer_block, pointer_block);
        block_id_t *block_ids = (block_id_t *)pointer_block;

        // Allocate all new blocks needed once to prevent inconsistent on failure
        size_t new_blocks_needed = 0;
        FOR_DATA_BLOCKS(size_remain) {
            if (block_ids[i] == 0) new_blocks_needed++;
        }

        block_id_t new_block_ids[new_blocks_needed];
        block_new_many(new_blocks_needed, new_block_ids);

        FOR_DATA_BLOCKS(size_remain) {
            if (block_ids[i] == 0) block_ids[i] = new_block_ids[--new_blocks_needed];

            block_t block;
            block_read(block_ids[i], block);
            memcpy(&block[offset_in_block], buffer, size_in_block);
            buffer += size_in_block;
            block_write(block_ids[i], block);
        }

        block_write(inode.pointer_block, pointer_block);

        inode.ctime = inode.atime = time(NULL);
        inode.size = max(inode.size, size + offset);
        inode_write(inode_id, inode);

        return size;
    } else {
        return error;
    }
    return 0;
}

int fs_rename(const char *oldpath, const char *newpath) {
    if (strcmp(oldpath, newpath) == 0) return 0;
    fs_unlink(newpath);

    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t src_inode_id, dst_inode_id;
        char src_basename[ENTRY_NAME_LENGTH + 1], dst_basename[ENTRY_NAME_LENGTH + 1];
        inode_t src_inode = resolve_parent_path(oldpath, &src_inode_id, src_basename, NULL),
                dst_directory_inode = resolve_parent_path(newpath, &dst_inode_id, dst_basename, NULL);

        static entry_t src_entries[INODE_NUM];
        entries_read(src_inode, src_entries);
        entry_t *src_entry = entries_find(src_entries, entries_count(src_inode), src_basename, strlen(src_basename));
        src_inode.mtime = src_inode.atime = time(NULL);
        if (!src_entry)
            return -ENOENT;
        
        if (src_inode_id == dst_inode_id) {
            memcpy(src_entry->name, dst_basename, ENTRY_NAME_LENGTH);
            entries_write(src_inode, src_entries);
            inode_write(src_inode_id, src_inode);
        } else {
            static entry_t dst_entries[INODE_NUM];
            entries_read(dst_directory_inode, dst_entries);
            entry_t *dst_entry = entries_find(dst_entries, entries_count(dst_directory_inode), dst_basename, strlen(dst_basename));
            if (dst_entry)
                return -EEXIST;
            
            entries_extend(dst_directory_inode);
            size_t dst_count = entries_count(dst_directory_inode);
            memcpy(dst_entries[dst_count].name, dst_basename, ENTRY_NAME_LENGTH);
            dst_entries[dst_count].inode_id = src_entry->inode_id;
            dst_directory_inode.mtime = dst_directory_inode.atime = time(NULL);
            dst_directory_inode.size += sizeof(entry_t);
            entries_write(dst_directory_inode, dst_entries);
            inode_write(dst_inode_id, dst_directory_inode);

            size_t entries_to_move = (src_entries + entries_count(src_inode)) - (src_entry + 1);
            memmove(src_entry, src_entry + 1, entries_to_move * sizeof(entry_t));
            src_inode.size -= sizeof(entry_t);
            entries_write(src_inode, src_entries);
            inode_write(src_inode_id, src_inode);
        }
    } else {
        return error;
    }
    return 0;
}

int fs_truncate(const char *path, off_t size) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t inode_id;
        inode_t inode = resolve_path(path, &inode_id);
        if (inode.is_directory)
            return -EISDIR;

        block_t pointer_block;
        block_read(inode.pointer_block, pointer_block);
        block_id_t *block_ids = (block_id_t *)pointer_block;

        size_t old_block_count = block_count_from_size(inode.size),
               new_block_count = block_count_from_size(size);

        if (size > inode.size) {
            size_t new_blocks_required = new_block_count - old_block_count;
            block_id_t new_block_ids[new_blocks_required];
            block_new_many(new_blocks_required, new_block_ids);

            for (block_id_t i = old_block_count; i != new_block_count; i++) {
                block_ids[i] = new_block_ids[--new_blocks_required];
            }
        } else {
            size_t new_last_block_tail_start = size % BLOCK_SIZE;
            if (new_last_block_tail_start != 0) {
                block_t new_last_block;
                block_read(block_ids[new_block_count - 1], new_last_block);
                memset(new_last_block + new_last_block_tail_start, 0, BLOCK_SIZE - new_last_block_tail_start);
                block_write(block_ids[new_block_count - 1], new_last_block);
            }

            block_delete(&block_ids[new_block_count], old_block_count - new_block_count);
        }
        
        inode.size = size;
        inode.ctime = time(NULL);
        block_write(inode.pointer_block, pointer_block);
        inode_write(inode_id, inode);
    } else {
        return error;
    }
    return 0;
}

int fs_utime(const char *path, struct utimbuf *buffer) {
    int error = setjmp(jmp_error);
    if (!error) {
        inode_id_t inode_id;
        inode_t inode = resolve_path(path, &inode_id);

        inode.ctime = time(NULL);
        inode.atime = buffer->actime;
        inode.mtime = buffer->modtime;

        inode_write(inode_id, inode);
    } else {
        return error;
    }
    return 0;
}

int fs_statfs(const char *path, struct statvfs *stat) {
    inode_bitmap_t inode_bitmap;
    inode_bitmap_read(inode_bitmap);
    block_bitmap_t block_bitmap;
    block_bitmap_read(block_bitmap);

    stat->f_bsize = BLOCK_SIZE;
    stat->f_blocks = DATA_AND_POINTER_BLOCK_NUM;
    stat->f_bavail = stat->f_bfree = DATA_AND_POINTER_BLOCK_NUM - bitmap_count(block_bitmap);
    stat->f_files = INODE_NUM;
    stat->f_ffree = stat->f_favail = INODE_NUM - 1 - bitmap_count(inode_bitmap);
    stat->f_namemax = ENTRY_NAME_LENGTH;

    return 0;
}

int fs_stub(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static struct fuse_operations fs_operations = {
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .read       = fs_read,
    .mkdir      = fs_mkdir,
    .rmdir      = fs_rmdir,
    .unlink     = fs_unlink,
    .rename     = fs_rename,
    .truncate   = fs_truncate,
    .utime      = fs_utime,
    .mknod      = fs_mknod,
    .write      = fs_write,
    .statfs     = fs_statfs,
    .open       = fs_stub,
    .release    = fs_stub,
    .opendir    = fs_stub,
    .releasedir = fs_stub
};

int main(int argc, char *argv[]) {
    if (disk_init()) {
        printf("Can't open virtual disk!\n");
        return -1;
    }

    if (mkfs()) {
        printf("Mkfs failed!\n");
        return -2;
    }

    return fuse_main(argc, argv, &fs_operations, NULL);
}
