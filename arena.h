#ifndef STB_ARENA_H
#define STB_ARENA_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdarg.h>

#define KB(x) ((size_t)(x) * 1024)
#define MB(x) (KB(x) * 1024)
#define GB(x) (MB(x) * 1024)



typedef struct
{
    uint8_t *memory;
    size_t  used;
    size_t  commited;
    size_t  reserved;
    size_t  commit_chunk;
} Arena;

Arena arena_create(size_t reserve_size, size_t commit_chunk);
void *arena_push(Arena *a, size_t size);
void *arena_push_zero(Arena *a, size_t size);
char *arena_push_str(Arena *a, const char *cstr);
char *arena_push_strf(Arena *a, const char *cstr, ...);
char *arena_push_strn(Arena *a, const char *cstr, size_t n);
void arena_reset(Arena *a);
void arena_rewind(Arena *a);
void arena_destroy(Arena *a);

#define arena_push_array(arena, type, count)  (type *)arena_push((arena), sizeof(type)*(count))
#define arena_push_struct(arena, type) arena_push_array((arena), type, 1)

#define arena_push_array_zero(arena, type, count)  (type *)arena_push_zero((arena), sizeof(type)*(count))
#define arena_push_struct_zero(arena, type) arena_push_array_zero((arena), type, 1)

#endif // !STB_ARENA_H

#ifdef ARENA_IMPLEMENTATION




static void *os_reserve(size_t size)
{
    void *ptr = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr == MAP_FAILED ? NULL : ptr;
}

static void os_commit(void *ptr, size_t size)
{
    mprotect(ptr, size, PROT_READ | PROT_WRITE);
}


static void os_release(void *ptr, size_t size)
{
    munmap(ptr, size);
}

Arena arena_create(size_t reserve_size, size_t commit_chunk)
{
    Arena a = {0};
    a.memory = (uint8_t *)os_reserve(reserve_size);
    a.reserved = reserve_size;
    a.commit_chunk = commit_chunk;
    return a;
}

void *arena_push(Arena *a, size_t size)
{
    size_t align = 16;
    size_t padding = (~a->used + 1) & (align -  1);
    size_t total = size + padding;
    void *ptr;

    while (a->used + total > a->commited) 
    {
        assert(a->commited + a->commit_chunk <= a->reserved && "Arena out of reserved space");
        os_commit(a->memory + a->commited, a->commit_chunk);
        a->commited += a->commit_chunk;
    }

    ptr = a->memory + a->used + padding;
    a->used += total;

    return ptr;

}

void *arena_push_zero(Arena *a, size_t size)
{
    size_t new_commit;
    size_t align = 16;
    size_t padding = (~a->used + 1) & (align -  1);
    size_t total = size + padding;
    void *ptr;

    while (a->used + total > a->commited) 
    {
        new_commit = a->commited + a->commit_chunk;
        assert(new_commit <= a->reserved && "Arena out of reserved space");
        os_commit(a->memory, new_commit);
        a->commited = new_commit;
        assert(a->commited + a->commit_chunk <= a->reserved && "Arena out of reserved space");
        os_commit(a->memory + a->commited, a->commit_chunk);
        a->commited += a->commit_chunk;
    }

    ptr = a->memory + a->used + padding;
    memset(ptr, 0, size);
    a->used += total;

    return ptr;
}

void arena_rewind(Arena *a)
{
    a->used = 0;
}

void arena_reset(Arena *a)
{
    madvise(a->memory, a->commited,  MADV_DONTNEED);
    a->used = 0;
    a->commited = 0;
}
void arena_destroy(Arena *a)
{
    os_release(a->memory, a->reserved);
}

char *arena_push_str(Arena *a, const char *cstr)
{
    size_t len = strlen(cstr);
    char *data = arena_push_array(a, char, len+1);
    memcpy(data, cstr, len+1);
    return data;
}

char *arena_push_strn(Arena *a, const char *cstr, size_t n)
{
    char *data = arena_push_array(a, char, n+1);
    memcpy(data, cstr, n);
    data[n] = '\0';
    return data;
}

char *arena_push_strf(Arena *a, const char *cstr, ...)
{
    va_list args;
    va_start(args, cstr);
    size_t len = vsnprintf(NULL, 0, cstr, args);
    va_end(args);

    char *data = arena_push_array(a, char, len+1);
    va_start(args, cstr);
    vsnprintf(data, len + 1, cstr, args);
    va_end(args);

    return data;
}
#endif // ARENA_IMPLEMENTATION
