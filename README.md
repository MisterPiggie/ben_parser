# ben_parser for parsing and encoding benecnoded data

A bencode encoder and decoder for C, using arena allocation. Decodes bencode into a tree of typed values, and encodes that tree back to bencode bytes. 

## Design decisions

Allocation is arena-based, and the arena is borrowed, not owned. Every parse and encode call takes an Arena * from the caller. The library never creates or destroys an arena itself — it just allocates into one for the duration of a call. This means the caller controls the lifetime of every BEN_value tree: reset the arena when you're done with the result, or keep it alive as long as you need it. For arena imlementation check arena.h file or github.vom/MisterPiggie/arena.h

## API

### Decoding
 
```c
BEN_parser *BEN_init_parser_from_buffer(Arena *arena, const unsigned char *data, size_t len);
BEN_parser *BEN_init_parser_from_file(Arena *arena, const char *file_path);
BEN_value  *BEN_decode_buffer(Arena *arena, const unsigned char *data, size_t len);
BEN_value  *BEN_decode_file(Arena *arena, const char *file_path);
```
### Encoding
 
```c
BEN_writer          *BEN_init_writer(Arena *arena, BEN_value *value);
int                 BEN_write_file(BEN_writer *writer, const char *file_path);
int                 BEN_encode_to_file(Arena *arena, BEN_value *value, const char *file_path);
const unsigned char *BEN_encode_to_buffer(Arena *arena, BEN_value *value, size_t *out_len);
```
 
### Working with decoded values
 
```c
BEN_value *BEN_get_value_by_key(const BEN_pair *pairs, const char *key);
bool       BEN_string_equals(const BEN_string *b_key, const char *key);
char      *BEN_string_to_C_string(Arena *arena, const BEN_string *b_string);
```
 
### Types
 
```c
typedef enum {
    BENCODE_STRING,
    BENCODE_NUMBER,
    BENCODE_LIST,
    BENCODE_DICT
} BEN_type;
 
typedef struct {
    unsigned char *data;
    size_t length;
} BEN_string;
 
struct BEN_value {
    BEN_type type;
    union {
        int64_t number;
        BEN_string string;
        BEN_list *list;
        BEN_pair *dict;
    };
};
```

## Quick usage
 
```c
#define ARENA_IMPLEMENTATION
#include "arena.h"
#include "ben.h"
 
int main(void) {
    Arena arena = arena_create(MB(1), KB(64));
 
    // Decode a .torrent file
    BEN_value *torrent = BEN_decode_file(&arena, "example.torrent");
    if (!torrent) {
        // malformed input — rejected cleanly, nothing to clean up
        return 1;
    }
 
    if (torrent->type == BENCODE_DICT) {
        BEN_value *info = BEN_get_value_by_key(torrent->dict, "info");
        // ...
    }
 
    // Round-trip: encode it back out
    size_t encoded_len;
    const unsigned char *encoded = BEN_encode_to_buffer(&arena, torrent, &encoded_len);
    if (!encoded) {
        return 1;
    }
 
    arena_destroy(&arena);
    return 0;
}
```

## Manual usage 


`BEN_decode_file`/`BEN_decode_buffer` and `BEN_encode_to_file`/`BEN_encode_to_buffer` are thin wrappers — three or four lines each — around the lower layers. Most callers want those. The lower layers exist for cases where you need more control: checking failure before doing further work, reusing one parser/writer across multiple steps, or working with a buffer that didn't come from a file (a tracker response, for instance).

**Decoding, manually:**
 
```c
BEN_parser *parser = BEN_init_parser_from_buffer(&arena, data, len);
if (parser->err) {
    // failed before parsing even started — e.g. underlying file read failed
    // if this came from BEN_init_parser_from_file
    return NULL;
}
 
BEN_value *value = BEN_parse(parser);   // the actual parse step
if (parser->err) {
    // malformed bencode — parser->err is set, value is not safe to use
    return NULL;
}
```
 
This is exactly what `BEN_decode_buffer` does internally — constructing the parser and parsing are two separate steps, and the convenience function just does both and collapses the result to `NULL` on either failure. Calling them separately is useful if you want to distinguish "the input couldn't even be read" from "the input was read but wasn't valid bencode," or if you're going to parse multiple top-level values out of the same buffer using one parser instead of starting over each time.
 
**Encoding, manually:**
 
```c
BEN_writer *writer = BEN_init_writer(&arena, value);
if (writer->err) {
    // value couldn't be encoded — e.g. an unrecognized BEN_type
    return -1;
}
 
// writer->data / writer->size now hold the encoded bytes — use them directly,
// or write to a file yourself:
int result = BEN_write_file(writer, "output.torrent");



