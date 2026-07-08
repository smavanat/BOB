#ifndef BOB_H
#define BOB_H
#include <stdint.h>
#include <stddef.h>

#ifndef GLAD_PATH
#define GLAD_PATH "glad.h"
#endif //GLAD_PATH

#include GLAD_PATH

#ifndef BOB_MALLOC
#include <stdlib.h>
#define BOB_MALLOC malloc
#define BOB_MEMSET memset
#define BOB_FREE free
#endif //BOB_MALLOC

#ifndef BOB_ASSERT
#include <assert.h>
#define BOB_ASSERT assert
#endif //BOB_ASSERT

#ifndef BOB_MEMCPY
#include <string.h>
#define BOB_MEMCPY memcpy
#endif //BOB_MEMCPY

#ifndef BOB_PRINT
#include <stdio.h>
#define BOB_PRINT printf
#endif //BOB_PRINT

//TODO: Drawing triangles and circles
//      Proper bitmap font support
//      Support Custom Shaders
//      Debug mode/Release mode building (turning asserts on and off)
//      Figure out whether some functions will return error codes or not e.g. BOB_draw_char
//      Add an arena to manage the total memory easily and get rid of BOB_MEMSET and BOB_MEMCPY
//      since these should all take place within the arena
//      Vulkan support
typedef float BOB_Mat4[4][4];

typedef struct {
    uint8_t *pixel_buf; //Array of pixel data
    size_t buf_sz;
    size_t width, height;
    uint32_t pbo; //pbo this renderer uses
    uint32_t pixel_tex; //The texture the pbo is rendered to
} BOB_PixelBuffer;

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
BOB_PixelBuffer BOB_pixelbuffer_init(size_t width, size_t height, uint8_t pixel_size);
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer *pb);

//Arbitrary constants for now
#define BOB_INIT_TRIANGLES 2048
#define BOB_INIT_QUADS 4096
#define BOB_MAX_LAYERS 64

#define BOB_VERTICIES_PER_QUAD 4
#define BOB_VERTICIES_PER_TRIANGLE 3
#define BOB_INDECIES_PER_QUAD 6
#define BOB_INDECIES_PER_TRIANGLE 3
#define BOB_INIT_VERTEX_CAPACITY BOB_INIT_QUADS * BOB_VERTICIES_PER_QUAD + BOB_INIT_TRIANGLES * BOB_VERTICIES_PER_TRIANGLE
#define BOB_INIT_INDEX_CAPACITY BOB_INIT_QUADS * BOB_INDECIES_PER_QUAD + BOB_INIT_TRIANGLES * BOB_INDECIES_PER_TRIANGLE
#define BOB_INVALID_TEX_INDEX 1248
#define BOB_CIRCLE_LINE_SEGMENTS 64 //Number of line segments that make up the circumference of a circle

typedef struct {
    float x;
    float y;
    float w;
    float h;
} BOB_Quad;

typedef struct {
    float x, y;
} BOB_Vector2;

typedef struct {
    float x, y, z, w;
} BOB_Vector4;

//Data structure to hold data about a single render vertex
typedef struct {
    BOB_Vector2 pos; //The on-screen position of the render vertex
    BOB_Vector4 colour; //The colour of the vertex
    BOB_Vector2 uv; //The (u,v) coordinates of the vertex
} BOB_Render_Vertex;

typedef struct {
    uint32_t texture; //GL index of the atlas texture
    uint32_t width, height; //Dimensions of the atlas
    uint32_t cursor_x, cursor_y; //Current packing position
    uint32_t row_height; //Height of the tallest texture on the current row
    uint8_t pixel_size; //Number of bytes a pixel in the atlas takes up. Must be either 3 or 4
} BOB_TextureAtlas;

//Initialises a texture atlas
//Optionally packs a single white pixel at the start of the texture atlas to render a solid quad
BOB_TextureAtlas BOB_atlas_init(uint32_t width, uint32_t height, uint32_t texture, uint8_t pixel_size);
BOB_TextureAtlas BOB_atlas_init_blank(uint32_t width, uint32_t height, uint8_t pixel_size);
//Returns the UV rect where the texture was placed
//pixel_size must be either 3 or 4. If it does not match the pixel size of the atlas,
//an empty quad will returned as the pixel formats are different
BOB_Quad BOB_atlas_pack(BOB_TextureAtlas *a, uint8_t* pixels, size_t w, size_t h, uint8_t pixel_size);
void BOB_atlas_free(BOB_TextureAtlas *a);

//Represents a single batch sent off in a draw call from a texture atlas
typedef struct {
    BOB_Render_Vertex *vertex_data;
    uint32_t *index_data; //The index count (for ebo) for this renderer
    BOB_TextureAtlas *a;
    size_t vertex_size;
    size_t index_size;
    size_t vertex_count;
    size_t index_count;
} BOB_AtlasRenderBatch;

//Struct for renderering one off textures
typedef struct {
    uint32_t texture;
    BOB_Quad dimensions;
    BOB_Quad uv;
    BOB_Vector4 colour;
} BOB_DynamicTexture;

typedef struct {
    BOB_AtlasRenderBatch *atlas_batches;
    BOB_DynamicTexture *dynamic_textures;
    size_t dynamic_texture_count;
    size_t dynamic_texture_capacity;
    int earliest_atlas_used; //If negative, this layer is not used, otherwise is the index of the earliest atlas used
} BOB_Render_Layer;

typedef struct {
    float left, right, top, bottom;
    //As much as I would like to compress these into one uint8_t and extract the bits using enum flags,
    //that would not provide any tangible benefit as the space would still be used by this struct
    //due to the alignment of this struct being 4 bytes
    uint8_t clip_vert, clip_horz, empty;
} BOB_Clip_Rect;

typedef struct {
    BOB_Clip_Rect *elems;
    size_t size;
    size_t capacity;
} BOB_Clip_Stack;

#define INIT_STACK_CAPACITY 64

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_push_clip_rect(BOB_Clip_Stack *stack, BOB_Clip_Rect rect);
//Removes the first clipping intersection from the stack and returns its value
BOB_Clip_Rect BOB_pop_clip_rect(BOB_Clip_Stack* stack);
//Return the value of the element at the top of the stack without popping it
#define BOB_peek_clip_rect(stack) (((stack)->size > 0) ? (stack)->elems[(stack)->size-1] : (BOB_Clip_Rect){0})

//Pixel renderer that renders a single frame
typedef struct {
    BOB_Render_Layer layers[BOB_MAX_LAYERS]; //Layers of rendering
    BOB_Clip_Stack *stack; //Stores the current clipping rect and the history
    BOB_Mat4 projection; //projection matrix for this renderer
    uint32_t vao; //vao this renderer uses
    uint32_t vbo; //vbo this renderer uses
    uint32_t ebo; //ebo this renderer uses
    uint32_t shader; //shader this renderer uses
    size_t atlas_batch_capacity;
    size_t num_atlas_batches;

    uint32_t screen_height;
    uint32_t screen_width;
} BOB_Renderer;

#define BOB_GET_ATLAS_BATCH(r, layer, i) (r)->layers[layer].atlas_batches[i]

//Initialises the pixel renderer
BOB_Renderer BOB_renderer_init(size_t width, size_t height);
//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r);
//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r);
//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer *r);
//Adds a texture atlas to the render's context and returns a reference to
//use the texture atlas by
//TODO: Add a way to return a failure value to this
//      Add a way to remove an atlas efficiently
uint32_t BOB_add_texture_atlas(BOB_Renderer *r, BOB_TextureAtlas *ta);
//Draws a quad
void BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint32_t atlas, uint8_t layer);
//Draws a dynamically allocated texture
void BOB_draw_texture(BOB_Renderer *r, uint32_t texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint8_t layer);
//Draws a frame straight to a texture by uploading it to a pixel buffer
void BOB_pixelbuffer_updload_data(BOB_PixelBuffer *pb, uint8_t *data);
//Draws a pixel buffer
void BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer *pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint8_t layer);
//Draws a filled circle
void BOB_draw_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint8_t layer);
//Draws a filled quad
void BOB_draw_quad(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, uint8_t layer);
//TODO:Draws an unfilled circle
void BOB_draw_unfilled_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint8_t layer);
//Draws an unfilled quad
void BOB_draw_unfilled_quad(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint8_t layer);
//Draws a line between two points
void BOB_draw_line(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint8_t layer);
void BOB_draw_quad_bordered(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 q_col, BOB_Vector4 b_col, float thick, uint8_t layer);

//Determines the projection matrix
void BOB_ortho(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 dest);

//TODO: support the .fnt metadata from AngelCode
//      And various other bitmap formats, not just loading from a monospaced png

//Way of telling the bitmap renderer what layout your bitmap uses
typedef enum {
    BOB_BITMAP_STANDARD, //Standard ASCII bitmap with chars from 32 to 126
    BOB_BITMAP_OFFSET, //Some subset of the standard ASCII ordering that still adheres to the original, e.g. having only chars 46 - 97
    BOB_BITMAP_CUSTOM, //A completely custom ordering of chars
    BOB_NUM_BITMAP_LAYOUTS,
} BOB_Bitmap_Layout;

typedef union {
    //Stores the jump table for a custom layout
    //Stores the offsets for a subset layout
    struct {} standard_desc;
    struct {
        size_t start_offset;
        size_t end_offset;
    } offset_desc;
    struct {
        char *data;
        size_t len;
    } custom_desc;
} BOB_Bitmap_Layout_Desc;

typedef struct {
    uint32_t atlas; //Reference to the TextureAtlas used to store the bitmap

    uint32_t tex_pixel_width;
    uint32_t tex_pixel_height;
    uint32_t char_pixel_width;
    uint32_t char_pixel_height;

    uint32_t char_padding_x; //Horizontal padding between chars on the atlas
    uint32_t char_padding_y; //Vertical padding between chars on the atlas
    uint32_t tex_border_padding_x;
    uint32_t tex_border_padding_y;

    BOB_Bitmap_Layout layout;
    BOB_Bitmap_Layout_Desc desc;
} BOB_Bitmap_Font;

uint8_t BOB_bitmap_font_init(BOB_Bitmap_Font*opts, uint32_t atls, uint32_t tpw, uint32_t tph, uint32_t cpw, uint32_t cph, uint32_t cpx, uint32_t cpy, uint32_t tbpx, uint32_t tbpy, BOB_Bitmap_Layout lyt, BOB_Bitmap_Layout_Desc desc);
void BOB_bitmap_font_free(BOB_Bitmap_Font*bf);

uint8_t BOB_draw_char(BOB_Renderer *r, BOB_Bitmap_Font*bf, char c, BOB_Quad dimensions, BOB_Vector4 colour, uint8_t layer);
uint8_t BOB_draw_string(BOB_Renderer *r, BOB_Bitmap_Font*bf, const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 start, BOB_Vector2 scale, BOB_Vector4 colour, uint8_t layer);
BOB_Vector2 BOB_measure_text(const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 scale);


#endif //BOB_H
