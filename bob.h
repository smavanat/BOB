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

//TODO: Create an internal struct to manage state - And actually make removing an object associated with a handle invalidate that object in the internal storage
//      Proper bitmap font support
//      Make it so that unfilled shapes don't have the full outline drawn when clipped
//      Debug mode/Release mode building (turning asserts on and off)
//      Figure out whether some functions will return error codes or not e.g. BOB_draw_char
//      Add an arena to manage the total memory easily and get rid of BOB_MEMSET and BOB_MEMCPY since these should all take place within the arena
//      Vulkan support
//      Allow the user to define render passes -> Custom framebuffers
//      Maybe draw call sorting internally rather than relying on the depth buffer?
//      Maybe allow custom vertex layout?
typedef struct {
    float m[4][4];
} BOB_Mat4;

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
#define BOB_MAX_VERTEX_CAPACITY 1048576
#define BOB_MAX_INDEX_CAPACITY 2097152
#define BOB_MAX_TEX_CAPACITY 32
#define BOB_MAX_ATLAS_CAPACITY 16
#define BOB_MAX_PIXELBUFFER_CAPACITY 8
#define BOB_MAX_MATERIAL_CAPACITY 16
#define INIT_STACK_CAPACITY 64

void BOB_init(void);
void BOB_terminate(void);

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
   float x, y, z;
} BOB_Vector3;

typedef struct {
    float x, y, z, w;
} BOB_Vector4;

//Data structure to hold data about a single render vertex
typedef struct {
    BOB_Vector4 colour; //The colour of the vertex
    BOB_Vector3 pos; //The on-screen position of the render vertex
    BOB_Vector2 uv; //The (u,v) coordinates of the vertex
} BOB_Render_Vertex;

typedef uint32_t BOB_Texture_Handle;
typedef uint32_t BOB_Material_Handle;
typedef uint32_t BOB_Atlas_Handle;
typedef uint32_t BOB_PixelBuffer_Handle;
typedef uint32_t BOB_Uniform_Handle;

typedef enum {
    BOB_RED,
    BOB_RG,
    BOB_RGB,
    BOB_RGBA
} BOB_Format;

typedef struct {
    uint32_t texture, width, height;
} BOB_Texture;

//Creates a new texture on the gpu
BOB_Texture_Handle BOB_create_texture(uint32_t width, uint32_t height, uint8_t *data, BOB_Format format);
void BOB_remove_texture(BOB_Texture_Handle tex);

typedef struct {
    uint8_t *pixel_buf; //Array of pixel data
    size_t buf_sz;
    uint32_t pbo; //pbo this renderer uses
    uint32_t pixel_tex; //The texture the pbo is rendered to
} BOB_PixelBuffer;

//Creates a pixel buffer to hold the pixels representing a texture of size width * height
BOB_PixelBuffer_Handle BOB_pixelbuffer_init(size_t width, size_t height, BOB_Format format);
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer_Handle pb);
//Draws a frame straight to a texture by uploading it to a pixel buffer
void BOB_pixelbuffer_updload_data(BOB_PixelBuffer_Handle pb, uint8_t *data);

typedef struct {
    BOB_Texture_Handle texture; //GL index of the atlas texture
    uint32_t cursor_x, cursor_y; //Current packing position
    uint32_t row_height; //Height of the tallest texture on the current row
    BOB_Format format; //Pixel format of the texture
} BOB_TextureAtlas;

//Initialises a blank texture atlas
BOB_Atlas_Handle BOB_atlas_init(uint32_t width, uint32_t height, BOB_Format format);
//Returns the UV rect where the texture was placed
//TODO: Add a way of returning an error if there was no space to add new stuff
BOB_Quad BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h);
//Frees a texture atlas
void BOB_atlas_free(BOB_Atlas_Handle a);

typedef enum {
    BOB_VERTEX_SHADER,
    BOB_FRAGMENT_SHADER,
    BOB_TESS_CTRL_SHADER,
    BOB_TESS_EVAL_SHADER,
    BOB_COMPUTE_SHADER,
    BOB_GEOMETRY_SHADER
} BOB_Shader_Type;

//NOTE: string passed as shader_code must be null-terminated
typedef struct {
    const char *shader_code;
    BOB_Shader_Type type;
} BOB_Shader_Data;

typedef enum {
    BOB_UNIFORM_FLOAT,
    BOB_UNIFORM_UNSIGNED_INT,
    BOB_UNIFORM_SIGNED_INT,
    BOB_UNIFORM_VEC2,
    BOB_UNIFORM_VEC3,
    BOB_UNIFORM_VEC4,
    BOB_UNIFORM_TEXTURE,
    BOB_UNIFORM_MAT4,
} BOB_Uniform_Type;

typedef struct {
    const char *name;
    union {
        float f;
        uint32_t u32;
        int32_t i32;
        BOB_Vector2 vec2;
        BOB_Vector3 vec3;
        BOB_Vector4 vec4;
        BOB_Texture_Handle tex_index;
        BOB_Mat4 mat4;
        const void *ptr;
    };

    BOB_Uniform_Type type;
    int32_t location;
    uint8_t is_reference;
} BOB_Uniform;

#define BOB_uniform_float(u_name, value) (BOB_Uniform){.name = (u_name), .f = (value), .type = BOB_UNIFORM_FLOAT, .is_reference = 0}
#define BOB_uniform_unsigned_int(u_name, value) (BOB_Uniform){.name = (u_name), .u32 = (value), .type = BOB_UNIFORM_UNSIGNED_INT, .is_reference = 0}
#define BOB_uniform_signed_int(u_name, value) (BOB_Uniform){.name = (u_name), .i32 = (value), .type = BOB_UNIFORM_SIGNED_INT, .is_reference = 0}
#define BOB_uniform_vector2(u_name, value) (BOB_Uniform){.name = (u_name), .vec2 = (value), .type = BOB_UNIFORM_VEC2, .is_reference = 0}
#define BOB_uniform_vector3(u_name, value) (BOB_Uniform){.name = (u_name), .vec3 = (value), .type = BOB_UNIFORM_VEC3, .is_reference = 0}
#define BOB_uniform_vector4(u_name, value) (BOB_Uniform){.name = (u_name), .vec4 = (value), .type = BOB_UNIFORM_VEC4, .is_reference = 0}
#define BOB_uniform_texture(u_name, value) (BOB_Uniform){.name = (u_name), .tex_index = (value), .type = BOB_UNIFORM_TEXTURE, .is_reference = 0}
#define BOB_uniform_mat4(u_name, value) (BOB_Uniform){.name = (u_name), .mat4 = (value), .type = BOB_UNIFORM_MAT4, .is_reference = 0}
#define BOB_uniform_float_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_FLOAT, .is_reference = 1}
#define BOB_uniform_unsigned_int_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_UNSIGNED_INT, .is_reference = 1}
#define BOB_uniform_signed_int_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_SIGNED_INT, .is_reference = 1}
#define BOB_uniform_vector2_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC2, .is_reference = 1}
#define BOB_uniform_vector3_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC3, .is_reference = 1}
#define BOB_uniform_vector4_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC4, .is_reference = 1}
#define BOB_uniform_texture_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_TEXTURE, .is_reference = 1}
#define BOB_uniform_mat4_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_MAT4, .is_reference = 1}

BOB_Uniform_Handle get_uniform(BOB_Material_Handle mat, char *name);

typedef struct {
    BOB_Uniform *uniforms;
    size_t uniform_count;
    uint32_t shader;
} BOB_Material;

BOB_Material_Handle BOB_create_material(BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms);
void BOB_destroy_material(BOB_Material_Handle mat);

void BOB_set_material_float(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float value);
void BOB_set_material_unsigned_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t value);
void BOB_set_material_signed_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t value);
void BOB_set_material_vector2(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 value);
void BOB_set_material_vector3(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 value);
void BOB_set_material_vector4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 value);
void BOB_set_material_texture(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle value);
void BOB_set_material_mat4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 value);
void BOB_set_material_float_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float *value);
void BOB_set_material_unsigned_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t *value);
void BOB_set_material_signed_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t *value);
void BOB_set_material_vector2_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 *value);
void BOB_set_material_vector3_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 *value);
void BOB_set_material_vector4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 *value);
void BOB_set_material_texture_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle *value);
void BOB_set_material_mat4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 *value);

//Represents a single batch sent off in a draw call from a texture atlas
typedef struct {
    BOB_Render_Vertex *vertex_data;
    uint32_t *index_data; //The index count (for ebo) for this renderer
    size_t vertex_size;
    size_t index_size;
    size_t vertex_count;
    size_t index_count;
    uint8_t init;
} BOB_RenderBatch;

typedef enum {
    BOB_CLIP_HORZ,
    BOB_CLIP_VERT,
    BOB_CLIP_BOTH,
} BOB_Clip_Dir;

typedef struct {
    float left, right, top, bottom;
    //As much as I would like to compress these into one uint8_t and extract the bits using enum flags,
    //that would not provide any tangible benefit as the space would still be used by this struct
    //due to the alignment of this struct being 4 bytes
    uint8_t clip_vert, clip_horz, empty;
} BOBi_Clip_Rect;

typedef struct {
    BOBi_Clip_Rect *elems;
    size_t size;
    size_t capacity;
} BOBi_Clip_Stack;

typedef struct {
    // BOB_Render_Layer layer; //Layers of rendering
    BOBi_Clip_Stack *stack; //Stores the current clipping rect and the history
    BOB_Mat4 projection; //projection matrix for this renderer
    uint32_t vao; //vao this renderer uses
    uint32_t vbo; //vbo this renderer uses
    uint32_t ebo; //ebo this renderer uses
    BOB_Material_Handle default_mat; //Default material this renderer uses

    uint32_t screen_height;
    uint32_t screen_width;
} BOB_Renderer;

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer *r, BOB_Quad rect, BOB_Clip_Dir dir);
//Removes the first clipping intersection from the stack and returns its value
void BOB_end_clip(BOB_Renderer *r);

//Initialises the pixel renderer
BOB_Renderer BOB_renderer_init(size_t width, size_t height);
//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r);
//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r);
//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer *r);
//Updates the dimensions of the screen that the renderer renders to.
//Updates projection matrix
void BOB_renderer_update_dimensions(BOB_Renderer *r, uint32_t width, uint32_t height);

//Draws a quad
void BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, BOB_Atlas_Handle atlas, float depth, float rotation);
//Draws a dynamically allocated texture
void BOB_draw_texture(BOB_Renderer *r, uint32_t texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth, float rotation);
//Draws a pixel buffer
void BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth, float rotation);
//Draws a filled circle
void BOB_draw_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, float depth);
//Draws a filled quad
void BOB_draw_quad(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, float depth, float rotation);
//Draws a filled triangle
void BOB_draw_polygon(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float depth, float rotation);
//Draws an unfilled circle
void BOB_draw_unfilled_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, float depth);
//Draws an unfilled quad
void BOB_draw_unfilled_quad(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, float depth, float rotation);
//Draws an unfilled triange
void BOB_draw_unfilled_polygon(BOB_Renderer *r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, float depth, float rotation);
//Draws a line between two points
void BOB_draw_line(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, float depth);

//Draws a quad with a specified material
void BOB_draw_atlas_quad_mat(BOB_Renderer *r, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, BOB_Atlas_Handle atlas, float depth, float rotation, BOB_Material_Handle mat);
//Draws a dynamically allocated texture with a specified material
void BOB_draw_texture_mat(BOB_Renderer *r, uint32_t texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth, float rotation, BOB_Material_Handle mat);
//Draws a pixel buffer with a specified material
void BOB_draw_pixel_buffer_mat(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth, float rotation, BOB_Material_Handle mat);
//Draws a filled circle with a specified material
void BOB_draw_circle_mat(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, float depth, BOB_Material_Handle mat);
//Draws a filled quad with a specified material
void BOB_draw_quad_mat(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, float depth, float rotation, BOB_Material_Handle mat);
//Draws a filled triangle with a specified material
void BOB_draw_polygon_mat(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float depth, float rotation, BOB_Material_Handle mat);
//Draws an unfilled circle with a specified material
void BOB_draw_unfilled_circle_mat(BOB_Renderer *r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, float depth, BOB_Material_Handle mat);
//Draws an unfilled quad with a specified material
void BOB_draw_unfilled_quad_mat(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, float depth, float rotation, BOB_Material_Handle mat);
//Draws an unfilled triange with a specified material
void BOB_draw_unfilled_polygon_mat(BOB_Renderer *r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, float depth, float rotation, BOB_Material_Handle mat);
//Draws a line between two points with a specified material
void BOB_draw_line_mat(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, float depth, BOB_Material_Handle mat);

//Determines the projection matrix
void BOB_ortho(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest);
//Clears the collur of the screen
void BOB_clear_colour(BOB_Vector4 colour);
//Converts an angle in degrees to radians
float BOB_degrees_to_radians(float angle);

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

uint8_t BOB_draw_char(BOB_Renderer *r, BOB_Bitmap_Font*bf, char c, BOB_Quad dimensions, BOB_Vector4 colour, float depth);
uint8_t BOB_draw_string(BOB_Renderer *r, BOB_Bitmap_Font*bf, const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 start, BOB_Vector2 scale, BOB_Vector4 colour, float depth);
BOB_Vector2 BOB_measure_text(const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 scale);


#endif //BOB_H
