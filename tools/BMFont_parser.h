#ifndef BMFONT_PARSER_H
#define BMFONT_PARSER_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    float x;
    float y;
    float w;
    float h;
} BMF_Quad;

//Font structs
typedef struct {
    uint32_t codepoint; //Unicode codepoint
    BMF_Quad sub_rect; //What region of the page the glyph occupies
    float x_offset, y_offset, x_advance; //Cursor positions before and after drawing this character
    uint8_t page; //Page used to draw this character
    uint8_t channel; //Channel flags
} BMF_Glyph;

typedef struct {
    uint32_t first, second; //Codepoints of the chars involved in the kerning
    float amount; //How much the xpos should be adjusted when drawing the second char immediately following the first
} BMF_Kerning;

typedef enum {
    BMF_BINARY,
    BMF_TEXT,
} BMF_Format;

typedef struct {
    size_t size;
    size_t capacity;
    uint64_t *keys;
    uint32_t *values;
} BMFi_Hashmap;

typedef struct {
    //Each glyph's page attribute is 1 byte in the binary format, so only need to worry about 256 pages max
    BMF_Glyph *glyphs;
    BMF_Kerning *kernings;
    BMFi_Hashmap *glyph_map;
    BMFi_Hashmap *kerning_map;
    size_t glyph_capacity;
    size_t glyph_count;
    size_t kerning_capacity;
    size_t kerning_count;
    uint32_t line_height;
    uint32_t base;
    uint8_t page_count;
    uint8_t init;
} BMFont;

uint8_t BMF_load_font(const char *font_path, BMF_Format format, BMFont *font);
void BMF_print_parsing_error(void);
void BMF_font_free(BMFont *font);

#endif

#ifdef BMFONT_PARSER_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define BMFi_HASHMAP_DUMMY UINT64_MAX

uint32_t BMFi_hashmap_add(BMFi_Hashmap *h, uint64_t key, uint32_t value);

//Checks if n is prime
uint64_t BMFi_is_prime(uint64_t n) {
    if(n <= 1) return 0;
    if(n <= 3) return 1;
    if(0 == n % 2 || 0 == n % 3) return 0;

    for(size_t i = 5; i * i <= n; i +=6) {
        if(n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

//Gets the next prime number after n
uint64_t BMFi_next_prime(uint64_t n) {
    if(n <= 2) return 2;
    n = (0 == n % 2) ? n +1 : n; //Make sure n is odd

    while(!BMFi_is_prime(n)){
        n += 2; //Skip even numbers
    }
    return n;

}

uint8_t BMFi_hashmap_init(size_t init_capacity, BMFi_Hashmap *out) {
    out->capacity = BMFi_next_prime(init_capacity);
    out->keys = malloc(sizeof(uint64_t) * out->capacity);
    if(out->keys == NULL) return 0;
    memset(out->keys, 0xFF, sizeof(uint64_t) * out->capacity);
    out->values = malloc(sizeof(uint32_t) * out->capacity);
    if(out->values == NULL) return 0;
    memset(out->values, 0xFF, sizeof(uint32_t) * out->capacity);

    return 1;
}

//Primary hash funtion.
uint64_t BMFi_hash_int(uint64_t key, size_t length) {
    return (key & 0x7FFFFFFFFFFFFFFF) % length;
}

//Secondary hash funtion.
uint64_t BMFi_second_hash_int(uint64_t key, size_t length) {
    return 1 + (key & 0x7FFFFFFFFFFFFFFF) % (length - 1);
}

//Finds the next slot that we can put a value into in the hashmap
uint64_t BMFi_hashmap_find(const BMFi_Hashmap *h, uint64_t key) {
    uint64_t hash = BMFi_hash_int(key, h->capacity);
    uint64_t step = BMFi_second_hash_int(key, h->capacity);

    uint64_t j = hash;

    for (size_t i = 0; i < h->capacity; ++i) {
        if (h->keys[j] == BMFi_HASHMAP_DUMMY) //At an empty slot
            return UINT64_MAX;

        if (h->keys[j] == key)
            return j;

        j = (j + step) % h->capacity;
    }

    return UINT64_MAX;
}

uint64_t BMFi_hashmap_find_insert(const BMFi_Hashmap *h, uint64_t key) {
    uint64_t hash = BMFi_hash_int(key, h->capacity);
    uint64_t step = BMFi_second_hash_int(key, h->capacity);

    uint64_t j = hash;
    uint64_t first_deleted = UINT64_MAX;

    for (size_t i = 0; i < h->capacity; ++i) {
        if (h->keys[j] == BMFi_HASHMAP_DUMMY)
            return j;

        if (h->keys[j] == key)
            return j;
        j = (j + step) % h->capacity;
    }

    return first_deleted;
}

//Resizes the hashmap to a new size
void BMFi_hashmap_resize(BMFi_Hashmap *h, size_t newCap) {
    //Save the old values for rehashing:
    uint64_t *oldKeys = h->keys;
    uint32_t *oldVals = h->values;
    size_t oldCap = h->capacity;

    //Update the capcity to the new value:
    h->capacity = newCap;
    h->size = 0; //Reset the size to 0 as it will be naturally incremented in add()

    //Create the new arrays with the new capacity
    h->keys = malloc(sizeof(uint64_t) * newCap);
    memset(h->keys, 0xFF, sizeof(uint64_t) * newCap);
    h->values = malloc(sizeof(uint32_t) * newCap);
    memset(h->values, 0xFF, sizeof(uint32_t) * newCap);

    //Rehash and reinsert all entries from the old table into the new one
    for(size_t i = 0; i < oldCap; i++) {
        if(oldKeys[i] != BMFi_HASHMAP_DUMMY) {
            BMFi_hashmap_add(h, oldKeys[i], oldVals[i]);
        }
    }
    free(oldKeys);
    free(oldVals);
}

//Gets a value from a int_hashmap
uint32_t BMFi_hashmap_get(BMFi_Hashmap *h, uint64_t key) {
    uint64_t j = BMFi_hashmap_find(h, key);
    if(j == BMFi_HASHMAP_DUMMY) return UINT32_MAX;
    return h->values[j];
}

//Removes a kvp from the int_hashmap and returns its value
uint32_t BMFi_hashmap_remove(BMFi_Hashmap *h, uint64_t key) {
    uint64_t j = BMFi_hashmap_find(h, key);
    if(j == BMFi_HASHMAP_DUMMY) return UINT32_MAX;

    uint32_t val = h->values[j];
    h->keys[j] = BMFi_HASHMAP_DUMMY;
    h->values[j] = UINT32_MAX;
    h->size--;
    return val;
}

//Adds a kvp to the int_hashmap, replacing the value if the key already exists in the hashmap
uint32_t BMFi_hashmap_add(BMFi_Hashmap *h, uint64_t key, uint32_t value) {
    uint64_t j = BMFi_hashmap_find_insert(h, key);
    if(j == BMFi_HASHMAP_DUMMY) return UINT32_MAX;
    if (h->keys[j] == key) {
        uint32_t old = h->values[j];
        h->values[j] = value;
        return old;
    }

    h->keys[j] = key;
    h->values[j] = value;
    h->size++;

    if (h->size * 4 >= h->capacity * 3)
        BMFi_hashmap_resize(h, BMFi_next_prime(h->capacity * 2));
    return UINT32_MAX;
}

void BMFi_hashmap_free(BMFi_Hashmap *h) {
    if(h->keys) free(h->keys);
    h->keys = NULL;
    if(h->values) free(h->values);
    h->values = NULL;
}

//Reads the entirety of a file into the given buffer
int BMFi_read_to_end(char const *path, uint8_t **buf, uint8_t add_null) {
    FILE *fp;
    size_t fsz;
    long offEnd;
    int rc;

    //Open the file
    fp = fopen(path, "rb");
    if(NULL == fp) {
        return -1;
    }

    //Seek to the end of the file
    rc = fseek(fp, 0L, SEEK_END);
    if(0 != rc) {
        return -1;
    }

    //Byte offset to the end of the file size
    if(0 > (offEnd = ftell(fp))) {
        return -1;
    }
    fsz = (size_t)offEnd;

    //Allocate a buffer to hold the whole file
    *buf = malloc(fsz + (int)add_null);
    if(NULL == *buf) {
        return -1;
    }

    //Rewind file pointer to the start of the file:
    rewind(fp);

    //Place the file into a buffer
    if(fsz != fread(*buf, 1, fsz, fp)) {
        free(*buf);
        return -1;
    }

    //Close the file
    if(EOF == fclose(fp)) {
        free(*buf);
        return -1;
    }

    //Add null terminator
    if(add_null) {
        (*buf)[fsz] = 0;
    }

    return fsz;
}

//TODO: Get errors working for the parser
typedef struct {
    uint32_t error_line;
    uint32_t error_col;
    char error_char;
} BMFi_Parse_Error_Data;

BMFi_Parse_Error_Data error_data = {0};

void BMFi_append_glyph(BMFont *font, BMF_Glyph g) {
    if(font->glyph_count >= font->glyph_capacity) {
        size_t new_cap = (font->glyph_capacity > 0) ? font->glyph_capacity * 2 : 16;
        font->glyphs = realloc(font->glyphs, new_cap);
        font->glyph_capacity = new_cap;
    }

    BMFi_hashmap_add(font->glyph_map, g.codepoint, font->glyph_count);
    font->glyphs[font->glyph_count++] = g;
}
void BMFi_append_kerning(BMFont *font, BMF_Kerning k) {
    if(font->kerning_count >= font->kerning_capacity) {
        size_t new_cap = (font->kerning_capacity > 0) ? font->kerning_capacity * 2 : 16;
        font->kernings = realloc(font->kernings, new_cap);
        font->kerning_capacity = new_cap;
    }

    BMFi_hashmap_add(font->kerning_map, ((uint64_t)k.first << 32) | k.second, font->kerning_count);
    font->kernings[font->kerning_count++] = k;
}

uint8_t BMFi_parse_char(char *line, BMF_Glyph *g) {
    while(*line) {
        while(*line && isspace((unsigned char)*line)) line++;
        char *eq = strchr(line, '=');
        if (!eq) break;
        *eq = '\0';

        char *key = line;
        char *end;
        int value = strtol(eq + 1, &end, 10);

        if(!strcmp("id", key)) g->codepoint = value;
        else if(!strcmp("x", key)) g->sub_rect.x = value;
        else if(!strcmp("y", key)) g->sub_rect.y = value;
        else if(!strcmp("width", key)) g->sub_rect.w = value;
        else if(!strcmp("height", key)) g->sub_rect.h = value;
        else if(!strcmp("xoffset", key)) g->x_offset = value;
        else if(!strcmp("yoffset", key)) g->y_offset = value;
        else if(!strcmp("xadvance", key)) g->x_advance = value;
        else if(!strcmp("page", key)) g->page = value;
        else if(!strcmp("chnl", key)) g->channel = value;
        else return 0;

        line = end;
    }
    return 1;
}
uint8_t BMFi_parse_count(char *line, size_t *num_chars) {
    size_t tag_count = 0;

    while(*line) {
        if(tag_count > 0) return 0; //Must only be one attribute
        while(*line && isspace((unsigned char)*line)) line++;
        char *eq = strchr(line, '=');
        if (!eq) break;
        *eq = '\0';

        char *key = line;
        char *end;
        int value = strtol(eq + 1, &end, 10);

        if(!strcmp("count", key)) *num_chars = value;
        else return 0;

        line = end;

        tag_count++;
    }
    return 1;
}
uint8_t BMFi_parse_kerning(char *line, BMF_Kerning *k) {
    while(*line) {
        while(*line && isspace((unsigned char)*line)) line++;
        char *eq = strchr(line, '=');
        if (!eq) break;
        *eq = '\0';

        char *key = line;
        char *end;
        int value = strtol(eq + 1, &end, 10);

        if(!strcmp("first", key)) k->first = value;
        else if(!strcmp("second", key)) k->second = value;
        else if(!strcmp("amount", key)) k->amount = value;
        else return 0;

        line = end;
    }
    return 1;
}
uint8_t BMFi_parse_common(char *line, BMFont *font) {
    while(*line) {
        while(*line && isspace((unsigned char)*line)) line++;
        char *eq = strchr(line, '=');
        if (!eq) break;
        *eq = '\0';

        char *key = line;
        char *end;
        int value = strtol(eq + 1, &end, 10);

        if(!strcmp("lineHeight", key)) font->line_height = value;
        else if(!strcmp("base", key)) font->base = value;
        //None of the others are implemented for now
        else if(!strcmp("scaleW", key)) {}
        else if(!strcmp("scaleH", key)) {}
        else if(!strcmp("pages", key)) {}
        else if(!strcmp("packed", key)) {}
        else if(!strcmp("alphaChnl", key)) {}
        else if(!strcmp("redChnl", key)) {}
        else if(!strcmp("greenChnl", key)) {}
        else if(!strcmp("blueChnl", key)) {}
        else return 0;

        line = end;
    }
    return 1;
}

uint8_t BMFi_parse_line(char *line, BMFont *font) {
    char *space = strchr(line, ' ');
    if (!space) return 0;

    *space = '\0';

    char *tag = line;
    char *rest = space + 1;

    if(!strcmp("info", tag)) return 1;
    else if(!strcmp("page", tag)) return 1; //Skip these two lines
    else if(!strcmp("common", tag)) return BMFi_parse_common(rest, font);
    else if(!strcmp("char", tag)) {
        BMF_Glyph g;
        if(BMFi_parse_char(rest, &g)) {
            BMFi_append_glyph(font, g);
            return 1;
        }
        return 0;
    }
    else if(!strcmp("chars", tag)) {
        if(BMFi_parse_count(rest, &font->glyph_capacity)) {
            font->glyph_map = malloc(sizeof(BMFi_Hashmap));
            if(!BMFi_hashmap_init(font->glyph_capacity, font->glyph_map)) return 0;
            return 1;
        }
        return 0;
    }
    else if(!strcmp("kerning", tag)) {
        BMF_Kerning k;
        if(BMFi_parse_kerning(rest, &k)) {
            BMFi_append_kerning(font, k);
            return 1;
        }
        return 0;
    }
    else if(!strcmp("kernings", tag)) {
        if(BMFi_parse_count(rest, &font->kerning_capacity)) {
            font->kerning_map = malloc(sizeof(BMFi_Hashmap));
            if(!BMFi_hashmap_init(font->kerning_capacity, font->kerning_map)) return 0;
            return 1;
        }
        return 0;
    }
    else return 0;
}

uint8_t BMFi_parse_text(BMFont *font, uint8_t *data, size_t data_sz) {
    char *line = strtok((char *)data, "\r\n");

    while(line) {
        if(!BMFi_parse_line(line, font)) return 0;
        line = strtok(NULL, "\r\n");
    }

    return 1;
}

//Need to pack these structs since the data itself is packed
#pragma pack(push,1)
typedef struct {
    uint16_t line_height;
    uint16_t base;
    uint16_t scale_w;
    uint16_t scale_h;
    uint16_t pages;
    uint8_t bitfield;
    uint8_t alpha;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} BMFi_BMF_Common_Block;
#pragma pack(pop)
uint8_t BMFi_parse_common_block(BMFont *font, uint8_t *data, size_t data_sz) {
    if(data_sz != sizeof(BMFi_BMF_Common_Block)) {
        printf("ERROR: Incorrect Common Block size\n");
        return 0;
    }

    BMFi_BMF_Common_Block block;
    memcpy(&block, data, sizeof(BMFi_BMF_Common_Block));
    font->line_height = block.line_height;
    font->base = block.base;
    return 1;
}
//Need to pack these structs since the data itself is packed
#pragma pack(push,1)
typedef struct {
    uint32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t x_offset;
    int16_t y_offset;
    int16_t x_advance;
    uint8_t page;
    uint8_t channel;
} BMFi_BMF_Chars_Block;
#pragma pack(pop)
uint8_t BMFi_parse_chars_block(BMFont *font, uint8_t *data, size_t data_sz) {
    if(data_sz % sizeof(BMFi_BMF_Chars_Block) != 0) {
        printf("ERROR: Incorrect Char Block size\n");
        return 0;
    }

    size_t num_chars = data_sz / sizeof(BMFi_BMF_Chars_Block);
    font->glyph_capacity = num_chars;
    font->glyph_map = malloc(sizeof(BMFi_Hashmap));
    if(!BMFi_hashmap_init(font->glyph_capacity, font->glyph_map)) return 0;

    for(size_t i = 0; i < num_chars; i++) {
        BMFi_BMF_Chars_Block block;
        memcpy(&block, data, sizeof(BMFi_BMF_Chars_Block));
        BMFi_append_glyph(font, (BMF_Glyph){block.id, (BMF_Quad){block.x, block.y, block.width, block.height}, block.x_offset, block.y_offset, block.x_advance, block.page, block.channel});

        data += sizeof(BMFi_BMF_Chars_Block);
    }

    return 1;
}
//Need to pack these structs since the data itself is packed
#pragma pack(push,1)
typedef struct {
    uint32_t first;
    uint32_t second;
    int16_t amount;
} BMFi_BMF_Kernings_Block;
#pragma pack(pop)
uint8_t BMFi_parse_kernings_block(BMFont *font, uint8_t *data, size_t data_sz) {
    if(data_sz % sizeof(BMFi_BMF_Kernings_Block) != 0) {
        printf("ERROR: Incorrect Kerning Block size\n");
        return 0;
    }

    size_t num_kernings = data_sz / sizeof(BMFi_BMF_Kernings_Block);
    font->kerning_capacity = num_kernings;
    font->kerning_map = malloc(sizeof(BMFi_Hashmap));
    if(!BMFi_hashmap_init(font->kerning_capacity, font->kerning_map)) return 0;

    for(size_t i = 0; i < num_kernings; i++) {
        BMFi_BMF_Kernings_Block block;
        memcpy(&block, data, sizeof(BMFi_BMF_Kernings_Block));
        BMFi_append_kerning(font, (BMF_Kerning){block.first, block.second, block.amount});

        data += sizeof(BMFi_BMF_Kernings_Block);
    }

    return 1;
}

uint8_t BMFi_parse_binary(BMFont *font, uint8_t *data, size_t data_sz) {
    if(data_sz < 4 || data[0] != 'B' || data[1] != 'M' || data[2] != 'F' || data[3] != 3) {
        printf("ERROR: Unsupported format\n");
        return 0;
    }

    uint8_t *ptr = data + 4;
    uint8_t *end = data + data_sz;

    while(ptr + 5 <= end) {
        uint8_t block_type = *ptr++;
        uint32_t block_sz;
        memcpy(&block_sz, ptr, sizeof(block_sz));
        ptr += 4;

        if (ptr + block_sz > end) {
            printf("ERROR: Corrupt BMF file\n");
            return 0;
        }

        switch (block_type) {
            case 1: break; //Info block. Do not need to parse
            case 2: //Common block
                if(!BMFi_parse_common_block(font, ptr, block_sz)) return 0;
                break;
            case 3: break; //Pages block. Do not need to parse;
            case 4: //Chars block
                if(!BMFi_parse_chars_block(font, ptr, block_sz)) return 0;
                break;
            case 5: //Kernings block
                if(!BMFi_parse_kernings_block(font, ptr, block_sz)) return 0;
                break;
            default:
                printf("ERROR: NON-Existent BMF Binary Block type\n");
                return 0;
        }

        ptr += block_sz;
    }

    return 1;
}

uint8_t BMF_load_font(const char *font_path, BMF_Format format, BMFont *font) {
    uint8_t *buf;
    int size = BMFi_read_to_end(font_path, &buf, 1);
    if(size < 0) {
        return 0;
    }

    uint8_t res = (format == BMF_TEXT) ? BMFi_parse_text(font, buf, size) : BMFi_parse_binary(font, buf, size);
    free(buf);
    if(!res) {
        *font = (BMFont){0}; //Clear all of the initially assigned font data
        return 0;
    }

    return 1;
}

void BMF_print_parsing_error(void) {
    printf("Error Line: %u\nError Column: %u\nError Char: %c\n", error_data.error_line, error_data.error_col, error_data.error_char);
}

void BMF_font_free(BMFont *font) {
    if(font->glyphs) free(font->glyphs);
    if(font->kernings) free(font->kernings);
    if(font->glyph_map) {
        BMFi_hashmap_free(font->glyph_map);
        free(font->glyph_map);
    }
    if(font->kerning_map) {
        BMFi_hashmap_free(font->kerning_map);
        free(font->kerning_map);
    }
    *font = (BMFont){0}; //Clear the data
}
#endif
