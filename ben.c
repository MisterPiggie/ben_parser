#include "ben.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"



BEN_parser *BEN_init_parser_from_file(Arena *arena, const char *file_path)
{
    FILE *fp;
    size_t bytes_read;
    long size;

    BEN_parser *parser = arena_push_struct(arena, BEN_parser);
    parser->arena = arena;
    parser->data = NULL;
    parser->size = 0;
    parser->cursor = 0;
    parser->err = 0;


    fp = fopen(file_path, "rb");
    if (!fp)
    {
        parser->err = -1;
        return parser;
    }

    fseek(fp, 0, SEEK_END);

    size = ftell(fp);
    if (size < 0)
    {
        fclose(fp);
        parser->err = -1;
        return parser;
    }
    parser->size = (size_t)size;

    rewind(fp);

    parser->data = arena_push_array(arena, unsigned char, parser->size);

    bytes_read = fread(parser->data, sizeof(unsigned char), parser->size, fp);
    fclose(fp);

    if (bytes_read != parser->size)
        parser->err = -1;

    return parser;
}

BEN_parser *BEN_init_parser_from_buffer(Arena *arena, const unsigned char *data, size_t len)
{
    BEN_parser *parser = arena_push_struct(arena, BEN_parser);
    parser->arena = arena;
    parser->data = NULL;
    parser->size = 0;
    parser->cursor = 0;
    parser->err = 0;

    parser->size = len;

    parser->data = arena_push_array(arena, unsigned char, len);
    memcpy(parser->data, data, len);

    return parser;
}

BEN_value *BEN_decode_file(Arena *arena, const char *file_path)
{
    BEN_parser *parser = BEN_init_parser_from_file(arena, file_path);
    if (parser->err != 0)
        return NULL;

    BEN_value *value = parse_value(parser);
    if (parser->err != 0)
        return NULL;

    return value;
}

BEN_value *BEN_decode_buffer(Arena *arena, const unsigned char *data, size_t len)
{
    BEN_parser *parser = BEN_init_parser_from_buffer(arena, data, len);
    if (parser->err != 0)
        return NULL;

    BEN_value *value = parse_value(parser);
    if (parser->err != 0)
        return NULL;

    return value;
}

BEN_value *parse_dict(BEN_parser *parser)
{
    BEN_pair *tmp_pair;
    BEN_pair *last = NULL;

    BEN_value *return_dict = arena_push_struct(parser->arena, BEN_value);

    return_dict->type = BENCODE_DICT;
    return_dict->dict = NULL;

    if (consume(parser) != 'd')
    {
        parser->err = -1;
        return NULL;
    }

    while((peek(parser) != 'e') && (parser->err == 0))
    {
        tmp_pair        = arena_push_struct(parser->arena, BEN_pair);
        tmp_pair->key   = parse_raw_string(parser);
        tmp_pair->value = parse_value(parser);
        tmp_pair->next  = NULL;
        if (!tmp_pair->value)
        {
            parser->err = -1;
            return NULL;
        }

        if (last)
            last->next        = tmp_pair;
        else
            return_dict->dict = tmp_pair;

        last = tmp_pair;
    }
    if (consume(parser) != 'e')
    {
        parser->err = -1;
        return NULL;
    }
    return return_dict;
}

BEN_value *parse_string(BEN_parser *parser)
{
    BEN_value *return_val = arena_push_struct(parser->arena, BEN_value);

    return_val->type = BENCODE_STRING;
    return_val->string = parse_raw_string(parser);

    return return_val;
}

BEN_value *parse_num(BEN_parser *parser)
{
    int64_t num; 
    char *end;
    BEN_value *return_val = arena_push_struct(parser->arena, BEN_value);

    if (consume(parser) != 'i')   
    {
        parser->err =-1;
        return NULL;
    }

    size_t sign_start = parser->cursor;
    bool negative = (parser->cursor < parser->size && parser->data[parser->cursor] == '-');
    size_t digit_start = sign_start + (negative ? 1 : 0);

    num = strtol((char *) parser->data + parser->cursor, &end, 10);

    if (end == (char *)parser->data + parser->cursor)
    {
        parser->err = -1;
        return NULL;
    }

    size_t digit_count = end - ((char *)parser->data + digit_start);

    if (digit_count > 1 && parser->data[digit_start] == '0')
    {
        parser->err = -1;
        return NULL;
    }

    if (negative && num == 0)
    {
        parser->err = -1;
        return NULL;
    }

    return_val->type = BENCODE_NUMBER;
    return_val->number = num;

    parser->cursor = end - (char *) parser->data; 

    if (consume(parser) != 'e')
    {
        parser->err = -1;
        return NULL;
    }
    
    return return_val;
}

BEN_value *parse_list(BEN_parser *parser)
{
    BEN_list *tmp_list;
    BEN_list *last = NULL;

    BEN_value *val;
    BEN_value *return_val = arena_push_struct(parser->arena, BEN_value);
    return_val->type = BENCODE_LIST;
    return_val->list = NULL;

    if (consume(parser) != 'l')
    {
        parser->err = -1;
        return NULL;
    }
    while(peek(parser) != 'e' && parser->err == 0)
    {
        val = parse_value(parser);
        if (!val)
        {
            parser->err = -1;
            return NULL;
        }

        tmp_list = arena_push_struct(parser->arena, BEN_list);
        tmp_list->value = val;
        tmp_list->next = NULL;

        if (last)
            last->next = tmp_list;
        else
            return_val->list = tmp_list;

        last = tmp_list;
    }

    if (consume(parser) != 'e')
    {
        parser->err = -1;
        return NULL;
    }

    return return_val;
}

unsigned char peek(BEN_parser *parser)
{
    if (parser->cursor >= parser->size)
    {
        parser->err = -1;
        return 0;
    }
    return parser->data[parser->cursor];
}

unsigned char consume(BEN_parser *parser)
{
    if (parser->cursor >= parser->size)
    {
        parser->err = -1;
        return 0;
    }
     return parser->data[parser->cursor++]; 
}


BEN_string parse_raw_string(BEN_parser *parser)
{
    char *delimiter;
    BEN_string raw_str = {0};
    size_t digit_start = parser->cursor;

    long length = BEN_strtol(parser, &delimiter); 
    if (length < 0 || delimiter == (char *)parser->data + parser->cursor)
    {
        parser->err = -1;
        return raw_str;
    }

    size_t digit_count = delimiter - ((char *)parser->data + digit_start);
    if (digit_count > 1 && parser->data[digit_start] == '0')
    {
        parser->err = -1;
        return raw_str;
    }

    if (parser->cursor + (size_t)length > parser->size)
    {
        parser->err = -1;
        return raw_str;
    }

    parser->cursor = delimiter - (char *) parser->data; 

    if (consume(parser) != ':')
    {
        parser->err = -1;
        return raw_str;
    }

    raw_str.data = parser->data + parser->cursor;
    raw_str.length = length;
    parser->cursor += raw_str.length;
    return raw_str;
}

BEN_value *parse_value(BEN_parser *parser)
{
    switch (peek(parser))
    {
        case 'i': return parse_num(parser);
        case 'l': return parse_list(parser);
        case 'd': return parse_dict(parser);

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': 
                  return parse_string(parser);

        default:      
                  parser->err = -1;
                  return NULL;    
    }
}



char *BEN_string_to_C_string(Arena *arena, const BEN_string *b_string)
{
    return arena_push_strn(arena, (char *) b_string->data, b_string->length);
}

bool BEN_string_equals(const BEN_string *b_key, const char *key)
{
    if (b_key->length != strlen(key))
            return false;
    if (strncmp((char *)b_key->data, key, b_key->length) == 0) 
        return true;
    return false;
}

BEN_value *BEN_get_value_by_key(const BEN_pair *pairs, const char *key)
{
    const BEN_pair *tmp_pair = pairs;

    while (tmp_pair != NULL)
    {
        if (BEN_string_equals(&tmp_pair->key, key))
            return tmp_pair->value;
        tmp_pair = tmp_pair->next;
    }

    return NULL;
}

long BEN_strtol(BEN_parser *parser, char **end_out)
{
    size_t i = parser->cursor;
    size_t digit_start;
    int negative = 0;
    long value = 0;

    if (i < parser->size && parser->data[i] == '-')
    {
        negative = 1;
        i++;
    }
    
    digit_start = i;
    while (i < parser->size && parser->data[i] >= '0' && parser->data[i] <= '9')
    {
        value = value * 10 + (parser->data[i] - '0');
        i++;
    }

    if (i == digit_start)
    {
        *end_out = (char *)parser->data + parser->cursor;
        return 0;
    }

    *end_out = (char *)parser->data + i;
    return negative == 0 ? value : -value;
}

BEN_writer *BEN_init_writer(Arena *arena, BEN_value *value)
{

    BEN_writer *writer = arena_push_struct(arena, BEN_writer);
    writer->arena = arena;
    writer->data = NULL;
    writer->cursor = 0;
    writer->err = 0;
    writer->size = BEN_get_value_length(value);
    if (writer->size <= 0)
    {
        writer->err = -1;
        return writer;
    }
    writer->data = arena_push_array(arena, unsigned char, writer->size);

    if (fill_in_writer_data(writer, value) != 0)
    {
        writer->err = -1;
        return writer;
    }

    return writer;
}

int BEN_write_file(BEN_writer *writer, const char *fp)
{
    FILE *fptr = fopen(fp, "wb");
    if (fptr == NULL)
        return -1;

    size_t bytes_written = fwrite(writer->data, sizeof(unsigned char), writer->size, fptr);
    if (bytes_written != writer->size)
    {
        fclose(fptr);
        remove(fp);
        return -1;
    }
    fclose(fptr);

    return 0;
}

int  BEN_get_value_length(BEN_value *value)
{
    int length = 0;
    switch (value->type)
    {
        case BENCODE_STRING:
            if (increment_by_str(&value->string, &length) != 0)
                return -1;
            break;
        case BENCODE_DICT:
            if (increment_by_dict(value->dict, &length) != 0)
                return -1;
            break;
        case BENCODE_NUMBER:
            if (increment_by_number(&value->number, &length) != 0)
                return -1;
            break;
        case BENCODE_LIST:
            if (increment_by_list(value->list, &length) != 0)
                return -1;
            break;
        default:
            return -1;
    }

    return length;
}

int increment_by_str(BEN_string *str, int *length)
{
    size_t n = str->length;
    int digits = 0;
    if (n == 0)
        digits = 1;
    else 
    {
        while (n > 0)
        {
            digits++;
            n /= 10;
        }
    }
    *length += digits;
    *length += 1; //delimiter
    *length += str->length; //str length

    return 0;
}

int increment_by_dict(BEN_pair *dict, int *length)
{
    BEN_pair *tmp_pair = dict;
    while (tmp_pair != NULL)
    {
        if (increment_by_str(&tmp_pair->key, length) != 0)
            return -1;
        int value_len = BEN_get_value_length(tmp_pair->value); 
        if (value_len == -1)
            return -1;
        *length += value_len;

        tmp_pair = tmp_pair->next;
    }

    *length += 2; //d and e 
    return 0;
}

int increment_by_list(BEN_list *value, int *length)
{
    BEN_list *tmp_list = value;
    while (tmp_list != NULL)
    {
        int value_len = BEN_get_value_length(tmp_list->value); 
        if (value_len == -1)
            return -1;
        *length += value_len;
        tmp_list = tmp_list->next;
    }
    *length += 2; //l and e 
    return 0;
}

int increment_by_number(int64_t *value, int *length)
{
    if ( *value == 0)
    {
        *length += 3; // i, e and 0
        return 0;
    }

    *length += 2; //i and e 
    int64_t n = llabs(*value);
    int digits = 0;
    if (n == 0)
        digits = 1;
    else 
    {
        while (n > 0)
        {
            digits++;
            n /= 10;
        }
    }
    *length += digits;

    if (*value < 0)
        *length += 1;

    return 0;
}

int fill_in_writer_data(BEN_writer *writer, BEN_value *value)
{
    switch (value->type)
    {
        case BENCODE_STRING:
            if (fill_in_str(writer, &value->string) != 0)
                return -1;
            break;
        case BENCODE_DICT:
            if (fill_in_dict(writer, value->dict) != 0)
                return -1;
            break;
        case BENCODE_NUMBER:
            if (fill_in_number(writer, &value->number) != 0)
                return -1;
            break;
        case BENCODE_LIST:
            if (fill_in_list(writer, value->list) != 0)
                return -1;
            break;
        default:
            return -1;
    }

    return 0;
}

int fill_in_str(BEN_writer *writer, BEN_string *str)
{
    int bytes_written = snprintf((char *)writer->data + writer->cursor,
        writer->size - writer->cursor, "%zu", str->length);

    if (bytes_written < 0 || bytes_written + writer->cursor > writer->size)
        return -1;
    writer->cursor += bytes_written;

    writer->data[writer->cursor++] = ':';
    for (size_t i = 0; i < str->length && writer->cursor < writer->size; i++)
    {
        writer->data[writer->cursor++] = str->data[i];
    }
    return 0;
}

int fill_in_dict(BEN_writer *writer, BEN_pair *dict)
{
    BEN_pair *tmp_pair = dict;

    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'd';
    while (tmp_pair != NULL)
    {
        if (fill_in_str(writer, &tmp_pair->key) != 0)
            return -1;
        if (fill_in_writer_data(writer, tmp_pair->value) != 0)
            return -1;

        tmp_pair = tmp_pair->next;
    }

    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'e';
    return 0;
}

int fill_in_list(BEN_writer *writer, BEN_list *value)
{
    BEN_list *tmp_list = value;

    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'l';
    while (tmp_list != NULL)
    {
        if (fill_in_writer_data(writer, tmp_list->value) != 0)
            return -1;

        tmp_list = tmp_list->next;
    }

    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'e';
    return 0;
}

int fill_in_number(BEN_writer *writer, int64_t *value)
{
    
    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'i';

    int bytes_written = snprintf((char *)writer->data + writer->cursor,
        writer->size - writer->cursor, "%ld", *value);

    if (bytes_written < 0 || bytes_written + writer->cursor > writer->size)
        return -1;
    writer->cursor += bytes_written;

    if (writer->cursor >= writer->size)
        return -1;

    writer->data[writer->cursor++] = 'e';
    return 0;
}

int BEN_encode_to_file(Arena *arena, BEN_value *value, const char *file_path)
{
    BEN_writer *writer = BEN_init_writer(arena, value);
    if (writer->err != 0)
        return -1;
    if (BEN_write_file(writer, file_path) != 0)
        return -1;

    return 0;
}
const unsigned char *BEN_encode_to_buffer(Arena *arena, BEN_value *value, size_t *out_len)
{
    BEN_writer *writer = BEN_init_writer(arena, value);
    if (writer->err != 0)
    {
        if (out_len)
            *out_len = 0;
        return NULL;
    }
    
    if (out_len)
        *out_len = writer->size;

    return writer->data;
}
