#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "arena.h"



//BEN structs
typedef enum {
    BENCODE_LIST,
    BENCODE_NUMBER,
    BENCODE_STRING,
    BENCODE_DICT,
} BEN_type;

typedef struct BEN_value BEN_value;


typedef struct {
    unsigned char *data;
    size_t length;
} BEN_string;


typedef struct BEN_pair BEN_pair;
struct BEN_pair{
    BEN_string key;
    BEN_value *value;
    BEN_pair  *next;
};


typedef struct {
    Arena         *arena;
    unsigned char *data;
    size_t        cursor;
    size_t        size;
    int           err;
} BEN_parser;


typedef struct BEN_list BEN_list;
struct BEN_list{
    BEN_value *value;
    BEN_list *next;
};


struct BEN_value {
    BEN_type type;
    union {
        int64_t number;
        BEN_string string;
        BEN_list *list;
        BEN_pair *dict;
    };
};

typedef struct {
    Arena         *arena;
    unsigned char *data;
    size_t        cursor;
    size_t        size;
    int           err;
} BEN_writer;

//parser init
BEN_parser *BEN_init_parser(Arena *arena, const char *file_path);
BEN_writer *BEN_init_writer(Arena *arena, BEN_value *value);


//parser main interface
unsigned char peek(BEN_parser *parser);
unsigned char consume(BEN_parser *parser);

//parser helper funcs
static long BEN_strtol(BEN_parser *parser, char **end_out);


//parsing action
BEN_value *parse_value(BEN_parser *parser);

BEN_value *parse_dict(BEN_parser *parser);
BEN_value *parse_num(BEN_parser *parser);
BEN_value *parse_list(BEN_parser *parser);
BEN_value *parse_string(BEN_parser *parser);
BEN_string parse_raw_string(BEN_parser *parser);


//BEN helper funcs to navigate linked list
BEN_value *BEN_get_value_by_key(const BEN_pair *pairs, const char *key); 
char *BEN_string_to_C_string(Arena *arena, const BEN_string *b_string);
bool BEN_string_equals(const BEN_string *b_key, const char *key);

//writing BEN file
int BEN_encode_data(BEN_writer *writer, const char *fp);
int  BEN_get_value_length(BEN_value *value);
int BEN_write_file(BEN_writer *writer, const char *fp);

//helper funcs to fill in data inside BEN_writer
int increment_by_str(BEN_string *str, int *length);
int increment_by_dict(BEN_pair *dict, int *length);
int increment_by_list(BEN_list *value, int *length);
int increment_by_number(int64_t *value, int *length);

int fill_in_writer_data(BEN_writer *writer, BEN_value *value);
int fill_in_str(BEN_writer *writer, BEN_string *str);
int fill_in_dict(BEN_writer *writer, BEN_pair *dict);
int fill_in_list(BEN_writer *writer, BEN_list *value);
int fill_in_number(BEN_writer *writer, int64_t *value);

