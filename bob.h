// MIT License
//
// Copyright (c) 2026 Edwin Hadass
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef BOB_H
#define BOB_H
#include <stdint.h>
#include <stddef.h>

//TODO: Change the vulkan memory code to use our allocator so the PBO memory mapping works properly -> Maybe a ring allocator to ensure constant memory usage
//      Use macros to hide getting index and context from a handle and finding index where next object is placed
//      Do proper error reporting and document what each error code means somewhere
//      Debug mode with statistics
//      Reduce number of memory allocations cpu-side and in the Vulkan backend
//      Allow more customisability in the shaders in general (and add push constants)
//      Use texture arrays instead of binding textures every draw call (and ssbos for shaders (opengl))
//      Allow the user to define render passes -> Custom framebuffers
//      Allow the user to define their own pipeline and sampler layout
//      Allow the user to select what kind of device you want vulkan to use
//      Custom vertex layout
//      Compute shader support

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BOB_INCLUDE_GLAD
#include <glad/glad.h>
#endif

#ifdef BOB_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

//Callback function to create the vulkan surface used to represent the window
typedef uint8_t (*BOB_vk_create_surface)(VkInstance, VkSurfaceKHR *);
#endif

typedef uint64_t BOB_Texture_Handle;
typedef uint64_t BOB_Material_Handle;
typedef uint64_t BOB_Atlas_Handle;
typedef uint64_t BOB_Pixelbuffer_Handle;
typedef uint64_t BOB_Uniform_Handle;
typedef uint64_t BOB_Font_Handle;
typedef uint32_t BOB_Renderer_Handle;

typedef struct {
    float m[4][4];
} BOB_Mat4;

//Constants used by the renderer
#define BOB_VERTICIES_PER_QUAD 4
#define BOB_VERTICIES_PER_TRIANGLE 3
#define BOB_INDECIES_PER_QUAD 6
#define BOB_INDECIES_PER_TRIANGLE 3

//Constants used by the renderer that can be set by the user
#ifndef BOB_CIRCLE_LINE_SEGMENTS
#define BOB_CIRCLE_LINE_SEGMENTS 64 //Number of line segments that make up the circumference of a circle
#endif
#ifndef BOB_MAX_VERTEX_CAPACITY
#define BOB_MAX_VERTEX_CAPACITY 1048576
#endif
#ifndef BOB_MAX_INDEX_CAPACITY
#define BOB_MAX_INDEX_CAPACITY 2097152
#endif
#ifndef BOB_MAX_TEX_CAPACITY
#define BOB_MAX_TEX_CAPACITY 32
#endif
#ifndef BOB_MAX_ATLAS_CAPACITY
#define BOB_MAX_ATLAS_CAPACITY 8
#endif
#ifndef BOB_MAX_PIXELBUFFER_CAPACITY
#define BOB_MAX_PIXELBUFFER_CAPACITY 8
#endif
#ifndef BOB_MAX_MATERIAL_CAPACITY
#define BOB_MAX_MATERIAL_CAPACITY 16
#endif
#ifndef BOB_MAX_FONT_CAPACITY
#define BOB_MAX_FONT_CAPACITY 32
#endif
#ifndef BOB_MAX_DRAW_CALL_CAPACITY
#define BOB_MAX_DRAW_CALL_CAPACITY BOB_MAX_VERTEX_CAPACITY / 3 //Since minimum number of vertices in a draw call is 3
#endif
#ifndef INIT_STACK_CAPACITY
#define INIT_STACK_CAPACITY 64
#endif
#ifndef BOB_MAX_LAYER
#define BOB_MAX_LAYER 1024
#endif
#ifndef BOB_MAX_SHADERS
#define BOB_MAX_SHADERS 32
#endif
#ifndef BOB_MAX_UNIFORMS
#define BOB_MAX_UNIFORMS 64
#endif
#ifndef BOB_MAX_POLY_SIZE
#define BOB_MAX_POLY_SIZE 256
#endif

#ifdef BOB_INCLUDE_GLAD
uint8_t BOB_create_opengl_renderer(size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity, size_t font_capacity,
                                  size_t width, size_t height, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, BOB_Renderer_Handle *renderer);
#endif //BOB_INCLUDE_GLAD
#ifdef BOB_INCLUDE_VULKAN
uint8_t BOB_create_vulkan_renderer(size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity,
                            size_t font_capacity, size_t width, size_t height, size_t width_px, size_t height_px, size_t vertex_capacity,
                            size_t index_capacity, size_t draw_call_capacity, BOB_vk_create_surface surface_func, BOB_Renderer_Handle *r);
#endif //BOB_INCLUDE_VULKAN
void BOB_destroy_renderer(BOB_Renderer_Handle *r);

#ifdef BOB_INCLUDE_GLAD
#ifdef BOB_INCLUDE_VULKAN
uint8_t BOB_init(GLADloadproc proc, const char **required_extensions, size_t num_extensions, size_t num_renderers);
#else
uint8_t BOB_init(GLADloadproc proc, size_t num_renderers);
#endif // BOB_INCLUDE_VULKAN
#else
#ifdef BOB_INCLUDE_VULKAN
uint8_t BOB_init(const char **required_extensions, size_t num_extensions, size_t num_renderers);
#endif //BOB_INCLUDE_VULKAN
#endif //BOB_INCLUDE_GLAD
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

typedef enum {
    BOB_RED,
    BOB_RG,
    BOB_RGB,
    BOB_RGBA
} BOB_Format;

//Creates a new texture on the gpu
uint8_t BOB_create_texture(BOB_Renderer_Handle r, uint32_t width, uint32_t height, uint8_t *data, BOB_Format format, BOB_Texture_Handle *tex);
void BOB_texture_free(BOB_Texture_Handle *tex);

//Creates a pixel buffer to hold the pixels representing a texture of size width * height
uint8_t BOB_pixelbuffer_init(BOB_Renderer_Handle r, size_t width, size_t height, BOB_Format format, BOB_Pixelbuffer_Handle *pb);
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_Pixelbuffer_Handle *pb);
//Binds the pixelbuffers gpu memory to cpu memory. Returns a pointer to the mapped cpu region and its size in bytes
uint8_t BOB_bind_pixelbuffer_memory(BOB_Pixelbuffer_Handle pb, void **mapped_mem_ptr, size_t *mem_sz);
//Unbinds the pixelbuffer's gpu memory from cpu space. Must be called before BOB_pixelbuffer_upload
void BOB_unbind_pixelbuffer_memory(BOB_Pixelbuffer_Handle pb);
//Uploads the pixel data from the pixelbuffer into its associated texture
void BOB_pixelbuffer_upload(BOB_Pixelbuffer_Handle pb);

//Initialises a blank texture atlas
uint8_t BOB_atlas_init(BOB_Renderer_Handle r, uint32_t width, uint32_t height, BOB_Format format, BOB_Atlas_Handle *a);
//Returns the UV rect where the texture was placed
uint8_t BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h, BOB_Quad *out_quad);
//Frees a texture atlas
void BOB_atlas_free(BOB_Atlas_Handle *a);

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
    const char *entrypoint_name;
    size_t code_buf_sz;
    BOB_Shader_Type type;
} BOB_Shader_Data;

//Reads the shader data from a file and creates a shader data object
uint8_t BOB_create_shader_data(const char * shader_path, const char *entrypoint, BOB_Shader_Type type, BOB_Shader_Data *out);
//Destroys a shader data by freeing the shader code bytes and setting the memory region at the pointer to 0
void BOB_destroy_shader_data(BOB_Shader_Data *data);

typedef enum {
    BOB_UNIFORM_FLOAT,
    BOB_UNIFORM_UNSIGNED_INT,
    BOB_UNIFORM_SIGNED_INT,
    BOB_UNIFORM_VEC2,
    BOB_UNIFORM_VEC3,
    BOB_UNIFORM_VEC4,
    BOB_UNIFORM_MAT4,
} BOB_Uniform_Type;

typedef struct {
    const char *name; //Name of the uniform variable
    //Tagged union representing its value
    union {
        float f;
        uint32_t u32;
        int32_t i32;
        BOB_Vector2 vec2;
        BOB_Vector3 vec3;
        BOB_Vector4 vec4;
        BOB_Mat4 mat4;
        const void *ptr;
    } value;
    BOB_Uniform_Type type;
    BOB_Shader_Type shader_stage; //What stage of the pipeline it occurs in
    uint8_t is_reference; //If the value is a pointer to another value (used if the value is updated frequently)
} BOB_Uniform;

#define BOB_uniform_float(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.f = (v), .type = BOB_UNIFORM_FLOAT, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_unsigned_int(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.u32 = (v), .type = BOB_UNIFORM_UNSIGNED_INT, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_signed_int(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.i32 = (v), .type = BOB_UNIFORM_SIGNED_INT, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_vector2(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.vec2 = (v), .type = BOB_UNIFORM_VEC2, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_vector3(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.vec3 = (v), .type = BOB_UNIFORM_VEC3, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_vector4(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.vec4 = (v), .type = BOB_UNIFORM_VEC4, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_mat4(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.mat4 = (v), .type = BOB_UNIFORM_MAT4, .shader_stage = stage, .is_reference = 0}
#define BOB_uniform_float_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_FLOAT, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_unsigned_int_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_UNSIGNED_INT, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_signed_int_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_SIGNED_INT, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_vector2_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_VEC2, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_vector3_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_VEC3, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_vector4_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_VEC4, .shader_stage = stage, .is_reference = 1}
#define BOB_uniform_mat4_ref(u_name, v, stage) (BOB_Uniform){.name = (u_name), .value.ptr = (v), .type = BOB_UNIFORM_MAT4, .shader_stage = stage, .is_reference = 1}

uint8_t get_uniform(BOB_Material_Handle mat, char *name, BOB_Uniform_Handle *uniform);

uint8_t BOB_create_material(BOB_Renderer_Handle r, BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat);
void BOB_material_free(BOB_Material_Handle *mat);

uint8_t BOB_set_material_float(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float value);
uint8_t BOB_set_material_unsigned_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t value);
uint8_t BOB_set_material_signed_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t value);
uint8_t BOB_set_material_vector2(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 value);
uint8_t BOB_set_material_vector3(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 value);
uint8_t BOB_set_material_vector4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 value);
uint8_t BOB_set_material_mat4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 value);
uint8_t BOB_set_material_float_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float *value);
uint8_t BOB_set_material_unsigned_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t *value);
uint8_t BOB_set_material_signed_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t *value);
uint8_t BOB_set_material_vector2_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 *value);
uint8_t BOB_set_material_vector3_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 *value);
uint8_t BOB_set_material_vector4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 *value);
uint8_t BOB_set_material_mat4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 *value);

//Enum for clipping direction
typedef enum {
    BOB_CLIP_HORZ, //Only clip horizontally
    BOB_CLIP_VERT, //Only clip vertically
    BOB_CLIP_BOTH, //Clip in both directions
} BOB_Clip_Dir;

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer_Handle r, BOB_Quad rect, BOB_Clip_Dir dir);
//Removes the first clipping intersection from the stack and returns its value
void BOB_end_clip(BOB_Renderer_Handle r);

//Sets up the variables for renderering
void BOB_renderer_begin(BOB_Renderer_Handle r, float clear_colour[4]);
//Ends rendering to the current frame
void BOB_renderer_end(BOB_Renderer_Handle r);
//Updates the dimensions of the screen that the renderer renders to. Updates projection matrix
void BOB_renderer_update_dimensions(BOB_Renderer_Handle r, uint32_t width, uint32_t height, uint32_t width_px, uint32_t height_px);

//Draws a quad
uint8_t BOB_draw_atlas_quad(BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation);
//Draws a dynamically allocated texture
uint8_t BOB_draw_texture(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws a pixel buffer
uint8_t BOB_draw_pixelbuffer(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws a filled circle
uint8_t BOB_draw_circle(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer);
//Draws a filled quad
uint8_t BOB_draw_quad(BOB_Renderer_Handle r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws a filled triangle
uint8_t BOB_draw_polygon(BOB_Renderer_Handle r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws an unfilled circle
uint8_t BOB_draw_unfilled_circle(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer);
//Draws an unfilled quad
uint8_t BOB_draw_unfilled_quad(BOB_Renderer_Handle r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws an unfilled triange
uint8_t BOB_draw_unfilled_polygon(BOB_Renderer_Handle r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation);
//Draws a line between two points
uint8_t BOB_draw_line(BOB_Renderer_Handle r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer);

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_mat(BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_mat(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixelbuffer_mat(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a filled circle with a specified material
uint8_t BOB_draw_circle_mat(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat);
//Draws a filled quad with a specified material
uint8_t BOB_draw_quad_mat(BOB_Renderer_Handle r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a filled triangle with a specified material
uint8_t BOB_draw_polygon_mat(BOB_Renderer_Handle r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws an unfilled circle with a specified material
uint8_t BOB_draw_unfilled_circle_mat(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat);
//Draws an unfilled quad with a specified material
uint8_t BOB_draw_unfilled_quad_mat(BOB_Renderer_Handle r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws an unfilled triange with a specified material
uint8_t BOB_draw_unfilled_polygon_mat(BOB_Renderer_Handle r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a line between two points with a specified material
uint8_t BOB_draw_line_mat(BOB_Renderer_Handle r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat);

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_channel(BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);
//Draws a dynamically allocated texture with a specified material and channel
uint8_t BOB_draw_texture_channel(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);
//Draws a pixel buffer with a specified erial and channel
uint8_t BOB_draw_pixelbuffer_channel(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);

//Determines the projection matrix
void BOB_ortho_gl(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest);
void BOB_ortho_vk(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest);
//Converts an angle in degrees to radians
float BOB_degrees_to_radians(float angle);

//Font structs
typedef struct {
    uint32_t codepoint; //Unicode codepoint
    BOB_Quad sub_rect; //What region of the page the glyph occupies
    float x_offset, y_offset, x_advance; //Cursor positions before and after drawing this character
    uint8_t page; //Page used to draw this character
    uint8_t channel; //Channel flags
} BOB_Glyph;

typedef struct {
    uint32_t first, second; //Codepoints of the chars involved in the kerning
    float amount; //How much the xpos should be adjusted when drawing the second char immediately following the first
} BOB_Kerning;

typedef enum {
    BOB_BMF_BINARY,
    BOB_BMF_TEXT,
} BOB_BMF_Format;

uint8_t BOB_create_custom_font(BOB_Renderer_Handle r, size_t num_glyphs, size_t num_kernings, size_t line_height, size_t base, BOB_Font_Handle *font);
uint8_t BOB_load_bmf_font(BOB_Renderer_Handle r, const char *font_path, BOB_BMF_Format format, BOB_Font_Handle *font);
uint8_t BOB_add_font_page(BOB_Font_Handle font, uint32_t page_width, uint32_t page_height, uint8_t *page_data, BOB_Format page_format);
uint8_t BOB_draw_codepoint(BOB_Font_Handle font, uint32_t codepoint, BOB_Vector2 *pos, BOB_Vector4 colour, uint16_t layer);
uint8_t BOB_draw_char_string(BOB_Font_Handle font, char *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer);
uint8_t BOB_draw_codepoint_string(BOB_Font_Handle font, uint32_t *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer);
uint8_t BOB_font_append_glyph(BOB_Font_Handle font, BOB_Glyph glyph);
uint8_t BOB_font_append_kerning(BOB_Font_Handle font, BOB_Kerning kerning);
uint8_t BOB_measure_char_string(char *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out);
uint8_t BOB_measure_codepoint_string(uint32_t *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out);
void BOB_print_parsing_error(void);
uint8_t BOB_font_free(BOB_Font_Handle *font);

#ifdef __cplusplus
}
#endif

#endif //BOB_H

#ifdef BOB_IMPLEMENTATION

#include <ctype.h>
#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef BOB_INCLUDE_GLAD
#include <glad/glad.h>

typedef struct {
    uint32_t texture;
} BOBi_OpenGL_Texture;
#endif // BOB_INCLUDE_GLAD

#ifdef BOB_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
} BOBi_Vulkan_Buffer;

typedef struct {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    VkDescriptorSet descriptor;
} BOBi_Vulkan_Image;
#endif // BOB_INCLUDE_VULKAN

typedef enum {
    #ifdef BOB_INCLUDE_GLAD
    BOB_OPENGL_RENDERER,
    #endif //BOB_INCLUDE_GLAD
    #ifdef BOB_INCLUDE_VULKAN
    BOB_VULKAN_RENDERER,
    #endif //BOB_INCLUDE_VULKAN
    BOB_NUM_RENDERER_TYPES,
} BOBi_Renderer_Type;

typedef struct {
    void *memory;
    size_t capacity;
    size_t offset;
} BOBi_Arena;

//Data structure to hold data about a single render vertex
typedef struct {
    BOB_Vector4 colour; //The colour of the vertex
    BOB_Vector3 pos; //The on-screen position of the render vertex
    BOB_Vector2 uv; //The (u,v) coordinates of the vertex
    uint8_t flags; //Flag order: 0:red, 1:green, 2:blue, 3:alpha, 4:glyph, 5:greyscale.
} BOBi_Render_Vertex;

#define BOB_RED_CHNL_BIT 1
#define BOB_GREEN_CHNL_BIT 2
#define BOB_BLUE_CHNL_BIT 4
#define BOB_ALPHA_CHNL_BIT 8
#define BOB_GLYPH_BIT 16
#define BOB_GREYSCALE_BIT 32

typedef struct {
    union {
        #ifdef BOB_INCLUDE_VULKAN
        BOBi_Vulkan_Image vulkan;
        #endif
        #ifdef BOB_INCLUDE_GLAD
        BOBi_OpenGL_Texture opengl;
        #endif
    };
    uint32_t width, height;
    BOB_Format format;
    uint8_t init;
} BOBi_Texture_Impl;

typedef struct {
    size_t buf_sz;
    uint32_t pbo; //pbo this renderer uses
    BOB_Texture_Handle pixel_tex; //The texture the pbo is rendered to
    uint8_t init;
} BOBi_Pixelbuffer_Impl;

typedef struct {
    BOB_Texture_Handle texture; //GL index of the atlas texture
    uint32_t cursor_x, cursor_y; //Current packing position
    uint32_t row_height; //Height of the tallest texture on the current row
    BOB_Format format; //Pixel format of the texture
    uint8_t init;
} BOBi_Atlas_Impl;

typedef struct {
    const char *name;
    union {
        float f;
        uint32_t u32;
        int32_t i32;
        BOB_Vector2 vec2;
        BOB_Vector3 vec3;
        BOB_Vector4 vec4;
        BOB_Mat4 mat4;
        const void *ptr;
    } value;
    union {
        #ifdef BOB_INCLUDE_GLAD
        struct {
            int32_t location;
        } opengl;
        #endif //BOB_INCLUDE_GLAD
        #ifdef BOB_INCLUDE_VULKAN
        struct {
            union {
                uint32_t binding;
                uint32_t offset;
            };
            VkShaderStageFlags stage;
        } vulkan;
        #endif //BOB_INCLUDE_VULKAN
    };

    BOB_Uniform_Type type;
    uint8_t is_reference;
} BOBi_Uniform_Impl;


//TODO: Make the shader/pipeline independent of the material?
//      Only when we add other stuff to the material like blend modes etc
//      Because right now the material is the shader
typedef struct {
    BOBi_Uniform_Impl *uniforms;
    size_t uniform_count;
    union {
        #ifdef BOB_INCLUDE_GLAD
        struct {
            uint32_t shader;
        } opengl;
        #endif //BOB_INCLUDE_GLAD
        #ifdef BOB_INCLUDE_VULKAN
        struct {
            VkPipelineLayout layout;
            VkPipeline pipeline;
            VkDescriptorSetLayout uniform_set_layout;
            VkDescriptorSet uniform_descriptor_set;
            BOBi_Vulkan_Buffer uniform_buffer;
            void *uniform_buffer_mapped;
            uint32_t uniform_binding;
        } vulkan;
        #endif //BOB_INCLUDE_VULKAN
    };
    uint8_t init;
} BOBi_Material_Impl;

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
    BOBi_Arena vertex_arena; //Used for indices as well
    BOBi_Arena vertex_arena_2;
    BOBi_Arena draw_call_arena;
    size_t num_vertices;
    size_t num_indices;
    size_t num_draw_calls;
} BOBi_RenderBatch;

typedef struct {
    size_t size;
    size_t capacity;
    uint64_t *keys;
    uint32_t *values;
} BOBi_Hashmap;

typedef struct {
    BOB_Texture_Handle pages[256]; //Each glyph's page attribute is 1 byte in the binary format, so only need to worry about 256 pages max
    BOB_Glyph *glyphs;
    BOB_Kerning *kernings;
    BOBi_Hashmap *glyph_map;
    BOBi_Hashmap *kerning_map;
    size_t glyph_capacity;
    size_t glyph_count;
    size_t kerning_capacity;
    size_t kerning_count;
    uint32_t line_height;
    uint32_t base;
    uint8_t page_count;
    uint8_t init;
} BOBi_Font_Impl;

//TODO: Change renderers to also just return handles to the user and keep this struct internal
//      Store renderers in bob_state alongside contexts
typedef struct {
    BOBi_Clip_Stack *stack; //Stores the current clipping rect and the history
    BOB_Mat4 projection; //projection matrix for this renderer
    BOBi_RenderBatch batch; //Vertex/Index/Draw call memory of the renderer

    BOBi_Arena renderer_memory; //Memory arena that this context uses. Each table is just a pointer into this arena

    BOBi_Atlas_Impl *atlas_table;
    BOBi_Pixelbuffer_Impl *pixelbuffer_table;
    BOBi_Texture_Impl *texture_table;
    BOBi_Material_Impl *material_table;
    BOBi_Font_Impl *font_table;

    // void *mapped_mem_ptr; //Pointer to cpu memory mapped from gpu memory
    float *colour;

    //TODO: Maybe make this a pointer instead of a union
    union {
    #ifdef BOB_INCLUDE_GLAD
        struct {
            uint32_t vao; //vao this renderer uses
            uint32_t vbo; //vbo this renderer uses
            uint32_t ebo; //ebo this renderer uses
        } opengl;
    #endif //BOB_INCLUDE_GLAD
    #ifdef BOB_INCLUDE_VULKAN
        struct {
            BOBi_Vulkan_Buffer vertex_buffer;
            BOBi_Vulkan_Buffer index_buffer;
            VkSemaphore present_complete_semaphore;
            VkFence draw_fence;
            VkSemaphore *render_finished_semaphore;
            VkCommandBuffer command_buffer;

            //Device management
            VkPhysicalDevice phy_device;
            VkDevice log_device;
            VkQueue graphics_queue;
            uint32_t queue_family;

            //Swapchain management
            VkImage *images;
            VkImageView *views;
            VkSwapchainKHR swapchain;
            VkSurfaceKHR surface;
            VkSurfaceFormatKHR format;
            VkExtent2D extent;
            uint32_t num_images;
            uint32_t next_swapchain_image_index;

            VkCommandPool command_pool;

            BOBi_Vulkan_Image depth;
            uint8_t framebuffer_resized;

            //Pipeline stuff
            VkSampler sampler;
            VkDescriptorPool descriptor_pool;
            VkDescriptorSetLayout default_tex_layout;

            //TODO: FIX VULKAN MEMORY
            BOBi_Vulkan_Buffer vert_staging_buf;
            BOBi_Vulkan_Buffer index_staging_buf;
            BOBi_Vulkan_Buffer pbo_staging_buf;
            size_t pbo_staging_buf_sz;
        } vulkan;
    #endif //BOB_INCLUDE_VULKAN
    };

    size_t num_atlases;
    size_t num_textures;
    size_t num_pixelbuffers;
    size_t num_materials;
    size_t num_fonts;

    size_t atlas_capacity;
    size_t texture_capacity;
    size_t pixelbuffer_capacity;
    size_t material_capacity;
    size_t font_capacity;

    uint32_t next_atlas_slot;
    uint32_t next_tex_slot;
    uint32_t next_pixelbuf_slot;
    uint32_t next_mat_slot;
    uint32_t next_font_slot;

    BOB_Texture_Handle default_tex;
    BOB_Material_Handle default_mat; //Default material this renderer uses

    uint32_t screen_height;
    uint32_t screen_width;
    uint32_t screen_height_px;
    uint32_t screen_width_px;

    BOBi_Renderer_Type type;
    uint8_t frame_state; //Holds the state of the renderer: 0 - first ever frame 1 - start of new frame 2 - in the middle of drawing a frame
} BOBi_Renderer_Impl;

//Return the value of the element at the top of the stack without popping it
#define BOB_peek_clip_rect(stack) (((stack)->size > 0) ? (stack)->elems[(stack)->size-1] : (BOBi_Clip_Rect){0})
#define BOBi_MSB 0x80000000

typedef struct {
    BOBi_Renderer_Impl *renderers;
    size_t renderer_count;
    size_t renderer_capcity;
    size_t next_renderer_slot;
    #ifdef BOB_INCLUDE_VULKAN
    VkInstance instance;
    #endif //BOB_INCLUDE_VULKAN
} BOBi_Internal_State;

BOBi_Internal_State bob_state = {0};

// ================================ BOB ARENA IMPLEMENTATION =================================

#define MIN_ALIGNMENT alignof(max_align_t)

size_t BOBi_align_up(size_t value, size_t alignment) {
    //Assert that alignments are powers of 2
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);
    return (value + alignment - 1) & ~(alignment - 1);
}

uint8_t BOB_init_arena(BOBi_Arena *arena, size_t capacity) {
    arena->capacity = capacity;
    arena->offset = 0;
    arena->memory = malloc(capacity);

    return arena->memory != NULL;
}

void BOB_destroy_arena(BOBi_Arena *arena) {
    if(arena->memory) free(arena->memory);
    *arena = (BOBi_Arena){0};
}

void *BOB_arena_alloc(BOBi_Arena *arena, size_t size, size_t alignment) {
    //Assert that alignments are powers of 2:
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);

    uintptr_t current = (uintptr_t)arena->memory + arena->offset;
    uintptr_t offset = BOBi_align_up(current, alignment);
    offset -= (uintptr_t)(arena->memory);

    if(offset + size > arena->capacity) return NULL; //Out of memory

    void *ptr = (void *)((uintptr_t)arena->memory + offset);
    arena->offset = offset + size;

    return ptr;
}

void BOB_arena_clear(BOBi_Arena *arena) {
    arena->offset = 0;
}

#define BOBi_get_arena_elem(arena, index, type) ((type *)(arena).memory)[(index)]

uint8_t BOBi_get_renderer_from_handle(uint64_t handle, BOBi_Renderer_Impl **out) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles

    uint32_t index = (handle & 0xFFFFFFFF00000000) >> 32;
    if(bob_state.renderers[index].renderer_memory.memory == NULL) return 0; //Invalid renderer
    *out = &bob_state.renderers[index];
    return 1;
}

uint8_t BOBi_get_index_from_handle(uint64_t handle, uint32_t *out) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles
    *out = handle & (~0xFFFFFFFF80000000);
    return 1;
}

uint8_t BOBi_get_handle_data(uint64_t handle, BOBi_Renderer_Impl **renderer, uint32_t *index) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles

    uint32_t renderer_index = (handle & 0xFFFFFFFF00000000) >> 32;
    if(bob_state.renderers[renderer_index].renderer_memory.memory == NULL) return 0; //Invalid renderer
    *renderer = &bob_state.renderers[renderer_index];

    *index = handle & (~0xFFFFFFFF80000000);
    return 1;
}

uint8_t BOBi_get_renderer(BOB_Renderer_Handle handle, BOBi_Renderer_Impl **out) {
    if(handle & BOBi_MSB) {
        printf("Invalid renderer handle\n");
        return 0;
    }

    BOBi_Renderer_Impl *renderer = &bob_state.renderers[handle];
    if(renderer->renderer_memory.memory == NULL) {
        printf("Invalid renderer handle\n");
        return 0;
    }

    *out = renderer;
    return 1;
}

uint8_t BOBi_create_renderer(BOBi_Renderer_Type type, size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, size_t width, size_t height, BOB_Renderer_Handle *renderer);

typedef enum {
    BOBi_DRAW_QUAD,
    BOBi_DRAW_CIRCLE,
    BOBi_DRAW_POLY,
} BOBi_Draw_Type;

typedef struct {
    BOBi_Render_Vertex *vertices; //Pointer into the vertex arena where this draw call's vertices start
    size_t num_vertices; //Number of vertices in the draw call
    size_t num_indices; //Number of indices in the draw call
    size_t index_offset; //Offset from the start of the index array
    BOB_Texture_Handle tex; //Texture handle. Primary sorting key of draw calls
    BOB_Material_Handle mat; //Material handle. Secondary sorting key of draw calls
    uint32_t submission_id; //Tertiary sorting key of draw calls. Since this should be unique for each draw call associated with a renderer, this acts as a tiebreaker
    BOBi_Draw_Type type; //Determines how the draw call's indicies are generated
} BOBi_Draw_Call;

//Reads the entirety of a file into the given buffer
int BOBi_read_to_end(char const *path, uint8_t **buf, uint8_t add_null) {
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

//============================== OPENGL CODE ========================================

#ifdef BOB_INCLUDE_GLAD

void BOBi_gl_update_uniform(BOBi_Uniform_Impl uniform) {
    switch(uniform.type) {
        case BOB_UNIFORM_FLOAT:
            glUniform1f(uniform.opengl.location, (uniform.is_reference) ? *(float *)uniform.value.ptr : uniform.value.f);
            break;
        case BOB_UNIFORM_UNSIGNED_INT:
            glUniform1ui(uniform.opengl.location, (uniform.is_reference) ? *(uint32_t *)uniform.value.ptr : uniform.value.u32);
            break;
        case BOB_UNIFORM_SIGNED_INT:
            glUniform1i(uniform.opengl.location, (uniform.is_reference) ? *(int32_t *)uniform.value.ptr : uniform.value.i32);
            break;
        case BOB_UNIFORM_VEC2:
            glUniform2fv(uniform.opengl.location, 1, (uniform.is_reference) ? &(*(BOB_Vector2 *)uniform.value.ptr).x : &uniform.value.vec2.x);
            break;
        case BOB_UNIFORM_VEC3:
            glUniform3fv(uniform.opengl.location, 1, (uniform.is_reference) ? &(*(BOB_Vector3 *)uniform.value.ptr).x : &uniform.value.vec3.x);
            break;
        case BOB_UNIFORM_VEC4:
            glUniform4fv(uniform.opengl.location, 1, (uniform.is_reference) ? &(*(BOB_Vector4 *)uniform.value.ptr).x : &uniform.value.vec4.x);
            break;
        case BOB_UNIFORM_MAT4:
            glUniformMatrix4fv(uniform.opengl.location, 1, GL_FALSE, (uniform.is_reference) ? (float *)(*(BOB_Mat4 *)uniform.value.ptr).m : (float *)uniform.value.mat4.m);
            break;
    }
}

void BOBi_gl_delete_texture(BOBi_Renderer_Impl *renderer, uint32_t tex_index) {
    if(renderer->texture_table[tex_index].init)
        glDeleteTextures(1, &renderer->texture_table[tex_index].opengl.texture);
}

void BOBi_gl_delete_buffer(BOBi_Renderer_Impl *renderer, uint32_t buf_index) {
    if(renderer->pixelbuffer_table[buf_index].init)
        glDeleteBuffers(1, &renderer->pixelbuffer_table[buf_index].pbo);
}

void BOBi_gl_delete_program(BOBi_Renderer_Impl *renderer, uint32_t program_index) {
    if(renderer->material_table[program_index].init)
        glDeleteProgram(renderer->material_table[program_index].opengl.shader);
}

//Shaders for this program are simple enough that we can just encode them as strings
//to avoid annoying file loading/reading every startup
const char *vertex_shader = "#version 330 core\n"
                            "layout (location = 0) in vec4 aColor;\n"
                            "layout (location = 1) in vec3 aPos;\n"
                            "layout (location = 2) in vec2 aTexCoord;\n"
                            "layout (location = 3) in uint aChannel;\n"
                            "uniform mat4 uProjection;\n"
                            "out vec4 ourColor;\n"
                            "out vec2 TexCoord;\n"
                            "flat out uint Channel;\n"
                            "void main() {\n"
                            "    gl_Position = uProjection * vec4(aPos, 1.0);\n"
                            "    ourColor = aColor;"
                            "    TexCoord = aTexCoord;\n"
                            "    Channel = aChannel;\n"
                            "}\n";
const char *fragment_shader = "#version 330 core\n"
                              "out vec4 FragColor;\n"
                              "in vec2 TexCoord;\n"
                              "in vec4 ourColor;\n"
                              "flat in uint Channel;\n"
                              "uniform sampler2D screenTexture;\n"
                              "void main() {\n"
                              "    if((Channel & 16u) != 0u) {\n"//Glyph bit set
                              "        vec4 texel = texture(screenTexture, TexCoord);\n"
                              "        float glyphAlpha;\n"
                              "        if((Channel & 15u) == 0u) {\n" //Non-packed glyph
                              "            if((Channel & 32u) == 0u)\n" //Not greyscale
                              "                glyphAlpha = texel.a;\n"
                              "            else\n"
                              "                glyphAlpha = texel.r;\n"
                              "        }\n"
                              "        else {\n"
                              "            glyphAlpha = 0.0;\n"
                              "            if ((Channel & 8u) != 0u)\n"
                              "                glyphAlpha = max(glyphAlpha, texel.a);\n"
                              "            if ((Channel & 4u) != 0u)\n"
                              "                glyphAlpha = max(glyphAlpha, texel.r);\n"
                              "            if ((Channel & 2u) != 0u)\n"
                              "                glyphAlpha = max(glyphAlpha, texel.g);\n"
                              "            if ((Channel & 1u) != 0u)\n"
                              "                glyphAlpha = max(glyphAlpha, texel.b);\n"
                              "        }\n"
                              "        FragColor = vec4(ourColor.rgb, ourColor.a * glyphAlpha);\n"
                              "    }\n"
                              "    else {\n" //Glyph bit not set
                              "        FragColor = texture(screenTexture, TexCoord) * ourColor;\n"
                              "    }\n"
                              "}\n";

uint8_t BOB_create_opengl_renderer(size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t width, size_t height, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, BOB_Renderer_Handle *renderer) {
    if(!BOBi_create_renderer(BOB_OPENGL_RENDERER, atlas_capacity, pixelbuf_capacity, tex_capacity, mat_capacity, font_capacity, vertex_capacity, index_capacity, draw_call_capacity, width, height, renderer)) return 0;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);

    BOBi_Renderer_Impl *r;
    BOBi_get_renderer(*renderer, &r);

    glGenVertexArrays(1, &r->opengl.vao);
    glBindVertexArray(r->opengl.vao);

    //Getting the vbo
    glGenBuffers(1, &r->opengl.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->opengl.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(BOBi_Render_Vertex) * vertex_capacity, NULL, GL_DYNAMIC_DRAW);

    //Getting the ebo
    glGenBuffers(1, &r->opengl.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->opengl.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * index_capacity, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BOBi_Render_Vertex), (void *)offsetof(BOBi_Render_Vertex, colour)); //Vertex Colour
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BOBi_Render_Vertex), (void *)offsetof(BOBi_Render_Vertex, pos)); //Vertex Position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BOBi_Render_Vertex), (void *)offsetof(BOBi_Render_Vertex, uv)); //UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(BOBi_Render_Vertex), (void *)offsetof(BOBi_Render_Vertex, flags));
    glEnableVertexAttribArray(3);

    BOB_ortho_gl(0.0f, r->screen_width, r->screen_height, 0.0f, -BOB_MAX_LAYER, 0.0f, &r->projection);

    if(!BOB_create_texture(*renderer, 1, 1, (uint8_t[4]){255, 255, 255, 255}, BOB_RGBA, &r->default_tex)) {
        BOB_destroy_renderer(renderer);
        return 0;
    }

    if(!BOB_create_material(*renderer, (BOB_Shader_Data[2]){(BOB_Shader_Data){.shader_code = vertex_shader, .type = BOB_VERTEX_SHADER}, (BOB_Shader_Data){.shader_code = fragment_shader, .type = BOB_FRAGMENT_SHADER}}, 2, (BOB_Uniform[1]){BOB_uniform_mat4("uProjection", r->projection, BOB_VERTEX_SHADER)}, 1, &r->default_mat)) {
        BOB_destroy_renderer(renderer);
        return 0;
    }

    return 1;
}

void BOBi_gl_destroy_renderer_mem(BOBi_Renderer_Impl *r) {
    glDeleteBuffers(2, (uint32_t[2]){r->opengl.vbo, r->opengl.ebo});
    glDeleteVertexArrays(1, &r->opengl.vao);
}

uint8_t BOBi_gl_begin_frame(BOBi_Renderer_Impl *r) {
    //Resetting colour and depth
    if(r->colour != NULL) glClearColor(r->colour[0], r->colour[1], r->colour[2], r->colour[3]);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Need to clear the depth buffer as well

    return 1;
}

uint8_t BOBi_gl_end_frame(BOBi_Renderer_Impl *r) {return 1;}

uint8_t BOBi_gl_draw(BOBi_Renderer_Impl *r) {
    //Bind all of the arrays and buffers we will reuse over time
    glBindVertexArray(r->opengl.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->opengl.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->opengl.ebo);

    BOBi_Draw_Call start = BOBi_get_arena_elem(r->batch.draw_call_arena, 0, BOBi_Draw_Call);
    glBufferSubData(GL_ARRAY_BUFFER, 0, r->batch.num_vertices * sizeof(BOBi_Render_Vertex), r->batch.vertex_arena_2.memory); //Copies the data from renderer's triangle data into the vbo
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, r->batch.num_indices * sizeof(uint32_t), r->batch.vertex_arena.memory); //Copies the quad data into the vbo

    //TODO: At the very least make a texture array so we don't keep switching textures, but would also be nice to make an SSBO for the materials
    for(size_t i = 0; i < r->batch.num_draw_calls; i++) {
        BOBi_Draw_Call call = BOBi_get_arena_elem(r->batch.draw_call_arena, i, BOBi_Draw_Call);
        uint32_t mat_index, tex_index;
        if(!BOBi_get_index_from_handle(call.mat, &mat_index)) return 0;
        if(!BOBi_get_index_from_handle(call.tex, &tex_index)) return 0;
        glUseProgram(r->material_table[mat_index].opengl.shader);
        //Setting the uniforms
        for(size_t j = 0; j < r->material_table[mat_index].uniform_count; j++) {
            BOBi_gl_update_uniform(r->material_table[mat_index].uniforms[j]);
        }

        //Bind the atlas texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, r->texture_table[tex_index].opengl.texture);

        glDrawElements(GL_TRIANGLES, call.num_indices, GL_UNSIGNED_INT, (void *)(call.index_offset * sizeof(uint32_t))); //Make the draw call
    }

    return 1;
}

void BOBi_gl_copy_buffer_data(BOBi_Renderer_Impl *r, void *data, size_t data_sz) {
    //Update projection matrix for renderer
    BOB_ortho_gl(0.0f, r->screen_width, r->screen_height, 0.0f, -BOB_MAX_LAYER, 0.0f, &r->projection);
    BOB_set_material_mat4(r->default_mat, 0, r->projection);

    glBindBuffer(GL_ARRAY_BUFFER, r->opengl.vao);
    glBufferSubData(GL_ARRAY_BUFFER, 0, data_sz, data);
}

uint32_t BOBi_gl_convert_format(BOB_Format format) {
    switch (format) {
        case BOB_RED: return GL_RED;
        case BOB_RG: return GL_RG;
        case BOB_RGB: return GL_RGB;
        case BOB_RGBA: return GL_RGBA;
    }
}

uint8_t BOBi_gl_create_tex(BOBi_Renderer_Impl *renderer, uint32_t tex_index, size_t width, size_t height, uint8_t *data, BOB_Format format) {
    uint32_t *tex = &renderer->texture_table[tex_index].opengl.texture;

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, BOBi_gl_convert_format(format), width, height, 0, BOBi_gl_convert_format(format), GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 1;
}

uint32_t BOBi_gl_create_shader(BOB_Shader_Data s) {
    uint32_t shader;
    int32_t shader_type;
    switch(s.type) {
        case BOB_VERTEX_SHADER: shader_type = GL_VERTEX_SHADER; break;
        case BOB_FRAGMENT_SHADER: shader_type = GL_FRAGMENT_SHADER; break;
        case BOB_TESS_CTRL_SHADER: shader_type = GL_TESS_CONTROL_SHADER; break;
        case BOB_TESS_EVAL_SHADER: shader_type = GL_TESS_EVALUATION_SHADER; break;
        case BOB_COMPUTE_SHADER: shader_type = GL_COMPUTE_SHADER; break;
        case BOB_GEOMETRY_SHADER: shader_type = GL_GEOMETRY_SHADER; break;
    }

    shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &s.shader_code, NULL);
    glCompileShader(shader);

    int result;
    char infolog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if(!result) {
        glGetShaderInfoLog(shader, 512, NULL, infolog);
        printf("ERROR::SHADER::COMPILATION_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            printf("%c", infolog[i]);
        }
        printf("\n");
    }

    return shader;
}

uint8_t BOBi_gl_create_material(BOBi_Renderer_Impl *renderer, uint32_t index, BOB_Shader_Data *data, size_t num_shaders) {
    BOBi_Material_Impl *intrn_mat = &renderer->material_table[index];
    uint32_t shader_buf[BOB_MAX_SHADERS]; //Array to store the ids of the loaded shader sub-programs
    intrn_mat->opengl.shader = glCreateProgram();

    //Attaching all of the shaders together
    for(int i = 0; i < num_shaders; i++) {
        shader_buf[i] = BOBi_gl_create_shader(data[i]);
        glAttachShader(intrn_mat->opengl.shader, shader_buf[i]);
    }

    glLinkProgram(intrn_mat->opengl.shader);
    int result;
    char infolog[512];

    //Print errors if any:
    glGetProgramiv(intrn_mat->opengl.shader, GL_LINK_STATUS, &result);
    if(!result) {
        glGetProgramInfoLog(intrn_mat->opengl.shader, 512, NULL, infolog);
        printf("ERROR::SHADER::LINKING_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            printf("%c", infolog[i]);
        }
        printf("\n");
        return 0;
    }

    //Cleanup
    for(int i = 0; i < num_shaders; i++) {
        glDeleteShader(shader_buf[i]);
    }

    //Setting the uniforms
    for(size_t i = 0; i < intrn_mat->uniform_count; i++) {
        intrn_mat->uniforms[i].opengl.location = glGetUniformLocation(intrn_mat->opengl.shader, intrn_mat->uniforms[i].name);
    }
    return 1;
}

void BOBi_gl_copy_data_tex(BOBi_Renderer_Impl *renderer, uint32_t tex_index, BOB_Format format, BOB_Quad region, uint8_t *pixels) {
    GLenum gl_format = BOBi_gl_convert_format(format);

    //Upload the subregion
    glBindTexture(GL_TEXTURE_2D, renderer->texture_table[tex_index].opengl.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, region.x, region.y, region.w, region.h, gl_format, GL_UNSIGNED_BYTE, pixels);
}

uint8_t BOBi_gl_create_pbo(BOBi_Renderer_Impl *renderer, uint32_t index) {
    uint32_t *pbo = &renderer->pixelbuffer_table[index].pbo;
    size_t buf_sz = renderer->pixelbuffer_table[index].buf_sz;

    glGenBuffers(1, pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, buf_sz, NULL, GL_STREAM_DRAW);
    uint8_t *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
    if(ptr == NULL) {
        printf("Failed to map GPU to CPU memory\n");
        return 0;
    }
    memset(ptr, 0x00, buf_sz); //Setting all of the pixels to be colourless initially
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 1;
}

uint8_t BOBi_gl_bind_pbo_mem(BOBi_Renderer_Impl *renderer, uint32_t pb_index, void **mapped_mem_ptr, size_t *mem_sz) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, renderer->pixelbuffer_table[pb_index].pbo);

    *mapped_mem_ptr= glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_WRITE);
    if(*mapped_mem_ptr == NULL) {
        printf("Failed to map GPU to CPU memory\n");
        return 0;
    }
    *mem_sz = renderer->pixelbuffer_table[pb_index].buf_sz;
    return 1;
}

void BOBi_gl_unbind_pbo_mem(BOBi_Renderer_Impl *renderer, uint32_t index) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, renderer->pixelbuffer_table[index].pbo);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void BOBi_gl_upload_pbo_data(BOBi_Renderer_Impl *renderer, uint32_t pb_index) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, renderer->pixelbuffer_table[pb_index].pbo);

    uint32_t tex_index;
    BOBi_get_index_from_handle(renderer->pixelbuffer_table[pb_index].pixel_tex, &tex_index);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_table[tex_index].opengl.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, renderer->texture_table[tex_index].width, renderer->texture_table[tex_index].height, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

#endif //BOB_INCLUDE_GLAD

//================================================= VULKAN FUNCTIONS ================================================

#ifdef BOB_INCLUDE_VULKAN
unsigned char shader_frag_spv[] = {
  0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00,
  0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x09, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x00, 0x00,
  0x60, 0x00, 0x00, 0x00, 0x10, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
  0xc2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x47, 0x4c, 0x5f, 0x47,
  0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x63, 0x70, 0x70, 0x5f, 0x73, 0x74,
  0x79, 0x6c, 0x65, 0x5f, 0x6c, 0x69, 0x6e, 0x65, 0x5f, 0x64, 0x69, 0x72,
  0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00,
  0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x69, 0x6e,
  0x63, 0x6c, 0x75, 0x64, 0x65, 0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74,
  0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x43, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x00,
  0x05, 0x00, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x65,
  0x6c, 0x00, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x73, 0x63, 0x72, 0x65, 0x65, 0x6e, 0x54, 0x65, 0x78, 0x74, 0x75, 0x72,
  0x65, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0x1c, 0x00, 0x00, 0x00,
  0x54, 0x65, 0x78, 0x43, 0x6f, 0x6f, 0x72, 0x64, 0x00, 0x00, 0x00, 0x00,
  0x05, 0x00, 0x05, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x79, 0x70,
  0x68, 0x41, 0x6c, 0x70, 0x68, 0x61, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
  0x5e, 0x00, 0x00, 0x00, 0x46, 0x72, 0x61, 0x67, 0x43, 0x6f, 0x6c, 0x6f,
  0x72, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0x60, 0x00, 0x00, 0x00,
  0x6f, 0x75, 0x72, 0x43, 0x6f, 0x6c, 0x6f, 0x72, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x04, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x5e, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
  0x60, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x21, 0x00, 0x03, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x02, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x16, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x17, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x19, 0x00, 0x09, 0x00,
  0x15, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x03, 0x00,
  0x16, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
  0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
  0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x04, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x2d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x5d, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x5d, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x12, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x5f, 0x00, 0x00, 0x00,
  0x60, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00,
  0x61, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x13, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x04, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x09, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x05, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0xab, 0x00, 0x05, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x0e, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0xf7, 0x00, 0x03, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfa, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
  0x6d, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x0f, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x16, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x1d, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x57, 0x00, 0x05, 0x00,
  0x12, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00,
  0x1d, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x1f, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x05, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x00, 0xaa, 0x00, 0x05, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x22, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0xf7, 0x00, 0x03, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfa, 0x00, 0x04, 0x00, 0x22, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x33, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x27, 0x00, 0x00, 0x00, 0x25, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00,
  0xaa, 0x00, 0x05, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x27, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x03, 0x00,
  0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x04, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x29, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
  0x2b, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x2d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x2f, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00,
  0x2a, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x41, 0x00, 0x05, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
  0xf9, 0x00, 0x02, 0x00, 0x2a, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x2a, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00, 0x24, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x33, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0xc7, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00,
  0x35, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0xab, 0x00, 0x05, 0x00,
  0x0d, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x03, 0x00, 0x3a, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x04, 0x00, 0x38, 0x00, 0x00, 0x00,
  0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x39, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
  0x2b, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x2d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
  0xf9, 0x00, 0x02, 0x00, 0x3a, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x3a, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x3f, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x05, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0xab, 0x00, 0x05, 0x00, 0x0d, 0x00, 0x00, 0x00,
  0x42, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0xf7, 0x00, 0x03, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xfa, 0x00, 0x04, 0x00, 0x42, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00,
  0x44, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x43, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x46, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00,
  0x46, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x45, 0x00, 0x00, 0x00, 0x47, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00,
  0x44, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x44, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0xc7, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x4b, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00,
  0xab, 0x00, 0x05, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00,
  0x4b, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x03, 0x00,
  0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x04, 0x00,
  0x4c, 0x00, 0x00, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
  0x41, 0x00, 0x05, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x07, 0x00, 0x11, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00,
  0x52, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x00, 0x00,
  0x53, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00, 0x4e, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x4e, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0xc7, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,
  0x54, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0xab, 0x00, 0x05, 0x00,
  0x0d, 0x00, 0x00, 0x00, 0x56, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x03, 0x00, 0x58, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x04, 0x00, 0x56, 0x00, 0x00, 0x00,
  0x57, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x57, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x59, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
  0x2b, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x4a, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x5b, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x07, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x59, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00,
  0xf9, 0x00, 0x02, 0x00, 0x58, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x58, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00, 0x24, 0x00, 0x00, 0x00,
  0xf8, 0x00, 0x02, 0x00, 0x24, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x12, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
  0x4f, 0x00, 0x08, 0x00, 0x61, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00,
  0x62, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00,
  0x64, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
  0x2d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x66, 0x00, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00,
  0x85, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00,
  0x66, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x6a, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x51, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00,
  0x63, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00,
  0x12, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00,
  0x6a, 0x00, 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x5e, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00,
  0xf9, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00,
  0x6d, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x6e, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x1a, 0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
  0x57, 0x00, 0x05, 0x00, 0x12, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00,
  0x6e, 0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x12, 0x00, 0x00, 0x00, 0x71, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
  0x85, 0x00, 0x05, 0x00, 0x12, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00,
  0x70, 0x00, 0x00, 0x00, 0x71, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
  0x5e, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x02, 0x00,
  0x10, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00,
  0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
};
unsigned int shader_frag_spv_len = 2708;

unsigned char shader_vert_spv[] = {
  0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x0d, 0x00,
  0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x47, 0x4c, 0x53, 0x4c, 0x2e, 0x73, 0x74, 0x64, 0x2e, 0x34, 0x35, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x0d, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x25, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x2e, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00,
  0x02, 0x00, 0x00, 0x00, 0xc2, 0x01, 0x00, 0x00, 0x04, 0x00, 0x0a, 0x00,
  0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c, 0x45, 0x5f, 0x63, 0x70,
  0x70, 0x5f, 0x73, 0x74, 0x79, 0x6c, 0x65, 0x5f, 0x6c, 0x69, 0x6e, 0x65,
  0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x00,
  0x04, 0x00, 0x08, 0x00, 0x47, 0x4c, 0x5f, 0x47, 0x4f, 0x4f, 0x47, 0x4c,
  0x45, 0x5f, 0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65, 0x5f, 0x64, 0x69,
  0x72, 0x65, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x05, 0x00, 0x04, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x05, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x50,
  0x65, 0x72, 0x56, 0x65, 0x72, 0x74, 0x65, 0x78, 0x00, 0x00, 0x00, 0x00,
  0x06, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x67, 0x6c, 0x5f, 0x50, 0x6f, 0x73, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x00,
  0x06, 0x00, 0x07, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x67, 0x6c, 0x5f, 0x50, 0x6f, 0x69, 0x6e, 0x74, 0x53, 0x69, 0x7a, 0x65,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x07, 0x00, 0x0b, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x43, 0x6c, 0x69, 0x70, 0x44,
  0x69, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x00, 0x06, 0x00, 0x07, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x67, 0x6c, 0x5f, 0x43,
  0x75, 0x6c, 0x6c, 0x44, 0x69, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x00,
  0x05, 0x00, 0x03, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x05, 0x00, 0x04, 0x00, 0x11, 0x00, 0x00, 0x00, 0x43, 0x61, 0x6d, 0x65,
  0x72, 0x61, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x75, 0x50, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74,
  0x69, 0x6f, 0x6e, 0x00, 0x05, 0x00, 0x03, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x19, 0x00, 0x00, 0x00,
  0x61, 0x50, 0x6f, 0x73, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
  0x23, 0x00, 0x00, 0x00, 0x6f, 0x75, 0x72, 0x43, 0x6f, 0x6c, 0x6f, 0x72,
  0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00, 0x25, 0x00, 0x00, 0x00,
  0x61, 0x43, 0x6f, 0x6c, 0x6f, 0x72, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
  0x29, 0x00, 0x00, 0x00, 0x54, 0x65, 0x78, 0x43, 0x6f, 0x6f, 0x72, 0x64,
  0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x61, 0x54, 0x65, 0x78, 0x43, 0x6f, 0x6f, 0x72, 0x64, 0x00, 0x00, 0x00,
  0x05, 0x00, 0x04, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x43, 0x68, 0x61, 0x6e,
  0x6e, 0x65, 0x6c, 0x00, 0x05, 0x00, 0x05, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x61, 0x43, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x03, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x05, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x0b, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x48, 0x00, 0x04, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x05, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x48, 0x00, 0x05, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
  0x13, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x04, 0x00, 0x19, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
  0x25, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x04, 0x00, 0x29, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x47, 0x00, 0x03, 0x00,
  0x2e, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x47, 0x00, 0x04, 0x00,
  0x2e, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x47, 0x00, 0x04, 0x00, 0x30, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x13, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x21, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x16, 0x00, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x17, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x15, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x1c, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x09, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
  0x0a, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x15, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x00, 0x00,
  0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x04, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x03, 0x00, 0x11, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x12, 0x00, 0x00, 0x00,
  0x13, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x17, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x18, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x2b, 0x00, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x80, 0x3f, 0x20, 0x00, 0x04, 0x00, 0x21, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x21, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00,
  0x25, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x17, 0x00, 0x04, 0x00,
  0x27, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x28, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x27, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x29, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00,
  0x2a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
  0x3b, 0x00, 0x04, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x2d, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00,
  0x2d, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x04, 0x00, 0x2f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x04, 0x00, 0x2f, 0x00, 0x00, 0x00,
  0x30, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x36, 0x00, 0x05, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00,
  0x41, 0x00, 0x05, 0x00, 0x14, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
  0x13, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x17, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x19, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00,
  0x1c, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x51, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
  0x1a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x51, 0x00, 0x05, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x50, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00,
  0x1f, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
  0x1e, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x91, 0x00, 0x05, 0x00,
  0x07, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x1f, 0x00, 0x00, 0x00, 0x41, 0x00, 0x05, 0x00, 0x21, 0x00, 0x00, 0x00,
  0x22, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x22, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x3d, 0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00,
  0x25, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00, 0x23, 0x00, 0x00, 0x00,
  0x26, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00, 0x27, 0x00, 0x00, 0x00,
  0x2c, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x03, 0x00,
  0x29, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x04, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00,
  0x3e, 0x00, 0x03, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
  0xfd, 0x00, 0x01, 0x00, 0x38, 0x00, 0x01, 0x00
};
unsigned int shader_vert_spv_len = 1700;

//Validation layers we are using. TODO: Figure out memory leaks
const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
size_t num_validation_layers = 1;
//Extensions we are using
const char *required_device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; //Need the swapchain extension for drawing surfaces to a window
size_t num_required_device_extensions = 1;

BOBi_Render_Vertex vertices[4] = {
    (BOBi_Render_Vertex){.pos = {-0.5f, -0.5f}},
    (BOBi_Render_Vertex){.pos = {-0.5f,  0.5f}},
    (BOBi_Render_Vertex){.pos = { 0.5f,  0.5f}},
    (BOBi_Render_Vertex){.pos = { 0.5f, -0.5f}},
};
size_t num_vertices = 4;
uint32_t indices[6] = {
    0,1,3,1,2,3
};
size_t num_indices = 6;
BOBi_Vulkan_Buffer index_buf;
BOBi_Vulkan_Buffer vertex_buf;

//Not really used ig?
#ifdef NDEBUG
#define ENABLE_VALIDATION_LAYERS 0
#else
#define ENABLE_VALIDATION_LAYERS 1
#endif

//Macro to get a list of values from vulkan. Allocates memory which must be freed later
//func1 must be the vulkan enumeration function with the output list set to NULL and the
//output size set to some variable
//func2 must have both values set.
//size must be the number of elements in the list (obtained from the call to func1)
//* the size of an individual element
#define VULKAN_ENUMERATE(func1, func2, enumerator_list, size, failure_string) do {  \
    if((func1) != VK_SUCCESS) {                                                     \
        printf("%s\n", (failure_string));                                           \
        return 0;                                                                   \
    }                                                                               \
    (enumerator_list) = malloc((size));                                             \
    if((func2) != VK_SUCCESS) {                                                     \
        printf("%s\n", (failure_string));                                           \
        return 0;                                                                   \
    }                                                                               \
} while(0)

//Checks if a vulkan function has succeeded, returns 0, calls the functions to free data, and prints failure if not
#define VULKAN_ERROR(func, failure_string, ...) do {     \
    if((func) != VK_SUCCESS) {                           \
        printf("%s\n", (failure_string));                \
        __VA_ARGS__;                                     \
        return 0;                                        \
    }                                                    \
} while(0)

//A vertex binding describes the rate at which to load data from memory throughout the vertices
VkVertexInputBindingDescription BOBi_vk_get_binding_desc() {
    return (VkVertexInputBindingDescription){.binding = 0, .stride = sizeof(BOBi_Render_Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
}

//Returns the VAO for our Vertex struct
void BOBi_vk_get_attrib_descs(VkVertexInputAttributeDescription *out_list, size_t *sz) {
    *sz = 4;
    out_list[0] = (VkVertexInputAttributeDescription){.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(BOBi_Render_Vertex, colour)};
    out_list[1] = (VkVertexInputAttributeDescription){.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(BOBi_Render_Vertex, pos)};
    out_list[2] = (VkVertexInputAttributeDescription){.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(BOBi_Render_Vertex, uv)};
    out_list[3] = (VkVertexInputAttributeDescription){.location = 3, .binding = 0, .format = VK_FORMAT_R8_UINT, .offset = offsetof(BOBi_Render_Vertex, flags)};
}

//Allocates and begins a given command buffer. Should only be used if a command buffer needs to be used once
uint8_t BOBi_vk_begin_single_time_commands(BOBi_Renderer_Impl *renderer, VkCommandBuffer *out) {
    //Allocate the command buffer
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = NULL,
        .commandPool = renderer->vulkan.command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1
    };
    VULKAN_ERROR(vkAllocateCommandBuffers(renderer->vulkan.log_device, &alloc_info, out), "Failed to allocate a command buffer");

    //Begin accepting commands
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VULKAN_ERROR(vkBeginCommandBuffer(*out, &begin_info), "Failed to start command buffer");

    return 1;
}

//Ends given command buffer, submits its internal commands to the Vulkan_State
//struct's graphics_queue and frees the command buffer at the end
uint8_t BOBi_vk_end_single_time_commands(BOBi_Renderer_Impl *renderer, VkCommandBuffer buf) {
    //End the command buffer
    VULKAN_ERROR(vkEndCommandBuffer(buf), "Failed to end copy command buffer",
                          vkFreeCommandBuffers(renderer->vulkan.log_device, renderer->vulkan.command_pool, 1, &buf));

    //Submit the command buffer's instructions to the graphics_queue
    VkSubmitInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL,
        .commandBufferCount = 1, .pCommandBuffers = &buf
    };
    VULKAN_ERROR(vkQueueSubmit(renderer->vulkan.graphics_queue, 1, &queue_info, VK_NULL_HANDLE), "Failed to submit commands to graphics queue",
                          vkFreeCommandBuffers(renderer->vulkan.log_device, renderer->vulkan.command_pool, 1, &buf));
    //Wait until the queue is idle to continue with the program
    VULKAN_ERROR(vkQueueWaitIdle(renderer->vulkan.graphics_queue), "Failed to wait for commands to complete",
                          vkFreeCommandBuffers(renderer->vulkan.log_device, renderer->vulkan.command_pool, 1, &buf));

    //Free command buffer memory
    vkFreeCommandBuffers(renderer->vulkan.log_device, renderer->vulkan.command_pool, 1, &buf);

    return 1;
}

//Gets the index of the memory type that matches our desired properties
uint8_t BOBi_vk_find_memory_type(BOBi_Renderer_Impl *renderer, uint32_t type_filter, VkMemoryPropertyFlags properties, uint32_t *out) {
    //Getting the properties used on our current physical device
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(renderer->vulkan.phy_device, &mem_properties);

    //Search to find the one that matches our desired properties
    for(size_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            *out = i;
            return 1;
        }
    }

    //Throw an error on failure
    printf("Failed to find suitable memory type\n");
    return 0;
}

void BOBi_vk_destroy_image(BOBi_Renderer_Impl *renderer, BOBi_Vulkan_Image *tex) {
    vkWaitForFences(renderer->vulkan.log_device, 1, &renderer->vulkan.draw_fence, VK_TRUE, UINT64_MAX); //Need to wait for the GPU to stop using these resources
    if(tex->descriptor != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(renderer->vulkan.log_device, renderer->vulkan.descriptor_pool, 1, &tex->descriptor);
        tex->descriptor = VK_NULL_HANDLE;
    }
    if(tex->view != VK_NULL_HANDLE) {
        vkDestroyImageView(renderer->vulkan.log_device, tex->view, NULL);
        tex->view = VK_NULL_HANDLE;
    }
    if(tex->image != VK_NULL_HANDLE) {
        vkDestroyImage(renderer->vulkan.log_device, tex->image, NULL);
        tex->image = VK_NULL_HANDLE;
    }
    if(tex->memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->vulkan.log_device, tex->memory, NULL);
        tex->memory = VK_NULL_HANDLE;
    }
}

//Creates an image and its allocated memory
uint8_t BOBi_vk_create_image(BOBi_Renderer_Impl *renderer, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, BOBi_Vulkan_Image *out_image) {
    //Creating the struct that holds the image properties
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D, .format = format,
        .extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = tiling,
        .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VULKAN_ERROR(vkCreateImage(renderer->vulkan.log_device, &image_info, NULL, &out_image->image), "Failed to create image");

    //Get the memory requirements to store the image
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(renderer->vulkan.log_device, out_image->image, &mem_req);

    //Allocate the memory to store the image data
    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL, .allocationSize = mem_req.size };
    VULKAN_ERROR(!BOBi_vk_find_memory_type(renderer, mem_req.memoryTypeBits, properties, &alloc_info.memoryTypeIndex), "Failed to find memory type",
                 vkDestroyImage(renderer->vulkan.log_device, out_image->image, NULL); out_image->image = NULL);
    VULKAN_ERROR(vkAllocateMemory(renderer->vulkan.log_device, &alloc_info, NULL, &out_image->memory), "Failed to create image memory", BOBi_vk_destroy_image(renderer, out_image));
    //Bind the memory to the image properties
    VULKAN_ERROR(vkBindImageMemory(renderer->vulkan.log_device, out_image->image, out_image->memory, 0), "Failed to bind image memory", BOBi_vk_destroy_image(renderer, out_image));

    return 1;
}

//Creates a view for an image
uint8_t BOBi_vk_create_image_view(BOBi_Renderer_Impl *renderer, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkImageView *out) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = NULL,
        .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = format,
        .subresourceRange = {.aspectMask = aspect_flags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
    };
    VULKAN_ERROR(vkCreateImageView(renderer->vulkan.log_device, &view_info, NULL, out), "Failed to create image view");

    return 1;
}

void BOBi_vk_destroy_image_view(BOBi_Renderer_Impl *renderer, VkImageView *view) {
    if(*view != VK_NULL_HANDLE) vkDestroyImageView(renderer->vulkan.log_device, *view, NULL);
    *view = VK_NULL_HANDLE;
}

//Returns a format that supports our given features from a list of candidates, or throws an error on failure
uint8_t BOBi_vk_find_supported_format(BOBi_Renderer_Impl *renderer, VkFormat *candidates, size_t num_candidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat *out) {
    for(size_t i = 0; i < num_candidates; i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(renderer->vulkan.phy_device, candidates[i], &props);

        if(((tiling == VK_IMAGE_TILING_LINEAR) && ((props.linearTilingFeatures & features) == features)) ||
           ((tiling == VK_IMAGE_TILING_OPTIMAL) && ((props.optimalTilingFeatures & features) == features))) {
            *out = candidates[i];
            return 1;
        }
    }

    printf("Failed to find supported format\n");
    return 0;
}

//Returns the format used by our depth image
uint8_t BOBi_vk_find_depth_format(BOBi_Renderer_Impl *renderer, VkFormat *out) {
    return BOBi_vk_find_supported_format(renderer, (VkFormat[3]){VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 3, 
                                 VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, out);
}

//Destroys a BOBi_Vulkan_Buffer
void BOBi_vk_destroy_buffer(VkDevice device, BOBi_Vulkan_Buffer *buf) {
    if(buf->buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buf->buffer, NULL);
    if(buf->memory != VK_NULL_HANDLE) vkFreeMemory(device, buf->memory, NULL);
    buf->memory = VK_NULL_HANDLE;
    buf->buffer = VK_NULL_HANDLE;
}

//Creates buffers in GPU memory
uint8_t BOBi_vk_create_buffer(BOBi_Renderer_Impl *renderer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BOBi_Vulkan_Buffer *out_buf) {
    //Creates the VKBuffer struct that stores the buffer's properties
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = NULL,
        .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VULKAN_ERROR(vkCreateBuffer(renderer->vulkan.log_device, &buf_info, NULL, &out_buf->buffer), "Failed to create a buffer");

    //Getting the memory requirements for this buffer
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(renderer->vulkan.log_device, out_buf->buffer, &mem_req);

    //Allocating the memory region to store this buffer
    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL,
        .allocationSize = mem_req.size,
    };

    if(!BOBi_vk_find_memory_type(renderer, mem_req.memoryTypeBits, properties, &mem_alloc_info.memoryTypeIndex)) return 0;
    VULKAN_ERROR(vkAllocateMemory(renderer->vulkan.log_device, &mem_alloc_info, NULL, &out_buf->memory), "Failed to allocate vertex buffer memory",
                          vkDestroyBuffer(renderer->vulkan.log_device, out_buf->buffer, NULL));
    //Bind the memory to this buffer properties struct
    VULKAN_ERROR(vkBindBufferMemory(renderer->vulkan.log_device, out_buf->buffer, out_buf->memory, 0), "Failed to bind buffer memory", 
                          BOBi_vk_destroy_buffer(renderer->vulkan.log_device, out_buf));

    return 1;
}

//Streams data into a BOBi_Vulkan_Buffer
uint8_t BOBi_vk_stream_to_buffer(VkDevice device, const void *src, size_t size, BOBi_Vulkan_Buffer *dst) {
    void *data;
    VULKAN_ERROR(vkMapMemory(device, dst->memory, 0, size, 0, &data), "Failed to map GPU memory to CPU memory");
    memcpy(data, src, size);
    vkUnmapMemory(device, dst->memory);
    return 1;
}

//Copies the data from a buffer to an image's data memory
void BOBi_vk_copy_buffer_to_image(VkCommandBuffer command_buf, VkBuffer buffer, VkImage image, BOB_Quad sub_rect) {
    VkBufferImageCopy region = {
        .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = {sub_rect.x, sub_rect.y, 0},
        .imageExtent = {sub_rect.w, sub_rect.h, 1}
    };
    vkCmdCopyBufferToImage(command_buf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

//Copies a certain amount of data from one buffer to another buffer. Assumes that the copy/sources ranges always start at 0
uint8_t BOBi_vk_copy_buffer(BOBi_Renderer_Impl *renderer, VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize sz) {
    //Begin a local command buffer
    VkCommandBuffer command_copy_buffer;
    if(!BOBi_vk_begin_single_time_commands(renderer, &command_copy_buffer)) return 0;

    //Copy the data
    VkBufferCopy copy_region = {0, 0, sz};
    vkCmdCopyBuffer(command_copy_buffer, src_buf, dst_buf, 1, &copy_region);

    //Destroy the local command buffer
    BOBi_vk_end_single_time_commands(renderer, command_copy_buffer);
    return 1;
}

//Checks if a given physical device is suitable to our needs
//TODO: Add some sort of priority to device selection (e.g. select a dedicated GPU over an integrated one)
uint8_t BOBi_vk_is_device_suitable(VkPhysicalDevice device) {
    //Get the properties of the physical device
    VkPhysicalDeviceProperties dProperties;
    vkGetPhysicalDeviceProperties(device, &dProperties);

    //Check if the physical device supports Vulkan 1.3 API version
    uint8_t supports_vulkan_1_3 = dProperties.apiVersion >= VK_API_VERSION_1_3;

    //Check if any of the queue families support graphics operations
    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, NULL);
    VkQueueFamilyProperties *family_properties = malloc(sizeof(VkQueueFamilyProperties) * num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, family_properties);

    //Iterate and check if the queue families have the graphics bit set
    uint8_t supports_graphics = 0;
    for(size_t i = 0; i < num_queue_families; i++) {
        if(family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            supports_graphics = 1;
            break;
        }
    }
    free(family_properties); //Cleanup

    //Get the extensions included in the device
    uint32_t included_extension_count = 0;
    VkExtensionProperties *available_extensions;
    VULKAN_ENUMERATE(vkEnumerateDeviceExtensionProperties(device, NULL, &included_extension_count, NULL),
                     vkEnumerateDeviceExtensionProperties(device, NULL, &included_extension_count, available_extensions),
                     available_extensions, included_extension_count * sizeof(VkExtensionProperties), "Failed to enumerate instance extensions");

    //Check if the device supports all of our required extensions
    uint8_t all_found = 1;
    for(size_t i = 0; i < num_required_device_extensions; i++) {
        uint8_t found = 0;

        for(size_t j = 0; j < included_extension_count; j++) {
            if(!strcmp(required_device_extensions[i], available_extensions[j].extensionName)) { //Use strcmp to compare extension names
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Missing required extension: %s\n", required_device_extensions[i]);
            all_found = 0;
            break;
        }
    }
    free(available_extensions); //Cleanup

    //Struct chain to get the features of the device
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = NULL,
    };
    VkPhysicalDeviceVulkan13Features vulkan_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &extended_dynamic_state
    };
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &vulkan_13
    };
    vkGetPhysicalDeviceFeatures2(device, &features2);

    uint8_t supports_required_features = features2.features.samplerAnisotropy && vulkan_13.dynamicRendering
        && vulkan_13.synchronization2 && extended_dynamic_state.extendedDynamicState;

    return all_found && supports_graphics && supports_vulkan_1_3 && supports_required_features;
}

//Choose a format that will be used by our swapchain
uint8_t BOBi_vk_choose_swap_surface_format(VkSurfaceFormatKHR *formats, size_t format_sz, VkSurfaceFormatKHR *out) {
    if(format_sz == 0) return 0; //Early exit

    size_t index = format_sz;
    for(size_t i = 0; i < format_sz; i++) {
        if(formats[i].format == VK_FORMAT_R8G8B8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            index = i;
            break;
        }
    }
    *out = (index == format_sz) ? formats[0] : formats[index];
    return 1;
}

//Choose the present mode used by our swapchain
uint8_t BOBi_vk_choose_swap_present_mode(VkPresentModeKHR *modes, size_t mode_sz, VkPresentModeKHR *out) {
    if(mode_sz == 0) return 0; //Early exit

    size_t index = mode_sz;
    for(size_t i = 0; i < mode_sz; i++) {
        if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            index = i;
            break;
        }
    }

    //VK_PRESENT_MODE_FIFO_KHR is guaranteed to be on all devices so can default to it if we don't find any other suitable ones
    *out = (index == mode_sz) ? VK_PRESENT_MODE_FIFO_KHR : modes[index];
    return 1;
}

//Returns the clamped version of a number between a given upper and lower bound
size_t BOBi_clamp(size_t val, size_t min, size_t max) {
    if(val < min) val = min;
    if(val > max) val = max;

    return val;
}

//Get the extent (dimensions) of the images in the swapchain
VkExtent2D BOBi_vk_choose_swap_extent(VkSurfaceCapabilitiesKHR *capabilities, size_t width, size_t height) {
    if(capabilities->currentExtent.width != UINT32_MAX) return capabilities->currentExtent; //If we already have it set to some value, just return that one

    //Otherwise clamp the size to the dimensions of the window
    return (VkExtent2D){BOBi_clamp(width, capabilities->minImageExtent.width, capabilities->maxImageExtent.width),
                        BOBi_clamp(height, capabilities->minImageExtent.height, capabilities->maxImageExtent.height)};
}

//Returns the minimum number of images present in the swapchain
uint32_t BOBi_vk_choose_swap_min_image_count(VkSurfaceCapabilitiesKHR *capabilities) {
    uint32_t min_image_count = (capabilities->minImageCount < 3) ? capabilities->minImageCount : 3; //Defaults to 3

    //If the max is lower than our min, set our min to the max
    if((0 < capabilities->maxImageCount) && (capabilities->maxImageCount < min_image_count)) {
        min_image_count = capabilities->maxImageCount;
    }

    return min_image_count;
}

//Transitions a swapchain image
void BOBi_vk_transition_image_layout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkAccessFlags2 src_access_mask,
                             VkAccessFlags2 dst_access_mask, VkPipelineStageFlags2 src_stage_mask, VkPipelineStageFlags2 dst_stage_mask,
                             VkImageAspectFlags image_aspect_flags, VkCommandBuffer command_buf) {
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = NULL,
        .srcStageMask = src_stage_mask, .srcAccessMask = src_access_mask,
        .dstStageMask = dst_stage_mask, .dstAccessMask = dst_access_mask,
        .oldLayout = old_layout, .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = (VkImageSubresourceRange){
            .aspectMask = image_aspect_flags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = NULL,
        .dependencyFlags = 0, .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(command_buf, &dep_info);
}

//Transitions a non-swapchain images layout
uint8_t BOBi_vk_transition_tex_layout(VkCommandBuffer command_buf, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
    //Deterine the flags for the source and destination changes
    VkPipelineStageFlags2 src_stage;
    VkPipelineStageFlags2 dst_stage;
    VkAccessFlags2 src_access_mask;
    VkAccessFlags2 dst_access_mask;
    if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        src_access_mask = (VkAccessFlags){0};
        dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        src_stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if(old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;

        src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else {
        printf("Unsupported layout transition\n");
        return 0;
    }

    //Transition the image
    BOBi_vk_transition_image_layout(image, old_layout, new_layout, src_access_mask, dst_access_mask, src_stage, dst_stage, VK_IMAGE_ASPECT_COLOR_BIT, command_buf);

    return 1;
}

//================================ INTIALISATION/RENDERER CREATION FUNCTIONS =================================

uint8_t BOBi_vk_init_vulkan(const char **required_extensions, size_t num_extensions) {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pNext = NULL,
        .pApplicationName = "", .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "", .engineVersion = VK_MAKE_VERSION(1, 0, 0), .apiVersion = VK_API_VERSION_1_3,
    };

    //Check if the required GLFW extensions are supported by the Vulkan implementation
    uint32_t included_extension_count = 0;
    VkExtensionProperties *available_extensions;
    VULKAN_ENUMERATE(vkEnumerateInstanceExtensionProperties(NULL, &included_extension_count, NULL),
                     vkEnumerateInstanceExtensionProperties(NULL, &included_extension_count, available_extensions),
                     available_extensions, included_extension_count * sizeof(VkExtensionProperties), "Failed to enumerate instance extensions");

    for(size_t i = 0; i < num_extensions; i++) {
        uint8_t found = 0;

        for(size_t j = 0; j < included_extension_count; j++) {
            if(!strcmp(required_extensions[i], available_extensions[j].extensionName)) {
                found = 1;
                break;
            }
        }

        if (!found) {
            printf("Missing required extension: %s\n", required_extensions[i]);
            free(available_extensions);
            return 0;
        }
    }
    free(available_extensions);

    //Setting up Validation layers
    const char **required_layers = NULL;
    if(ENABLE_VALIDATION_LAYERS) {
        required_layers = validation_layers;
        uint32_t enabled_layers = 0;
        VkLayerProperties *properties;
        VULKAN_ENUMERATE(vkEnumerateInstanceLayerProperties(&enabled_layers, NULL),
                         vkEnumerateInstanceLayerProperties(&enabled_layers, properties),
                         properties, enabled_layers * sizeof(VkLayerProperties), "Failed to enumerate instance layers");

        for(size_t i = 0; i < num_validation_layers; i++) {
            uint8_t found = 0;

            for(size_t j = 0; j < enabled_layers; j++) {
                if(!strcmp(validation_layers[i], properties[j].layerName)) {
                    found = 1;
                    break;
                }
            }

            if (!found) {
                printf("Missing required extension: %s\n", required_extensions[i]);
                free(properties);
                return 0;
            }
        }

        free(properties);
    }

    //Creating the instance itself
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = NULL, .pApplicationInfo = &app_info,
        .enabledExtensionCount = num_extensions, .ppEnabledExtensionNames = required_extensions, .ppEnabledLayerNames = required_layers,
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? num_validation_layers : 0
    };
    VULKAN_ERROR(vkCreateInstance(&create_info, NULL, &bob_state.instance), "Failed to create a vulkan instance");

    return 1;
}

//Picks the physical device to use for this application from all available options
uint8_t BOBi_vk_pick_physical_device(BOBi_Renderer_Impl *renderer) {
    //Get all available physical devices
    uint32_t physical_device_count = 0;
    VkPhysicalDevice *devices;
    VULKAN_ENUMERATE(vkEnumeratePhysicalDevices(bob_state.instance, &physical_device_count, NULL),
                     vkEnumeratePhysicalDevices(bob_state.instance, &physical_device_count, devices),
                     devices, physical_device_count * sizeof(VkPhysicalDevice), "Failed to enumerate physical devices");

    //Early exit if there aren't any
    if(physical_device_count == 0) {
        printf("Failed to find GPUs with Vulkan support\n");
        free(devices);
        return 0;
    }

    //Otherwise pick the first suitable one
    for(size_t i = 0; i < physical_device_count; i++) {
        if(BOBi_vk_is_device_suitable(devices[i])) {
            renderer->vulkan.phy_device = devices[i];
            free(devices);
            return 1;
        }
    }
    free(devices);

    printf("Failed to find a suitable GPU\n");
    return 0;
}

//Create a virtual device representation to liase with the physical hardware
uint8_t BOBi_vk_create_logical_device(BOBi_Renderer_Impl *renderer) {
    //Get all of the queue families present on the physical device
    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->vulkan.phy_device, &num_queue_families, NULL);
    VkQueueFamilyProperties *family_properties = malloc(sizeof(VkQueueFamilyProperties) * num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(renderer->vulkan.phy_device, &num_queue_families, family_properties);

    //Check if any of the queue families support graphics operations
    renderer->vulkan.queue_family = num_queue_families;
    for(size_t i = 0; i < num_queue_families; i++) {
        uint32_t res;
        VULKAN_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(renderer->vulkan.phy_device, i, renderer->vulkan.surface, &res), "Could not get device surface support");
        if(family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && res) {
            renderer->vulkan.queue_family = i;
            break;
        }
    }
    free(family_properties);

    if(renderer->vulkan.queue_family == num_queue_families) { //Early exit if no condition is met
        printf("No device queue supports graphics operations\n");
        return 0;
    }

    //Info for creating the graphics queue
    float queue_priority = 0.5f;
    VkDeviceQueueCreateInfo device_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .pNext = NULL,
        .queueFamilyIndex = renderer->vulkan.queue_family, .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    //Create a chain of feature structures:
    //Enable extended dynamic state from the extension
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = NULL,
        .extendedDynamicState = VK_TRUE,
    };

    //Enable dynamic rendering from Vulkan 1.3
    VkPhysicalDeviceVulkan13Features vulkan13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &extendedDynamicStateFeatures,
        .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE,
    };

    //Enable shader draw parameters from Vulkan 1.1
    VkPhysicalDeviceVulkan11Features vulkan11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vulkan13Features,
        .shaderDrawParameters = VK_TRUE,
    };

    //Empty for now
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan11Features,
        .features = {.samplerAnisotropy = VK_TRUE},
    };

    //Create the logical device
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &device_queue_create_info,
        .enabledExtensionCount = num_required_device_extensions,
        .ppEnabledExtensionNames = required_device_extensions
    };
    VULKAN_ERROR(vkCreateDevice(renderer->vulkan.phy_device, &device_create_info, NULL, &renderer->vulkan.log_device), "Failed to create logical device");
    vkGetDeviceQueue(renderer->vulkan.log_device, renderer->vulkan.queue_family, 0, &renderer->vulkan.graphics_queue); //Get the reference to the graphics queue

    return 1;
}

//Create the swapchain used to render images to the screen
uint8_t BOBi_vk_create_swapchain(BOBi_Renderer_Impl *renderer, size_t width, size_t height) {
    //Get the surface capabilities
    VkSurfaceCapabilitiesKHR sur_cap;
    VULKAN_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->vulkan.phy_device, renderer->vulkan.surface, &sur_cap), "Could not get surface capabilities");

    //Get swapchain properties
    renderer->vulkan.extent = BOBi_vk_choose_swap_extent(&sur_cap, width, height);
    uint32_t min_image_count = BOBi_vk_choose_swap_min_image_count(&sur_cap);

    //Get the available surface formats
    uint32_t surface_format_count = 0;
    VkSurfaceFormatKHR *surface_formats;
    VULKAN_ENUMERATE(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->vulkan.phy_device, renderer->vulkan.surface, &surface_format_count, NULL),
                     vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->vulkan.phy_device, renderer->vulkan.surface, &surface_format_count, surface_formats),
                     surface_formats, surface_format_count * sizeof(VkSurfaceFormatKHR), "Failed to get surface formats\n");
    //Pick the swapchain surface format we will use
    if(!BOBi_vk_choose_swap_surface_format(surface_formats, surface_format_count, &renderer->vulkan.format)) {
        printf("Failed to choose the swap chain surface format\n");
        free(surface_formats);
        return 0;
    }

    //Get the available present modes
    uint32_t available_present_modes = 0;
    VkPresentModeKHR *present_modes;
    VULKAN_ENUMERATE(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->vulkan.phy_device, renderer->vulkan.surface, &available_present_modes, NULL),
                     vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->vulkan.phy_device, renderer->vulkan.surface, &available_present_modes, present_modes),
                     present_modes, available_present_modes * sizeof(VkPresentModeKHR), "Failed to get surface modes\n");
    //Get the present mode we will use
    VkPresentModeKHR chosen_mode;
    if(!BOBi_vk_choose_swap_present_mode(present_modes, available_present_modes, &chosen_mode)) {
        printf("Failed to choose a present mode\n");
        free(present_modes);
        return 0;
    }

    //Cleanup
    free(surface_formats);
    free(present_modes);

    //Create the swapchain
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .pNext = NULL,
        .surface = renderer->vulkan.surface, .minImageCount = min_image_count,
        .imageFormat = renderer->vulkan.format.format,
        .imageColorSpace = renderer->vulkan.format.colorSpace,
        .imageExtent = renderer->vulkan.extent,
        .imageArrayLayers = 1, //Specifies num layers each image consists of. Always 1 unless making a stereoscopic 3D app
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, //Specifies what kind of operations we use the images in the swap chain for
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, //Specifies how to handle swap chain images that might be used across multiple queue families
        .preTransform = sur_cap.currentTransform, //Can specify that certain transforms can be applied to images in the swap chain
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, //Specifies if the alpha channel should be used for blending with other windows
        .presentMode = chosen_mode,
        .clipped = 1, .oldSwapchain = VK_NULL_HANDLE,
    };
    VULKAN_ERROR(vkCreateSwapchainKHR(renderer->vulkan.log_device, &swapchain_create_info, NULL, &renderer->vulkan.swapchain), "Failed to create swapchain");

    //Get a reference to the swapchain images
    VULKAN_ENUMERATE(vkGetSwapchainImagesKHR(renderer->vulkan.log_device, renderer->vulkan.swapchain, &renderer->vulkan.num_images, NULL),
                     vkGetSwapchainImagesKHR(renderer->vulkan.log_device, renderer->vulkan.swapchain, &renderer->vulkan.num_images, renderer->vulkan.images),
                     renderer->vulkan.images, renderer->vulkan.num_images * sizeof(VkImage), "Failed to get the swapchain images");


    return 1;
}

//Creates views for the swapchain images
uint8_t BOBi_vk_create_image_views(BOBi_Renderer_Impl *renderer) {
    renderer->vulkan.views = malloc(sizeof(VkImageView) * renderer->vulkan.num_images);

    for(size_t i = 0; i < renderer->vulkan.num_images; i++) {
        if(!BOBi_vk_create_image_view(renderer, renderer->vulkan.images[i], renderer->vulkan.format.format, VK_IMAGE_ASPECT_COLOR_BIT, &renderer->vulkan.views[i])) return 0;
    }

    return 1;
}

//Creates a command pool
uint8_t BOBi_vk_create_command_pool(BOBi_Renderer_Impl *renderer) {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = renderer->vulkan.queue_family
    };
    VULKAN_ERROR(vkCreateCommandPool(renderer->vulkan.log_device, &pool_info, NULL, &renderer->vulkan.command_pool), "Failed to create command pool");
    return 1;
}

//Creates the resources used to do depth culling
uint8_t BOBi_vk_create_depth_resources(BOBi_Renderer_Impl *renderer) {
    VkFormat depth_format;
    if(!BOBi_vk_find_depth_format(renderer, &depth_format)) return 0;

    if(!BOBi_vk_create_image(renderer, renderer->vulkan.extent.width, renderer->vulkan.extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &renderer->vulkan.depth)) return 0;
    if(!BOBi_vk_create_image_view(renderer, renderer->vulkan.depth.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, &renderer->vulkan.depth.view)) return 0;

    return 1;
}

//Creates the descriptor pools that hold the information on the data we send to the GPU
uint8_t BOBi_vk_create_descriptor_pool(BOBi_Renderer_Impl *renderer) {
    VkDescriptorPoolSize pool_size[2] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1}
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 
        .maxSets = BOB_MAX_MATERIAL_CAPACITY, .poolSizeCount = 2,
        .pPoolSizes = pool_size
    };

    VULKAN_ERROR(vkCreateDescriptorPool(renderer->vulkan.log_device, &pool_info, NULL, &renderer->vulkan.descriptor_pool), "Failed to create descriptor pool");
    return 1;
}

//Creates a texture sampler for the one texture we are using to be sent to the GPU
uint8_t BOBi_vk_create_texture_sampler(BOBi_Renderer_Impl *renderer, VkSampler *sampler) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(renderer->vulkan.phy_device, &properties);
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = NULL,
        .magFilter = VK_FILTER_LINEAR, .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f, .minLod = 0.0f, .maxLod = 0.0f,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VULKAN_ERROR(vkCreateSampler(renderer->vulkan.log_device, &sampler_info, NULL, sampler), "Failed to create texture sampler");
    return 1;
}

//Creates the general command buffers used to generate draw calls
uint8_t BOBi_vk_create_command_buffer(BOBi_Renderer_Impl *renderer, VkCommandBuffer *command_buf) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = NULL,
        .commandPool = renderer->vulkan.command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1
    };
        VULKAN_ERROR(vkAllocateCommandBuffers(renderer->vulkan.log_device, &alloc_info, command_buf), "Failed to allocate command buffers");
    return 1;
}

void BOBi_vk_destroy_sync_objects(BOBi_Renderer_Impl *renderer) {
    vkDeviceWaitIdle(renderer->vulkan.log_device);

    if(renderer->vulkan.present_complete_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(renderer->vulkan.log_device, renderer->vulkan.present_complete_semaphore, NULL);
        renderer->vulkan.present_complete_semaphore = VK_NULL_HANDLE;
    }
    if(renderer->vulkan.draw_fence != VK_NULL_HANDLE) {
        vkDestroyFence(renderer->vulkan.log_device, renderer->vulkan.draw_fence, NULL);
        renderer->vulkan.draw_fence = VK_NULL_HANDLE;
    }
    if(renderer->vulkan.render_finished_semaphore != NULL) {
        for(size_t i = 0; i < renderer->vulkan.num_images; i++) {
            if(renderer->vulkan.render_finished_semaphore[i] != VK_NULL_HANDLE) {
                vkDestroySemaphore(renderer->vulkan.log_device, renderer->vulkan.render_finished_semaphore[i], NULL);
                renderer->vulkan.render_finished_semaphore[i] = VK_NULL_HANDLE;
            }
        }
    }
}

//Creates the semaphores and fences required to synchronise operations
uint8_t BOBi_vk_create_sync_objects(BOBi_Renderer_Impl *renderer) {
    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0 };

    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    renderer->vulkan.render_finished_semaphore = malloc(sizeof(VkSemaphore) * renderer->vulkan.num_images);
    memset(renderer->vulkan.render_finished_semaphore, 0, sizeof(VkSemaphore) * renderer->vulkan.num_images);
    for(size_t i = 0; i < renderer->vulkan.num_images; i++) {
        VULKAN_ERROR(vkCreateSemaphore(renderer->vulkan.log_device, &sem_info, NULL, &renderer->vulkan.render_finished_semaphore[i]), "Failed to create a semaphore",
                        BOBi_vk_destroy_sync_objects(renderer));
    }
    VULKAN_ERROR(vkCreateSemaphore(renderer->vulkan.log_device, &sem_info, NULL, &renderer->vulkan.present_complete_semaphore), "Failed to create a semaphore",
                 BOBi_vk_destroy_sync_objects(renderer));
    VULKAN_ERROR(vkCreateFence(renderer->vulkan.log_device, &fence_info, NULL, &renderer->vulkan.draw_fence), "Failed to create a fence",
                 BOBi_vk_destroy_sync_objects(renderer));

    return 1;
}

//===================================== MATERIAL CREATION =========================================

//Creates a shader module object from a buffer filled with SPIR-V sharder bytecode
//The buffer must be aligned to 4 bytes
uint8_t BOBi_vk_create_shader_module(uint8_t *buf, size_t buf_sz, VkShaderModule *shader_module, VkDevice log_device) {
    //Alignment check
    if(buf_sz % 4 != 0) {
        printf("Byte data not aligned to 4 bytes\n");
        return 0;
    }
    //Create the shader module
    VkShaderModuleCreateInfo create_info = {.codeSize = buf_sz, .pCode = (uint32_t *)buf, .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = NULL};
    VULKAN_ERROR(vkCreateShaderModule(log_device, &create_info, NULL, shader_module), "Failed to create shader module");

    return 1;
}

int BOBi_vk_convert_shader_type(BOB_Shader_Type type) {
    switch (type) {
        case BOB_VERTEX_SHADER: return VK_SHADER_STAGE_VERTEX_BIT;
        case BOB_FRAGMENT_SHADER: return VK_SHADER_STAGE_FRAGMENT_BIT;
        default: return 0;
    }
}

//Creates our graphics pipeline
uint8_t BOBi_vk_create_graphics_pipeline(BOBi_Renderer_Impl *renderer, BOBi_Material_Impl *mat, BOB_Shader_Data *data, size_t num_shaders) {
    VkPipelineShaderStageCreateInfo *shader_stages = malloc(sizeof(VkPipelineShaderStageCreateInfo) * num_shaders);

    for(size_t i = 0; i < num_shaders; i++) {
        if(data[i].type != BOB_VERTEX_SHADER && data[i].type != BOB_FRAGMENT_SHADER) {
            printf("Shader type not supported");
            return 0;
        }

        //Create the shader module to hold the shader
        VkShaderModule shader_module;
        if(!BOBi_vk_create_shader_module((uint8_t *)data[i].shader_code, data[i].code_buf_sz, &shader_module, renderer->vulkan.log_device)) {
            return 0;
        }
        //Telling the pipleine what shader stages we are using
        shader_stages[i] = (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = NULL,
            .stage = BOBi_vk_convert_shader_type(data[i].type), .module = shader_module, .pName = data[i].entrypoint_name
        };
    }

    //Can tell the pipeline what stages we want to be able to change at runtime without having to recreate the whole program
    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .pNext = NULL,
                                                    .dynamicStateCount = 2, .pDynamicStates = dynamic_states};

    //Get the binding and attribute descriptions
    VkVertexInputBindingDescription binding_desc = BOBi_vk_get_binding_desc();
    VkVertexInputAttributeDescription attrib_descs[4];
    size_t num_attrib_descs;
    BOBi_vk_get_attrib_descs(attrib_descs, &num_attrib_descs);

    //Get the data on the vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .pNext = NULL,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = num_attrib_descs, .pVertexAttributeDescriptions = attrib_descs
    };

    //Tell the shader we will be outputting triangle data
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .pNext = NULL,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkViewport viewport = {0.0f, 0.0f, renderer->vulkan.extent.width, renderer->vulkan.extent.height, 0.0f, 1.0f}; //Viewport rectangle
    VkRect2D scissor = {(VkOffset2D){0, 0}, renderer->vulkan.extent}; //Scissor rectangle
    VkPipelineViewportStateCreateInfo viewport_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .pNext = NULL, .viewportCount = 1, .scissorCount = 1};

    //TODO: CHECK THAT THE OLD VERSION WORKS
    VkPipelineRasterizationStateCreateInfo rasteriser = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .pNext = NULL,
        .depthClampEnable = VK_FALSE, //If set to true, fragments beyond near and far planes are clamped to them instead of discarded
        .rasterizerDiscardEnable = VK_FALSE, //If set to true, then geometry never passes through rasteriser stage. Disables output to framebuffer
        .polygonMode = VK_POLYGON_MODE_FILL, //Determines how fragments are generated for geometry. Can also be drawn as lines or points
        .cullMode = VK_CULL_MODE_NONE, //Determines what kind of face culling to use
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, //Determines the vertex order for the faces to be considered front facing and can be clockwise or counter-clockwise
        .depthBiasEnable = VK_FALSE, //Rasteriser can alter the depth values by adding a constant value or biasing them based on a fragments slope. Not necessary
        .lineWidth = 1.0f //Determines the thickness of lines in terms of fragments
    };

    //Configure Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .pNext = NULL,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE
    };

    //Configuring Colour Blending
    VkPipelineColorBlendAttachmentState colour_blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colour_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .pNext = NULL,
        .logicOpEnable = VK_FALSE, .logicOp = VK_LOGIC_OP_COPY, .attachmentCount = 1, .pAttachments = &colour_blend_attachment
    };

    //Creating the pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = NULL,
        .setLayoutCount = 2, .pSetLayouts = (VkDescriptorSetLayout[2]){mat->vulkan.uniform_set_layout, renderer->vulkan.default_tex_layout}, .pushConstantRangeCount = 0
    };
    VULKAN_ERROR(vkCreatePipelineLayout(renderer->vulkan.log_device, &pipeline_layout_info, NULL, &mat->vulkan.layout), "Failed to create pipeline layout",
                 for(size_t i = 0; i < num_shaders; i++) { vkDestroyShaderModule(renderer->vulkan.log_device, shader_stages[i].module, NULL); } free(shader_stages););

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .pNext = NULL,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &renderer->vulkan.format.format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
    };
    VULKAN_ERROR(!BOBi_vk_find_depth_format(renderer, &pipeline_rendering_create_info.depthAttachmentFormat), "Failed to create pipeline layout",
                 for(size_t i = 0; i < num_shaders; i++) { vkDestroyShaderModule(renderer->vulkan.log_device, shader_stages[i].module, NULL); } free(shader_stages););

    //Creating the graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &pipeline_rendering_create_info,
        .stageCount = num_shaders, .pStages = shader_stages, .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &input_assembly, .pViewportState = &viewport_state,
        .pRasterizationState = &rasteriser, .pMultisampleState = &multisampling,
        .pColorBlendState = &colour_blending, .pDynamicState = &dynamic_state,
        .layout = mat->vulkan.layout, .renderPass = NULL, .pDepthStencilState = &depth_stencil
    };

    VULKAN_ERROR(vkCreateGraphicsPipelines(renderer->vulkan.log_device, VK_NULL_HANDLE, 1, &graphics_create_info, NULL, &mat->vulkan.pipeline), "Failed to create graphics pipeline",
                 for(size_t i = 0; i < num_shaders; i++) { vkDestroyShaderModule(renderer->vulkan.log_device, shader_stages[i].module, NULL); } free(shader_stages););

    for(size_t i = 0; i < num_shaders; i++) {
        vkDestroyShaderModule(renderer->vulkan.log_device, shader_stages[i].module, NULL);
    }
    free(shader_stages);

    return 1;
}

uint8_t BOBi_std140_alignment(BOB_Uniform_Type type) {
    switch (type) {
        case BOB_UNIFORM_FLOAT:
        case BOB_UNIFORM_UNSIGNED_INT:
        case BOB_UNIFORM_SIGNED_INT: return 4;
        case BOB_UNIFORM_VEC2: return 8;
        case BOB_UNIFORM_VEC3:
        case BOB_UNIFORM_VEC4: return 16;
        case BOB_UNIFORM_MAT4: return 16;
    }
}

uint8_t BOBi_std140_size(BOB_Uniform_Type type) {
    switch (type) {
        case BOB_UNIFORM_FLOAT:
        case BOB_UNIFORM_UNSIGNED_INT:
        case BOB_UNIFORM_SIGNED_INT: return 4;
        case BOB_UNIFORM_VEC2: return 8;
        case BOB_UNIFORM_VEC3:
        case BOB_UNIFORM_VEC4: return 16;
        case BOB_UNIFORM_MAT4: return 64;
    }
}

//Creates a layout for the descriptor set for the data we will be sending to our shader
uint8_t BOBi_vk_create_descriptor_layout(BOBi_Renderer_Impl *renderer, VkDescriptorSetLayout *layout, VkDescriptorType *type, VkShaderStageFlags *stages, size_t *bindings, size_t num_layouts) {
    VkDescriptorSetLayoutBinding layouts[BOB_MAX_UNIFORMS];
    for(size_t i = 0; i < num_layouts; i++) {
        layouts[i] = (VkDescriptorSetLayoutBinding){
            .binding = bindings[i], .descriptorType = type[i], .descriptorCount = 1, .stageFlags = stages[i]
        };
    }

    //Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = NULL,
        .bindingCount = num_layouts, .pBindings = layouts
    };
    VULKAN_ERROR(vkCreateDescriptorSetLayout(renderer->vulkan.log_device, &layout_info, NULL, layout), "Failed to create descriptor set layout");
    return 1;
}

uint8_t BOBi_vk_create_descriptor_sets(BOBi_Renderer_Impl *renderer, VkDescriptorSet *set, VkDescriptorSetLayout *layouts, size_t num_sets) {
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = NULL,
        .descriptorPool = renderer->vulkan.descriptor_pool,
        .descriptorSetCount = num_sets, .pSetLayouts = layouts
    };

    VULKAN_ERROR(vkAllocateDescriptorSets(renderer->vulkan.log_device, &alloc_info, set), "Failed to allocate descriptor sets");

    return 1;
}

void BOBi_vk_destroy_material(BOBi_Renderer_Impl *renderer, uint32_t index) {
    BOBi_Material_Impl *mat = &renderer->material_table[index];
    vkWaitForFences(renderer->vulkan.log_device, 1, &renderer->vulkan.draw_fence, VK_TRUE, UINT64_MAX);
    if(mat->vulkan.uniform_descriptor_set != VK_NULL_HANDLE) {vkFreeDescriptorSets(renderer->vulkan.log_device, renderer->vulkan.descriptor_pool, 1, &mat->vulkan.uniform_descriptor_set); mat->vulkan.uniform_descriptor_set = VK_NULL_HANDLE;}
    if(mat->vulkan.uniform_set_layout != VK_NULL_HANDLE) {vkDestroyDescriptorSetLayout(renderer->vulkan.log_device, mat->vulkan.uniform_set_layout, NULL); mat->vulkan.uniform_set_layout = VK_NULL_HANDLE;}
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &mat->vulkan.uniform_buffer);
    if(mat->vulkan.layout!= VK_NULL_HANDLE) {vkDestroyPipelineLayout(renderer->vulkan.log_device, mat->vulkan.layout, NULL); mat->vulkan.layout = VK_NULL_HANDLE;}
    if(mat->vulkan.pipeline!= VK_NULL_HANDLE) {vkDestroyPipeline(renderer->vulkan.log_device, mat->vulkan.pipeline, NULL); mat->vulkan.pipeline = VK_NULL_HANDLE;}
}

uint8_t BOBi_vk_create_material(BOBi_Renderer_Impl *renderer, uint32_t index, BOB_Shader_Data *data, size_t num_shaders) {
    BOBi_Material_Impl *mat = &renderer->material_table[index];

    size_t offset = 0;
    VkShaderStageFlags uniform_stages = 0;

    //Set the uniform data
    for(size_t i = 0; i < mat->uniform_count; i++) {
        BOBi_Uniform_Impl *uniform = &mat->uniforms[i];
        uniform->vulkan.offset = BOBi_align_up(offset, BOBi_std140_alignment(uniform->type));
        offset = uniform->vulkan.offset + BOBi_std140_size(uniform->type);
        uniform_stages |= uniform->vulkan.stage;
    }

    //Create the descriptor sets and the pipeline
    VULKAN_ERROR(!BOBi_vk_create_descriptor_layout(renderer, &mat->vulkan.uniform_set_layout,
                                        (VkDescriptorType[1]){VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                                        (VkShaderStageFlagBits[1]){uniform_stages}, (size_t[1]){0}, 1),
                 "Failed to create descriptor sets");
    VULKAN_ERROR(!BOBi_vk_create_descriptor_sets(renderer, &mat->vulkan.uniform_descriptor_set, &mat->vulkan.uniform_set_layout, 1),
                 "Failed to create descriptor layouts");
    VULKAN_ERROR(!BOBi_vk_create_graphics_pipeline(renderer, mat, data, num_shaders), "Failed to create graphics pipeline", BOBi_vk_destroy_material(renderer, index));

    //Create the uniform buffer to hold the data:
    VULKAN_ERROR(!BOBi_vk_create_buffer(renderer, offset, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &mat->vulkan.uniform_buffer), "Failed to create uniform buffer", BOBi_vk_destroy_material(renderer, index));

    VULKAN_ERROR(vkMapMemory(renderer->vulkan.log_device, mat->vulkan.uniform_buffer.memory, 0, offset, 0, &mat->vulkan.uniform_buffer_mapped),
                 "Failed to map uniform buffer memory", BOBi_vk_destroy_material(renderer, index));

    return 1;
}

void BOBi_vk_write_image_descriptor(BOBi_Renderer_Impl *renderer, VkDescriptorSet set, VkDescriptorType type, size_t binding, uint32_t index) {
    VkDescriptorImageInfo descriptor_images = (VkDescriptorImageInfo) {
        .sampler = renderer->vulkan.sampler, .imageView = renderer->texture_table[index].vulkan.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkWriteDescriptorSet descriptor_writes = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL,
        .dstSet = set, .dstBinding = binding,
        .descriptorCount = 1, .descriptorType = type,
        .pImageInfo = &descriptor_images
    };
    //Create the descriptor to hold the uniforms
    vkUpdateDescriptorSets(renderer->vulkan.log_device, 1, &descriptor_writes, 0, NULL);
}

void BOBi_vk_write_buffer_descriptor(BOBi_Renderer_Impl *renderer, VkDescriptorSet set, VkDescriptorType type, size_t binding, BOBi_Vulkan_Buffer buf, VkDeviceSize range) {
    VkDescriptorBufferInfo descriptor_buffer = (VkDescriptorBufferInfo) {
        .buffer = buf.buffer, .offset = 0, .range = range
    };
    VkWriteDescriptorSet descriptor_writes = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL,
        .dstSet = set, .dstBinding = binding,
        .descriptorCount = 1, .descriptorType = type,
        .pBufferInfo = &descriptor_buffer,
    };
    //Create the descriptor to hold the uniforms
    vkUpdateDescriptorSets(renderer->vulkan.log_device, 1, &descriptor_writes, 0, NULL);
}

//Updates the uniform buffer with new texture position
void BOBi_vk_update_uniform(BOBi_Renderer_Impl *renderer, BOBi_Material_Impl *mat) {
    for(size_t i = 0; i < mat->uniform_count; i++) {
        BOBi_Uniform_Impl *uniform = &mat->uniforms[i];
        //Copying data
        switch (uniform->type) {
            case BOB_UNIFORM_FLOAT: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.f, sizeof(float)); break;
            case BOB_UNIFORM_UNSIGNED_INT:memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.u32, sizeof(uint32_t)); break;
            case BOB_UNIFORM_SIGNED_INT: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.i32, sizeof(int32_t)); break;
            case BOB_UNIFORM_VEC2: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.vec2, sizeof(BOB_Vector2)); break;
            case BOB_UNIFORM_VEC3: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.vec3, sizeof(BOB_Vector3)); break;
            case BOB_UNIFORM_VEC4: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.vec4, sizeof(BOB_Vector4)); break;
            case BOB_UNIFORM_MAT4: memcpy(mat->vulkan.uniform_buffer_mapped + uniform->vulkan.offset, &uniform->value.mat4, sizeof(BOB_Mat4)); break;
        }
    }
}

//Initialises vulkan
uint8_t BOBi_vk_init_vulkan_renderer(BOBi_Renderer_Impl *renderer, size_t width, size_t height, size_t index_buf_sz, size_t vert_buf_sz, BOB_vk_create_surface surface_func) {
    VULKAN_ERROR(!surface_func(bob_state.instance, &renderer->vulkan.surface), "Didn't create surface");
    VULKAN_ERROR(!BOBi_vk_pick_physical_device(renderer), "Didn't pick physical device");
    VULKAN_ERROR(!BOBi_vk_create_logical_device(renderer), "Didn't create logical device");
    VULKAN_ERROR(!BOBi_vk_create_swapchain(renderer, width, height), "Didn't create swapchain");
    VULKAN_ERROR(!BOBi_vk_create_image_views(renderer), "Didn't create swapchain images");
    VULKAN_ERROR(!BOBi_vk_create_depth_resources(renderer), "Didn't create depth resources");
    VULKAN_ERROR(!BOBi_vk_create_command_pool(renderer), "Didn't create command pool");
    VULKAN_ERROR(!BOBi_vk_create_descriptor_pool(renderer), "Didn't create descriptor pool");
    VULKAN_ERROR(!BOBi_vk_create_descriptor_layout(renderer, &renderer->vulkan.default_tex_layout, 
                (VkDescriptorType[1]){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}, (VkShaderStageFlags[1]){VK_SHADER_STAGE_FRAGMENT_BIT},
                (size_t[1]){0}, 1), "Failed to create texture descriptor layout");
    VULKAN_ERROR(!BOBi_vk_create_texture_sampler(renderer, &renderer->vulkan.sampler), "Didn't create texture sampler");
    VULKAN_ERROR(!BOBi_vk_create_command_buffer(renderer, &renderer->vulkan.command_buffer), "Failed to creation command buffer");
    VULKAN_ERROR(!BOBi_vk_create_sync_objects(renderer), "Failed to create sync objects");

    //Create the vertex buffer
    VULKAN_ERROR(!BOBi_vk_create_buffer(renderer, vert_buf_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                &renderer->vulkan.vertex_buffer), "Failed to create vertex buffer");
    //Create the index buffer
    VULKAN_ERROR(!BOBi_vk_create_buffer(renderer, index_buf_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  &renderer->vulkan.index_buffer), "Failed to create index buffer");

    //Create the staging buffer
    if(!BOBi_vk_create_buffer(renderer, vert_buf_sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &renderer->vulkan.vert_staging_buf)) return 0;
    if(!BOBi_vk_create_buffer(renderer, index_buf_sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &renderer->vulkan.index_staging_buf)) return 0;
    if(!BOBi_vk_create_buffer(renderer, 4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &renderer->vulkan.pbo_staging_buf)) return 0;
    renderer->vulkan.pbo_staging_buf_sz = 4096;

    return 1;
}

uint8_t BOB_create_vulkan_renderer(size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity,
                            size_t font_capacity, size_t width, size_t height, size_t width_px, size_t height_px, size_t vertex_capacity,
                            size_t index_capacity, size_t draw_call_capacity, BOB_vk_create_surface surface_func, BOB_Renderer_Handle *r) {
    if(!BOBi_create_renderer(BOB_VULKAN_RENDERER, atlas_capacity, pixelbuf_capacity, tex_capacity, mat_capacity, font_capacity, vertex_capacity, index_capacity, draw_call_capacity, width, height, r)) return 0;

    BOBi_Renderer_Impl *intrn_renderer;
    BOBi_get_renderer(*r, &intrn_renderer);

    //Vulkan specific initialisation
    if(!BOBi_vk_init_vulkan_renderer(intrn_renderer, width_px, height_px, sizeof(uint32_t) * index_capacity, sizeof(BOBi_Render_Vertex) * vertex_capacity, surface_func)) {
        BOB_destroy_renderer(r);
        return 0;
    }

    //Create the default texture used
    if(!BOB_create_texture(*r, 1, 1, (uint8_t[4]){255, 255, 255, 255}, BOB_RGBA, &intrn_renderer->default_tex)) {
        BOB_destroy_renderer(r);
        return 0;
    }

    //Update the default projection matrix
    BOB_ortho_vk( 0.0f, intrn_renderer->screen_width, intrn_renderer->screen_height, 0.0f, 0.0f, BOB_MAX_LAYER, &intrn_renderer->projection);

    if(!BOB_create_material(*r, (BOB_Shader_Data[2]){
        (BOB_Shader_Data){.shader_code = (const char *)shader_vert_spv, .code_buf_sz = shader_vert_spv_len, .entrypoint_name = "main", .type = BOB_VERTEX_SHADER},
        (BOB_Shader_Data){.shader_code = (const char *)shader_frag_spv, .code_buf_sz = shader_frag_spv_len, .entrypoint_name = "main", .type = BOB_FRAGMENT_SHADER},
    }, 2, (BOB_Uniform[1]){ BOB_uniform_mat4("uProjection", intrn_renderer->projection, BOB_VERTEX_SHADER) }, 1, &intrn_renderer->default_mat)) {
        BOB_destroy_renderer(r);
        return 0;
    }
    return 1;
}

void BOBi_vk_destroy_renderer(BOBi_Renderer_Impl *renderer) {
    vkDeviceWaitIdle(renderer->vulkan.log_device); //Need to wait for the GPU to stop using these resources
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &renderer->vulkan.vertex_buffer);
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &renderer->vulkan.index_buffer);
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &renderer->vulkan.index_staging_buf);
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &renderer->vulkan.vert_staging_buf);
    BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &renderer->vulkan.pbo_staging_buf);
    vkDestroyDescriptorSetLayout(renderer->vulkan.log_device, renderer->vulkan.default_tex_layout, NULL);
    BOBi_vk_destroy_image(renderer, &renderer->vulkan.depth);

    for(size_t i = 0; i < renderer->vulkan.num_images; i++) {
        vkDestroyImageView(renderer->vulkan.log_device, renderer->vulkan.views[i], NULL);
    }

    free(renderer->vulkan.views);
    free(renderer->vulkan.images);

    vkFreeCommandBuffers(renderer->vulkan.log_device, renderer->vulkan.command_pool, 1, &renderer->vulkan.command_buffer);
    BOBi_vk_destroy_sync_objects(renderer);
    free(renderer->vulkan.render_finished_semaphore);
    vkDestroySemaphore(renderer->vulkan.log_device, renderer->vulkan.present_complete_semaphore, NULL);
    vkDestroyFence(renderer->vulkan.log_device, renderer->vulkan.draw_fence, NULL);

    vkDestroySampler(renderer->vulkan.log_device, renderer->vulkan.sampler, NULL);
    vkDestroyDescriptorPool(renderer->vulkan.log_device, renderer->vulkan.descriptor_pool, NULL);
    vkDestroyCommandPool(renderer->vulkan.log_device, renderer->vulkan.command_pool, NULL);
    vkDestroySwapchainKHR(renderer->vulkan.log_device, renderer->vulkan.swapchain, NULL);
    vkDestroyDevice(renderer->vulkan.log_device, NULL);
    vkDestroySurfaceKHR(bob_state.instance, renderer->vulkan.surface, NULL);
}

// ================================= TEXTURE FUNCTIONS ===========================================
void BOBi_vk_destroy_texture(BOBi_Renderer_Impl *renderer, uint32_t tex_index) {
    BOBi_Vulkan_Image *tex = &renderer->texture_table[tex_index].vulkan;

    BOBi_vk_destroy_image(renderer, tex);
}

VkFormat BOBi_vk_convert_format(BOB_Format format) {
    switch (format) {
        case BOB_RED: return VK_FORMAT_R8_UNORM;
        case BOB_RG: return VK_FORMAT_R8G8_UNORM;
        case BOB_RGB: return VK_FORMAT_R8G8B8_UNORM;
        case BOB_RGBA: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

//TODO: Merge BOBi_vk_create_image and BOBi_vk_create_view into this, and parse the BOB_Format into a vulkan format instead of hardcoding it
//      Also let user set the flags? Would need to make BOB API equivalents
//      This would let us simplify this and BOBi_vk_create_depth_resources
uint8_t BOBi_vk_create_texture(BOBi_Renderer_Impl *renderer, uint32_t index, size_t width, size_t height, uint8_t *data, BOB_Format format) {
    BOBi_Vulkan_Image *tex = &renderer->texture_table[index].vulkan;

    //Create the image
    if(!BOBi_vk_create_image(renderer, width, height, BOBi_vk_convert_format(format), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex)) return 0;

    VkCommandBuffer command_buf;
    VULKAN_ERROR(!BOBi_vk_begin_single_time_commands(renderer, &command_buf), "Failed to begin command buffer");
    VULKAN_ERROR(!BOBi_vk_transition_tex_layout(command_buf, tex->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), "Failed to transition layout",
                 BOBi_vk_destroy_image(renderer, tex));

    BOBi_Vulkan_Buffer staging_buf = {0};
    uint8_t has_staging = 0;

    if(data != NULL) {
        //Create the staging buffer
        VULKAN_ERROR(!BOBi_vk_create_buffer(renderer, width * height * (format + 1), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buf), "Failed to create staging buffer",
                     BOBi_vk_destroy_image(renderer, tex));

        has_staging = 1;

        //Map the staging buffer memory into CPU memory and copy the pixel data into it
        VULKAN_ERROR(!BOBi_vk_stream_to_buffer(renderer->vulkan.log_device, data, width * height * (format + 1), &staging_buf), "Failed to stream data into a Vulkan Buffer",
            BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &staging_buf), BOBi_vk_destroy_image(renderer, tex));

        BOBi_vk_copy_buffer_to_image(command_buf, staging_buf.buffer, tex->image, (BOB_Quad){0, 0, width, height});
    }

    VULKAN_ERROR(!BOBi_vk_transition_tex_layout(command_buf, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), "Failed to transition layout",
                 BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &staging_buf), BOBi_vk_destroy_image(renderer, tex));

    VULKAN_ERROR(!BOBi_vk_end_single_time_commands(renderer, command_buf), "Failed to end command buffer", BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &staging_buf), BOBi_vk_destroy_image(renderer, tex));

    if(has_staging) BOBi_vk_destroy_buffer(renderer->vulkan.log_device, &staging_buf);

    //Create view for the image
    VULKAN_ERROR(!BOBi_vk_create_image_view(renderer, tex->image, BOBi_vk_convert_format(format),  VK_IMAGE_ASPECT_COLOR_BIT, &tex->view),
                 "Failed to create view", BOBi_vk_destroy_image(renderer, tex));

    //Create the descriptor set for the image
    VULKAN_ERROR(!BOBi_vk_create_descriptor_sets(renderer, &tex->descriptor, &renderer->vulkan.default_tex_layout, 1),
                 "Failed to create texture descriptor set", BOBi_vk_destroy_image(renderer, tex));

    BOBi_vk_write_image_descriptor(renderer, tex->descriptor, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, index);
    return 1;
}

// ============================================ PBO FUNCTIONS ======================================

void BOBi_vk_copy_data_tex(BOBi_Renderer_Impl *renderer, uint32_t tex_index, BOB_Format format, BOB_Quad region, uint8_t *pixels) {
    BOBi_Vulkan_Buffer staging_buf;
    BOBi_vk_create_buffer(renderer, (region.h * region.w), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buf);
    BOBi_vk_stream_to_buffer(renderer->vulkan.log_device, pixels, region.h * region.w, &staging_buf);

    VkCommandBuffer buf;
    BOBi_vk_begin_single_time_commands(renderer, &buf);
    BOBi_vk_copy_buffer_to_image(buf, staging_buf.buffer, renderer->texture_table[tex_index].vulkan.image, region);
    BOBi_vk_end_single_time_commands(renderer, buf);
}

void BOBi_vk_destroy_pbo(BOBi_Renderer_Impl *r, uint32_t index) {return;}
uint8_t BOBi_vk_create_pbo(BOBi_Renderer_Impl *r, uint32_t index) {return 1;}

uint8_t BOBi_vk_bind_pbo_mem(BOBi_Renderer_Impl *renderer, uint32_t pb_index, void **mapped_mem_ptr, size_t *mem_sz) {
    VULKAN_ERROR(vkMapMemory(renderer->vulkan.log_device, renderer->vulkan.pbo_staging_buf.memory, 0, renderer->vulkan.pbo_staging_buf_sz, 0, mapped_mem_ptr), "Failed to map GPU memory to CPU memory");

    *mem_sz = renderer->pixelbuffer_table[pb_index].buf_sz;
    return 1;
}
void BOBi_vk_unbind_pbo_mem(BOBi_Renderer_Impl *renderer, uint32_t index) {
    vkUnmapMemory(renderer->vulkan.log_device, renderer->vulkan.pbo_staging_buf.memory);
}
void BOBi_vk_upload_pbo_data(BOBi_Renderer_Impl *renderer, uint32_t pb_index) {
    VkCommandBuffer buf;
    BOBi_vk_begin_single_time_commands(renderer, &buf);
    BOBi_Texture_Impl tex = renderer->texture_table[renderer->pixelbuffer_table[pb_index].pixel_tex];
    BOBi_vk_copy_buffer_to_image(buf, renderer->vulkan.pbo_staging_buf.buffer, tex.vulkan.image, (BOB_Quad){0, 0, tex.width, tex.height});
    BOBi_vk_end_single_time_commands(renderer, buf);
}

// ============================================ DESTRUCTION FUNCTIONS ========================================

//Destroys existing memory used by the swapchain
void BOBi_vk_cleanup_swapchain(BOBi_Renderer_Impl *renderer) {
    for(size_t i = 0; i < renderer->vulkan.num_images; i++) {
        vkDestroyImageView(renderer->vulkan.log_device, renderer->vulkan.views[i], NULL);
    }
    free(renderer->vulkan.views);
    free(renderer->vulkan.images);
    vkDestroySwapchainKHR(renderer->vulkan.log_device, renderer->vulkan.swapchain, NULL);
}

//Rebuilds the swapchain on framebuffer resize
uint8_t BOBi_vk_recreate_swapchain(BOBi_Renderer_Impl *renderer) {
    VULKAN_ERROR(vkDeviceWaitIdle(renderer->vulkan.log_device), "Failed to wait for signal");
    BOBi_vk_cleanup_swapchain(renderer);
    vkDestroyImageView(renderer->vulkan.log_device, renderer->vulkan.depth.view, NULL);
    vkDestroyImage(renderer->vulkan.log_device, renderer->vulkan.depth.image, NULL);
    vkFreeMemory(renderer->vulkan.log_device, renderer->vulkan.depth.memory, NULL);
    return BOBi_vk_create_swapchain(renderer, renderer->screen_width_px, renderer->screen_height_px) && BOBi_vk_create_image_views(renderer) && BOBi_vk_create_depth_resources(renderer);
}

// ===================================== DRAWING FUNCTIONS =========================================

uint8_t BOBi_vk_begin_frame(BOBi_Renderer_Impl *renderer) {
    VkCommandBuffer buffer = renderer->vulkan.command_buffer; //Getting a reference to the command buffer so that don't have to write out full code every time
    //Wait until operations from previous frame have completed
    VULKAN_ERROR(vkWaitForFences(renderer->vulkan.log_device, 1, &renderer->vulkan.draw_fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence");

    //Get the next swapchain image
    VkResult res = vkAcquireNextImageKHR(renderer->vulkan.log_device, renderer->vulkan.swapchain, UINT64_MAX,
                                         renderer->vulkan.present_complete_semaphore, VK_NULL_HANDLE, &renderer->vulkan.next_swapchain_image_index);
    //If the swapchain data is invalid, remake it
    if(res == VK_ERROR_OUT_OF_DATE_KHR) {
        BOBi_vk_recreate_swapchain(renderer);
        return 1;
    }
    //If we haven't been able to get a valid image, throw an error
    else if(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        assert(res == VK_TIMEOUT || res == VK_NOT_READY);
        printf("Failed to acquire swap chain image\n");
        return 0;
    }
    vkResetFences(renderer->vulkan.log_device, 1, &renderer->vulkan.draw_fence);

    //CLear the command buffer and record the draw commands for the current frame
    vkResetCommandBuffer(buffer, 0);

    //Begin writing to the command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = NULL
    };
    VULKAN_ERROR(vkBeginCommandBuffer(buffer, &begin_info), "Failed to begin command buffer operations");

    //TODO: FIX THE TRANSITIONING

    //Transition the image to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    BOBi_vk_transition_image_layout(renderer->vulkan.images[renderer->vulkan.next_swapchain_image_index],
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkAccessFlags2){},
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, buffer);

    //Transition depth image to depth attachment optimal layout
    BOBi_vk_transition_image_layout(renderer->vulkan.depth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                            VK_IMAGE_ASPECT_DEPTH_BIT, renderer->vulkan.command_buffer);

    if(renderer->frame_state != 1) renderer->frame_state = 1;

    return 1;
}

uint8_t BOBi_vk_draw(BOBi_Renderer_Impl *renderer) {
    VkCommandBuffer buffer = renderer->vulkan.command_buffer; //Getting a reference to the command buffer so that don't have to write out full code every time
    VkClearValue clear_color = {0};
    VkClearValue clear_depth = {0};
    if(renderer->frame_state == 1) {
        if(renderer->colour != NULL) clear_color = (VkClearValue){.color = {.float32 = {renderer->colour[0], renderer->colour[1], renderer->colour[2], renderer->colour[3]}}}; //Set the colour the screen gets cleared to
        clear_depth = (VkClearValue){.depthStencil = {.depth = 1.0f, .stencil = 0}}; //Set the depth the screen is cleared at
    }

    VkRenderingAttachmentInfo attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = NULL,
            .imageView = renderer->vulkan.views[renderer->vulkan.next_swapchain_image_index], //Specifies which view to render to
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = (renderer->frame_state == 1) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, //Specifies what to do with the image during rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, //Specifies what to do with the image after rendering
        .clearValue = clear_color
    };
    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = NULL,
        .imageView = renderer->vulkan.depth.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = (renderer->frame_state == 1) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD, //Specifies what to do with the image during rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //Specifies what to do with the image after rendering
        .clearValue = clear_depth
    };

    //Set the rendering info
    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = NULL,
        .renderArea = {.offset = {0, 0}, .extent = renderer->vulkan.extent},
        .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };

    size_t index_sz = sizeof(uint32_t) * renderer->batch.num_indices;
    size_t vertex_sz = sizeof(BOBi_Render_Vertex) * renderer->batch.num_vertices;
    //Map the staging buffer to CPU memory and copy the index data into it
    VULKAN_ERROR(!BOBi_vk_stream_to_buffer(renderer->vulkan.log_device, renderer->batch.vertex_arena.memory, index_sz, &renderer->vulkan.index_staging_buf), "Failed to stream data into a Vulkan Buffer");
    //Copy the data
    VkBufferCopy copy_region = {0, 0, index_sz};
    vkCmdCopyBuffer(buffer, renderer->vulkan.index_staging_buf.buffer, renderer->vulkan.index_buffer.buffer, 1, &copy_region);

    //Map the staging buffer to CPU memory and copy the vertex data into it
    VULKAN_ERROR(!BOBi_vk_stream_to_buffer(renderer->vulkan.log_device, renderer->batch.vertex_arena_2.memory, vertex_sz, &renderer->vulkan.vert_staging_buf), "Failed to stream data into a Vulkan Buffer");
    //Create the actual destination buffer and copy the staging buffer data into it
    copy_region = (VkBufferCopy){0, 0, vertex_sz};
    vkCmdCopyBuffer(buffer, renderer->vulkan.vert_staging_buf.buffer, renderer->vulkan.vertex_buffer.buffer, 1, &copy_region);

    // IMPORTANT: synchronize the copies with vertex/index input
    VkBufferMemoryBarrier2 barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

            .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,

            .buffer = renderer->vulkan.vertex_buffer.buffer,
            .offset = 0,
            .size = vertex_sz,
        },

        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

            .dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,

            .buffer = renderer->vulkan.index_buffer.buffer,
            .offset = 0,
            .size = index_sz,
        }
    };

    VkDependencyInfo dependency_info = { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .bufferMemoryBarrierCount = 2, .pBufferMemoryBarriers = barriers, };

    vkCmdPipelineBarrier2(buffer, &dependency_info);

    //Begin rendering
    vkCmdBeginRendering(buffer, &rendering_info);

    VkViewport viewport = { .x = 0.0f, .y = (float)renderer->vulkan.extent.height, .width = (float)renderer->vulkan.extent.width, .height = -(float)renderer->vulkan.extent.height, .minDepth = 0.0f, .maxDepth = 1.0f };

    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = renderer->vulkan.extent
    };

    vkCmdSetScissor(buffer, 0, 1, &scissor);

    uint32_t index;
    BOBi_get_index_from_handle(renderer->default_mat, &index);
    BOBi_Material_Impl *mat = &renderer->material_table[index];

    //Bind the vertex buffer to the command buffer
    vkCmdBindVertexBuffers(buffer, 0, 1, &renderer->vulkan.vertex_buffer.buffer, (VkDeviceSize[]){0});
    //Bind the index buffer to the command buffer
    vkCmdBindIndexBuffer(buffer, renderer->vulkan.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    uint32_t old_mat = UINT32_MAX, old_tex = UINT32_MAX;
    for(size_t i = 0; i < renderer->batch.num_draw_calls; i++) {
        BOBi_Draw_Call call = BOBi_get_arena_elem(renderer->batch.draw_call_arena, i, BOBi_Draw_Call);

        uint32_t mat_index, tex_index;
        if(!BOBi_get_index_from_handle(call.mat, &mat_index)) return 0;
        if(!BOBi_get_index_from_handle(call.tex, &tex_index)) return 0;

        BOBi_Material_Impl *mat = &renderer->material_table[mat_index];
        if(old_mat != mat_index) {
            BOBi_vk_update_uniform(renderer, mat);
            BOBi_vk_write_buffer_descriptor(renderer, mat->vulkan.uniform_descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, mat->vulkan.uniform_buffer, mat->uniforms[mat->uniform_count-1].vulkan.offset + BOBi_std140_size(mat->uniforms[mat->uniform_count-1].type));

            //Bind the graphics pipeline
            vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->vulkan.pipeline);
            //Bind correct descriptor set for each frame to the descriptors in the shader
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->vulkan.layout, 0, 1, &mat->vulkan.uniform_descriptor_set, 0, NULL);
        }
        if(old_tex != tex_index) {
            //Set the material to use this texture.
            BOBi_Texture_Impl *tex = &renderer->texture_table[tex_index];
            vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->vulkan.layout, 1, 1, &tex->vulkan.descriptor, 0, NULL);
        }
        //Draw to the screen
        vkCmdDrawIndexed(buffer, call.num_indices, 1, call.index_offset, 0, 0);
        old_mat = mat_index;
        old_tex = tex_index;
    }
    vkCmdEndRendering(buffer); //End rendering

    if(renderer->frame_state != 2) renderer->frame_state = 2;

    return 1;
}

uint8_t BOBi_vk_end_frame(BOBi_Renderer_Impl *renderer) {
    VkCommandBuffer buffer = renderer->vulkan.command_buffer; //Getting a reference to the command buffer so that don't have to write out full code every time
    //After rendering, transition the swapchain image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR so it can be presented to the screen
    BOBi_vk_transition_image_layout(renderer->vulkan.images[renderer->vulkan.next_swapchain_image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, (VkAccessFlags2){}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, buffer);

    VULKAN_ERROR(vkEndCommandBuffer(buffer), "Failed to end command buffer");

    //Submit render commands to the graphics queue
    VkPipelineStageFlagBits wait_dest_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &renderer->vulkan.present_complete_semaphore, .pWaitDstStageMask = &wait_dest_stage_mask,
        .commandBufferCount = 1, .pCommandBuffers = &buffer,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &renderer->vulkan.render_finished_semaphore[renderer->vulkan.next_swapchain_image_index]
    };
    VULKAN_ERROR(vkQueueSubmit(renderer->vulkan.graphics_queue, 1, &sub_info, renderer->vulkan.draw_fence), "Failed to Submit render data to the queue");

    //Get the present status
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pNext = NULL,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &renderer->vulkan.render_finished_semaphore[renderer->vulkan.next_swapchain_image_index],
        .swapchainCount = 1, .pSwapchains = &renderer->vulkan.swapchain, .pImageIndices = &renderer->vulkan.next_swapchain_image_index
    };
    VkResult res = vkQueuePresentKHR(renderer->vulkan.graphics_queue, &present_info);

    //If its invalid, recreate the swapchain
    if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || renderer->vulkan.framebuffer_resized) {
        renderer->vulkan.framebuffer_resized = 0;
        BOBi_vk_recreate_swapchain(renderer);
        return 1;
    }
    VULKAN_ERROR(res, "Failed to acquire swap chain image");

    return 1;
}

void BOBi_vk_update_rend_dim(BOBi_Renderer_Impl *renderer, void *data, size_t data_sz) {
    //Update the default projection matrix
    BOB_ortho_vk( 0.0f, renderer->screen_width, renderer->screen_height, 0.0f, 0.0f, BOB_MAX_LAYER, &renderer->projection);

    renderer->vulkan.framebuffer_resized = 1;
}

#endif

typedef uint8_t (*BOBi_back_create_tex_func)(BOBi_Renderer_Impl *renderer, uint32_t tex_index, size_t width, size_t height, uint8_t * data, BOB_Format format);
typedef uint8_t (*BOBi_back_create_mat_func)(BOBi_Renderer_Impl *renderer, uint32_t mat_index, BOB_Shader_Data *shader_data, size_t num_shaders);
typedef uint8_t (*BOBi_back_create_pb_func)(BOBi_Renderer_Impl *renderer, uint32_t pb_index);
typedef void (*BOBi_back_destr_obj_func)(BOBi_Renderer_Impl *renderer, uint32_t obj_index);
typedef void (*BOBi_back_destroy_renderer_func)(BOBi_Renderer_Impl *renderer);
typedef uint8_t (*BOBi_back_create_default_mat_func)(BOB_Renderer_Handle renderer);
typedef uint8_t (*BOBi_back_draw_func)(BOBi_Renderer_Impl *renderer);
typedef void (*BOBi_back_copy_data_tex_func)(BOBi_Renderer_Impl *renderer, uint32_t atlas_index, BOB_Format format, BOB_Quad dest_rect, uint8_t *data);
typedef uint8_t (*BOBi_back_bind_mem_func)(BOBi_Renderer_Impl *renderer, uint32_t pb_index, void **mapped_mem_ptr, size_t *mem_sz);
typedef void (*BOBi_back_unbind_mem_func)(BOBi_Renderer_Impl *renderer, uint32_t index);
typedef void (*BOBi_back_update_tex_from_buf_func)(BOBi_Renderer_Impl *renderer, uint32_t pb_index);
typedef void (*BOBi_back_update_renderer_dimensions_func)(BOBi_Renderer_Impl *renderer, void *data, size_t sz);

typedef struct {
    BOBi_back_destr_obj_func destroy_texture;
    BOBi_back_destr_obj_func destroy_pixelbuffer;
    BOBi_back_destr_obj_func destroy_material;
    BOBi_back_create_tex_func create_texture;
    BOBi_back_create_mat_func create_material;
    BOBi_back_create_pb_func create_pixelbuffer;
    BOBi_back_destroy_renderer_func destroy_renderer;
    BOBi_back_draw_func begin_frame;
    BOBi_back_draw_func draw;
    BOBi_back_draw_func end_frame;
    BOBi_back_copy_data_tex_func copy_data_to_tex;
    BOBi_back_bind_mem_func bind_memory;
    BOBi_back_unbind_mem_func unbind_memory;
    BOBi_back_update_tex_from_buf_func update_tex_from_buf;
    BOBi_back_update_renderer_dimensions_func update_rend_dim;
    BOBi_Renderer_Type type;
} BOBi_Backend_Vtable;

BOBi_Backend_Vtable renderer_functions[BOB_NUM_RENDERER_TYPES] = {
    #ifdef BOB_INCLUDE_GLAD
    (BOBi_Backend_Vtable) {
        .destroy_texture = &BOBi_gl_delete_texture,
        .destroy_pixelbuffer = &BOBi_gl_delete_buffer,
        .destroy_material = &BOBi_gl_delete_program,
        .create_texture = &BOBi_gl_create_tex,
        .create_material = &BOBi_gl_create_material,
        .create_pixelbuffer = &BOBi_gl_create_pbo,
        .destroy_renderer = &BOBi_gl_destroy_renderer_mem,
        .begin_frame = &BOBi_gl_begin_frame,
        .draw = &BOBi_gl_draw,
        .end_frame = &BOBi_gl_end_frame,
        .copy_data_to_tex = &BOBi_gl_copy_data_tex,
        .bind_memory = &BOBi_gl_bind_pbo_mem,
        .unbind_memory = &BOBi_gl_unbind_pbo_mem,
        .update_tex_from_buf = &BOBi_gl_upload_pbo_data,
        .update_rend_dim = &BOBi_gl_copy_buffer_data,
        .type = BOB_OPENGL_RENDERER,
    },
    #endif //BOB_INCLUDE_GLAD

    #ifdef BOB_INCLUDE_VULKAN
    (BOBi_Backend_Vtable) {
        .destroy_texture = &BOBi_vk_destroy_texture,
        .destroy_pixelbuffer = &BOBi_vk_destroy_pbo,
        .destroy_material = &BOBi_vk_destroy_material,
        .create_texture = &BOBi_vk_create_texture,
        .create_material = &BOBi_vk_create_material,
        .create_pixelbuffer = &BOBi_vk_create_pbo,
        .begin_frame = &BOBi_vk_begin_frame,
        .draw = &BOBi_vk_draw,
        .end_frame = &BOBi_vk_end_frame,
        .copy_data_to_tex = &BOBi_vk_copy_data_tex,
        .bind_memory = &BOBi_vk_bind_pbo_mem,
        .unbind_memory = &BOBi_vk_unbind_pbo_mem,
        .update_tex_from_buf = &BOBi_vk_upload_pbo_data,
        .update_rend_dim = &BOBi_vk_update_rend_dim,
        .destroy_renderer = &BOBi_vk_destroy_renderer,
    },
    #endif //BOB_INCLUDE_VULKAN
};

//================================================= INTERNAL HELPER FUNCTIONS ===================================================

uint8_t BOBi_create_renderer(BOBi_Renderer_Type type, size_t atlas_capacity, size_t pixelbuf_capacity, size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, size_t width, size_t height, BOB_Renderer_Handle *renderer) {
    if(bob_state.renderer_count >= bob_state.renderer_capcity) {
        printf("ERROR: Exceeded renderer capacity\n");
        return 0;
    }

    uint32_t index;
    if(bob_state.next_renderer_slot == UINT32_MAX) {
        index = bob_state.renderer_count;
    }
    else {
        index = bob_state.next_renderer_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (bob_state.next_renderer_slot = index + 1; bob_state.next_renderer_slot < bob_state.renderer_count; bob_state.next_renderer_slot++) {
            //Use the allocation status of the renderer's memory region as an initialisation tell
            //Relies on setting pointer to NULL on renderer destruction and zeroing memory on creating the BOB instance
            if (bob_state.renderers[bob_state.next_renderer_slot].renderer_memory.memory == NULL)
                break;
        }

        if (bob_state.next_renderer_slot >= bob_state.renderer_count)
            bob_state.next_renderer_slot = UINT32_MAX;
    }

    BOBi_Renderer_Impl *intrn_renderer = &bob_state.renderers[index];

    //Calculating the size of the memory regions each buffer will end up using
    size_t atlas_sz = atlas_capacity * sizeof(BOBi_Atlas_Impl);
    size_t pixelbuf_sz = pixelbuf_capacity * sizeof(BOBi_Pixelbuffer_Impl);
    size_t tex_sz = tex_capacity * sizeof(BOBi_Texture_Impl);
    size_t mat_sz = mat_capacity * sizeof(BOBi_Material_Impl);
    size_t font_sz = font_capacity * sizeof(BOBi_Font_Impl);

    //Figuring out how much aligned memory we will need
    char *p = (char *)0;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOBi_Atlas_Impl));
    p += atlas_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOBi_Pixelbuffer_Impl));
    p += pixelbuf_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOBi_Texture_Impl));
    p += tex_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOBi_Material_Impl));
    p += mat_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOBi_Font_Impl));
    p += font_sz;

    size_t total = (size_t)p;

    //Allocating the memory used for the object buffers and checking the allocation
    if(!BOB_init_arena(&intrn_renderer->renderer_memory, total)) return 0;
    memset(intrn_renderer->renderer_memory.memory, 0, total);

    //Assigning the start pointers from the general memory buffer
    intrn_renderer->atlas_table = BOB_arena_alloc(&intrn_renderer->renderer_memory, atlas_sz, alignof(BOBi_Atlas_Impl));
    intrn_renderer->pixelbuffer_table = BOB_arena_alloc(&intrn_renderer->renderer_memory, pixelbuf_sz, alignof(BOBi_Pixelbuffer_Impl));
    intrn_renderer->texture_table = BOB_arena_alloc(&intrn_renderer->renderer_memory, tex_sz, alignof(BOBi_Texture_Impl));
    intrn_renderer->material_table = BOB_arena_alloc(&intrn_renderer->renderer_memory, mat_sz, alignof(BOBi_Material_Impl));
    intrn_renderer->font_table = BOB_arena_alloc(&intrn_renderer->renderer_memory, font_sz, alignof(BOBi_Font_Impl));

    //Assiging the capacity values
    intrn_renderer->atlas_capacity = atlas_capacity;
    intrn_renderer->pixelbuffer_capacity = pixelbuf_capacity;
    intrn_renderer->texture_capacity = tex_capacity;
    intrn_renderer->material_capacity = mat_capacity;
    intrn_renderer->font_capacity = font_capacity;

    //Setting the sizes to be 0
    intrn_renderer->num_atlases = 0;
    intrn_renderer->num_pixelbuffers = 0;
    intrn_renderer->num_textures = 0;
    intrn_renderer->num_materials = 0;
    intrn_renderer->num_fonts = 0;

    //Setting the next free slot to point to the first one
    intrn_renderer->next_atlas_slot = UINT32_MAX;
    intrn_renderer->next_pixelbuf_slot = UINT32_MAX;
    intrn_renderer->next_tex_slot = UINT32_MAX;
    intrn_renderer->next_mat_slot = UINT32_MAX;
    intrn_renderer->next_font_slot = UINT32_MAX;

    intrn_renderer->type = type;

    size_t vert_buf_sz = vertex_capacity * sizeof(BOBi_Render_Vertex);
    size_t index_buf_sz = index_capacity * sizeof(uint32_t);

    intrn_renderer->screen_height = height;
    intrn_renderer->screen_width = width;

    //Initialise the stack of clip rects
    intrn_renderer->stack = malloc(sizeof(BOBi_Clip_Stack));
    intrn_renderer->stack->elems = malloc(sizeof(BOBi_Clip_Rect) * INIT_STACK_CAPACITY);
    intrn_renderer->stack->capacity = INIT_STACK_CAPACITY;
    intrn_renderer->stack->size = 0;

    BOB_init_arena(&intrn_renderer->batch.vertex_arena, (vert_buf_sz > index_buf_sz) ? vert_buf_sz : index_buf_sz); //Since this dual use need to take the max of the two
    BOB_init_arena(&intrn_renderer->batch.vertex_arena_2, vert_buf_sz);
    BOB_init_arena(&intrn_renderer->batch.draw_call_arena, draw_call_capacity * sizeof(BOBi_Draw_Call));
    *renderer = index;
    return 1;
}

// ============= QUICKSORT IMPLEMENTATION ===============
int8_t BOBi_compare_draw_calls(BOBi_Draw_Call a, BOBi_Draw_Call b, uint8_t strict) {
    if(a.tex < b.tex) return -1;
    if(a.tex > b.tex) return 1;
    if(a.mat < b.mat) return -1;
    if(a.mat > b.mat) return 1;
    if(strict) {
        if(a.submission_id < b.submission_id) return -1;
        if(a.submission_id > b.submission_id) return 1;
    }
    return 0; //Should not be reached since submission_id should act as a tiebreaker
}

void BOBi_swap_draw_calls(BOBi_Draw_Call *a, BOBi_Draw_Call *b) {
    BOBi_Draw_Call temp = *a;
    *a = *b;
    *b = temp;
}

size_t BOBi_quicksort_median_of_three(BOBi_Renderer_Impl *r, size_t lo, size_t hi)
{
    size_t mid = lo + (hi - lo) / 2;

    BOBi_Draw_Call *a = &BOBi_get_arena_elem(r->batch.draw_call_arena, lo, BOBi_Draw_Call);
    BOBi_Draw_Call *b = &BOBi_get_arena_elem(r->batch.draw_call_arena, mid, BOBi_Draw_Call);
    BOBi_Draw_Call *c = &BOBi_get_arena_elem(r->batch.draw_call_arena, hi, BOBi_Draw_Call);

    if (BOBi_compare_draw_calls(*a, *b, 1) > 0)
        BOBi_swap_draw_calls(a, b);

    if (BOBi_compare_draw_calls(*a, *c, 1) > 0)
        BOBi_swap_draw_calls(a, c);

    if (BOBi_compare_draw_calls(*b, *c, 1) > 0)
        BOBi_swap_draw_calls(b, c);

    return mid;
}

//Implementing Hoare's partition. Based on the code found here:
//https://www.geeksforgeeks.org/dsa/hoares-vs-lomuto-partition-scheme-quicksort/
size_t BOBi_quicksort_partition(BOBi_Renderer_Impl *r, size_t subarr_start, size_t subarr_end) {
    size_t pivot = BOBi_quicksort_median_of_three(r, subarr_start, subarr_end);

    BOBi_Draw_Call pivot_call = BOBi_get_arena_elem(r->batch.draw_call_arena, pivot, BOBi_Draw_Call);
    size_t i = subarr_start, j = subarr_end;
    while(1) {
        //Find leftmost element >= pivot
        while(BOBi_compare_draw_calls(BOBi_get_arena_elem(r->batch.draw_call_arena, i, BOBi_Draw_Call), pivot_call, 1) < 0)
            i++;

        //Find rightmost element <= pivot;
        while(BOBi_compare_draw_calls(BOBi_get_arena_elem(r->batch.draw_call_arena, j, BOBi_Draw_Call), pivot_call, 1) > 0)
            j--;

        if(i >= j) return j;

        BOBi_swap_draw_calls(&BOBi_get_arena_elem(r->batch.draw_call_arena, i, BOBi_Draw_Call), &BOBi_get_arena_elem(r->batch.draw_call_arena, j, BOBi_Draw_Call));

        i++;
        j--;
    }

    return i;
}

void BOBi_quicksort_draw_calls(BOBi_Renderer_Impl *r, size_t subarr_start, size_t subarr_end) {
    if(subarr_start < subarr_end) {
        size_t pos_pivot = BOBi_quicksort_partition(r, subarr_start, subarr_end);
        BOBi_quicksort_draw_calls(r, subarr_start, pos_pivot);
        BOBi_quicksort_draw_calls(r, pos_pivot+1, subarr_end);
    }
}

//Helper function to clip a quad. Only works if the quad is unrotated. Otherwise use BOBi_clip_polygon
uint8_t BOBi_clip_quad(BOBi_Renderer_Impl *r, BOB_Quad *quad) {
    if(r->stack->size == 0) return 1; //No need to clip if no clip rects
    BOBi_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
    if(clip.empty) return 0;

    if(clip.clip_horz) {
        if(quad->x > clip.right) return 0; //Outside the clip region
        if(quad->x < clip.left) {
            quad->w -= clip.left - quad->x;
            quad->x = clip.left;
        }
        if(quad->x + quad->w > clip.right) quad->w = clip.right - quad->x;
    }
    if(clip.clip_vert) {
        if(quad->y > clip.bottom) return 0; //Outside the clip region
        if(quad->y < clip.top) {
            quad->h -= clip.top - quad->y;
            quad->y = clip.top;
        }
        if(quad->y + quad->h > clip.bottom) quad->h = clip.bottom - quad->y;
    }

    if((quad->h <= 0) || (quad->w <= 0)) return 0; //If the clipped region is empty, return;

    return 1;
}

//Helper functions to clip a line by implementing the Cohen-Sutherland algorithm
//https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
uint8_t BOBi_line_outcode(BOB_Vector2* point, BOBi_Clip_Rect clip) {
    uint8_t code = 0;

    if(point->x < clip.left) code |= 1; //Left
    else if(point->x > clip.right) code |= 2; //Right

    if(point->y < clip.top) code |= 4; //Top
    else if(point->y > clip.bottom) code |= 8; //Bottom

    return code;
}

uint8_t BOBi_clip_line(BOBi_Renderer_Impl *r, BOB_Vector2 *start, BOB_Vector2* end) {
    if(r->stack->size == 0) return 1; //No need to clip if no clip rects
    BOBi_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
    if(clip.empty) return 0;

    uint8_t code_s = BOBi_line_outcode(start, clip);
    uint8_t code_e = BOBi_line_outcode(end, clip);

    while(1) {
        //Both points outside
        if(!(code_s | code_e)) return 1;

        //Both points share an outside region (both points on the same axis outside the clip region)
        if(code_e & code_s) return 0;

        //Pick the point outside
        uint8_t code_out = code_s ? code_s : code_e;

        float x, y;
        //Now find the intersection point;
        //use formulas:
        //  slope = (y1 - y0) / (x1 - x0)
        //  x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
        //  y = y0 + slope * (xm - x0), where xm is xmin or xmax
        //No need to worry about divide-by-zero because, in each case, the
        //outcode bit being tested guarantees the denominator is non-zero
        if(code_out & 4) { //Top
            x = start->x + (end->x - start->x) * (clip.top - start->y) / (end->y - start->y);
            y = clip.top;
        }
        else if(code_out & 8) { //Bottom
            x = start->x + (end->x - start->x) * (clip.bottom - start->y) / (end->y - start->y);
            y = clip.bottom;
        }
        else if (code_out & 2) { //Right
            y = start->y + (end->y - start->y) * (clip.right - start->x) / (end->x - start->x);
            x = clip.right;
        }
        else if (code_out & 1) { //Left
            y = start->y + (end->y - start->y) * (clip.left - start->x) / (end->x - start->x);
            x = clip.left;
        }

        // Now we move outside point to intersection point to clip
        // and get ready for next pass.
        if (code_out == code_s) {
            start->x = x;
            start->y = y;
            code_s = BOBi_line_outcode(start, clip);
        } else {
            end->x = x;
            end->y = y;
            code_e = BOBi_line_outcode(end, clip);
        }
    }
}

typedef enum {
    BOBi_CLIP_LEFT,
    BOBi_CLIP_RIGHT,
    BOBi_CLIP_BOTTOM,
    BOBi_CLIP_TOP,
} BOBi_Clip_Edge;

BOB_Vector2 BOBi_get_intersection(BOB_Vector2 a, BOB_Vector2 b, BOBi_Clip_Edge edge, float value) {
    switch(edge) {
        case BOBi_CLIP_LEFT:
        case BOBi_CLIP_RIGHT: {
            float t = (value - a.x) / (b.x - a.x);
            return (BOB_Vector2){value, a.y + t * (b.y - a.y)};
        }
        case BOBi_CLIP_TOP:
        case BOBi_CLIP_BOTTOM: {
            float t = (value - a.y) / (b.y - a.y);
            return (BOB_Vector2){a.x + t * (b.x - a.x), value};
        }
    }
}

static inline uint8_t BOBi_inside(BOB_Vector2 p, BOBi_Clip_Edge edge, float value) {
    switch(edge) {
        case BOBi_CLIP_LEFT: return p.x >= value;
        case BOBi_CLIP_RIGHT: return p.x <= value;
        case BOBi_CLIP_TOP: return p.y >= value;
        case BOBi_CLIP_BOTTOM: return p.y <= value;
    }
}

size_t BOBi_clip_edge(BOB_Vector2 *poly_points, size_t poly_size, BOBi_Clip_Edge edge, float value) {
    size_t new_poly_size = 0;
    BOB_Vector2 new_points[BOB_MAX_POLY_SIZE]; //Allow up to 256 vertex polygons

    //Iterate over all points
    for(size_t i = 0; i < poly_size; i++) {
        //Getting the point that forms the end of the current line
        size_t j = (i + 1) % poly_size;
        BOB_Vector2 start = poly_points[i];
        BOB_Vector2 end = poly_points[j];

        uint8_t start_inside = BOBi_inside(start, edge, value);
        uint8_t end_inside = BOBi_inside(end, edge, value);

        if(start_inside && end_inside) {
            if(new_poly_size >= BOB_MAX_POLY_SIZE) {
                printf("Exceeded new polygon point capacity\n");
                break;
            }
            //Only second point is added
            new_points[new_poly_size++] = end;
        }
        else if(!start_inside && end_inside) {
            if(new_poly_size+1 >= BOB_MAX_POLY_SIZE) {
                printf("Exceeded new polygon point capacity\n");
                break;
            }
            //Point of intersection with edge and second point is added
            new_points[new_poly_size++] = BOBi_get_intersection(start, end, edge, value);
            new_points[new_poly_size++] = end;
        }
        //When only second point is outside
        else if(start_inside && !end_inside) {
            if(new_poly_size >= BOB_MAX_POLY_SIZE) {
                printf("Exceeded new polygon point capacity\n");
                break;
            }
            //Only point of intersection with edge is added
            new_points[new_poly_size++] = BOBi_get_intersection(start, end, edge, value);
        }
        //When both points are outside, no points are added
    }
    memcpy(poly_points, new_points, new_poly_size * sizeof(BOB_Vector2));
    return new_poly_size;
}

size_t BOBi_clip_polygon(BOBi_Renderer_Impl *r, BOB_Vector2 *poly_points, size_t poly_size) {
    if(r->stack->size == 0) return poly_size; //No need to clip if no clip rects
    BOBi_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
    if(clip.empty) return 0;

    BOB_Vector2 clip_vertices[4] = {(BOB_Vector2){clip.left, clip.top}, (BOB_Vector2){clip.left, clip.bottom}, (BOB_Vector2){clip.right, clip.bottom}, (BOB_Vector2){clip.right, clip.top}};

    poly_size = BOBi_clip_edge(poly_points, poly_size, BOBi_CLIP_LEFT, clip.left);
    poly_size = BOBi_clip_edge(poly_points, poly_size, BOBi_CLIP_RIGHT, clip.right);
    poly_size = BOBi_clip_edge(poly_points, poly_size, BOBi_CLIP_TOP, clip.top);
    poly_size = BOBi_clip_edge(poly_points, poly_size, BOBi_CLIP_BOTTOM, clip.bottom);

    return poly_size;
}

typedef struct BOBi_Partition_Vertex {
    uint32_t index;
    BOB_Vector2 pos;
    struct BOBi_Partition_Vertex *prev, *next;
} BOBi_PartitionVertex;

float BOBi_cross_prod(BOB_Vector2 a, BOB_Vector2 b, BOB_Vector2 c) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float bcx = c.x - b.x;
    float bcy = c.y - b.y;

    return abx * bcy - aby * bcx;
}

uint8_t BOBi_point_inside_triangle(BOB_Vector2 point, BOB_Vector2 a, BOB_Vector2 b, BOB_Vector2 c) {
    float d1 = (point.x - b.x) * (a.y - b.y) - (a.x - b.x) * (point.y - b.y);
    float d2 = (point.x - c.x) * (b.y - c.y) - (b.x - c.x) * (point.y - c.y);
    float d3 = (point.x - a.x) * (c.y - a.y) - (c.x - a.x) * (point.y - a.y);

    uint8_t has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    uint8_t has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

uint8_t BOBi_is_ear(BOBi_PartitionVertex *v, BOBi_PartitionVertex *start, BOB_Vector2 *points) {
    BOBi_PartitionVertex *a = v->prev;
    BOBi_PartitionVertex *b = v;
    BOBi_PartitionVertex *c = v->next;

    if(BOBi_cross_prod(points[a->index], points[b->index], points[c->index]) <= 0.0f) return 0;

    BOBi_PartitionVertex *p = start;

    do {
        if(p != a && p != b && p != c) {
            if(BOBi_point_inside_triangle(points[p->index], points[a->index], points[b->index], points[c->index])) return 0;
        }
        p = p->next;
    } while(p != start);

    return 1;
}

size_t BOBi_triangulate_ec(BOB_Vector2 *poly_points, size_t poly_size, uint32_t *indices) {
    if(poly_size < 3) return 0;
    if(poly_size == 3) {
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        return 1;
    }

    BOBi_PartitionVertex vertices[BOB_MAX_POLY_SIZE];
    uint32_t processed[BOB_MAX_POLY_SIZE];
    size_t processed_size = 0;
    #define EPSILON 1e-6f

    //Preprocessing to remove duplicate vertices:
    for(int i = 0; i < poly_size; i++) {
        int prev = (i-1+poly_size) % poly_size;
        int next = (i+1) % poly_size;

        float dx = poly_points[i].x - poly_points[prev].x;
        float dy = poly_points[i].y - poly_points[prev].y;

        //Adding non-duplicate points
        if (dx*dx + dy*dy >= EPSILON*EPSILON) {
            processed[processed_size] = i;
            processed_size++;
            continue;
        }
    }

    size_t write = 0;

    //Preprocessing to remove collinear vertices:
    for(int read = 0; read < processed_size; read++) {
        int prev = (read-1+processed_size) % processed_size;
        int next = (read+1) % processed_size;

        uint32_t ia = processed[prev];
        uint32_t ib = processed[read];
        uint32_t ic = processed[next];

        //Removing colinear vertices:
        BOB_Vector2 ab = {poly_points[ib].x - poly_points[ia].x, poly_points[ib].y - poly_points[ia].y};
        BOB_Vector2 bc = {poly_points[ic].x - poly_points[ib].x, poly_points[ic].y - poly_points[ib].y};

        if((ab.x * bc.x + ab.y * bc.y) < 0.0f || fabsf(BOBi_cross_prod(poly_points[ia], poly_points[ib], poly_points[ic])) > EPSILON) {
            processed[write++] = processed[read];
        }
    }

    //Converting normal vertices into doubly-linked list
    for(int i = 0; i < processed_size; i++) {
        int prev = (i-1+processed_size) % processed_size;
        int next = (i+1) % processed_size;

        vertices[i].index = processed[i];
        vertices[i].pos = poly_points[processed[i]];
        vertices[i].prev = &vertices[prev];
        vertices[i].next = &vertices[next];
    }

    size_t vertex_count = processed_size;
    size_t triangle_count = 0;

    BOBi_PartitionVertex *start = &vertices[0];
    while(vertex_count > 3) {
        BOBi_PartitionVertex *v = start;

        uint8_t found = 0;

        do {
            if(BOBi_is_ear(v, start, poly_points)) {
                BOBi_PartitionVertex *prev = v->prev;
                BOBi_PartitionVertex *next = v->next;

                indices[(triangle_count * 3)] = prev->index;
                indices[(triangle_count * 3)+1] = v->index;
                indices[(triangle_count * 3)+2] = next->index;
                triangle_count++;

                prev->next = next;
                next->prev = prev;

                if(v == start) start = next;

                vertex_count--;
                found = 1;
                break;
            }

            v = v->next;
        } while(v != start);

        if(!found) return 0;
    }

    indices[(triangle_count * 3)] = start->index;
    indices[(triangle_count * 3)+1] = start->next->index;
    indices[(triangle_count * 3)+2] = start->next->next->index;
    triangle_count++;

    return triangle_count;
}

void BOBi_renderer_reset(BOBi_Renderer_Impl *r) {
    BOB_arena_clear(&r->batch.vertex_arena);
    BOB_arena_clear(&r->batch.vertex_arena_2);
    BOB_arena_clear(&r->batch.draw_call_arena);

    r->batch.num_draw_calls = 0;
    r->batch.num_indices = 0;
    r->batch.num_vertices = 0;
}

void BOBi_renderer_draw(BOBi_Renderer_Impl *r) {
    if(r->batch.num_draw_calls == 0 || r->batch.num_indices == 0 || r->batch.num_vertices == 0) return;
    //Sort the draw calls
    BOBi_quicksort_draw_calls(r, 0, r->batch.num_draw_calls-1);

    //Copy the vertices to be in sorted order
    for(size_t i = 0; i < r->batch.num_draw_calls; i++) {
        BOBi_Draw_Call *call = (BOBi_Draw_Call *)r->batch.draw_call_arena.memory + i;
        BOBi_Render_Vertex *new_pos = BOB_arena_alloc(&r->batch.vertex_arena_2, call->num_vertices * sizeof(BOBi_Render_Vertex), alignof(BOBi_Render_Vertex));
        memcpy(new_pos, call->vertices, call->num_vertices * sizeof(BOBi_Render_Vertex));
        call->vertices = new_pos;
    }

    //Clear the orginial vertex arena so that we can re-use it for indicies
    BOB_arena_clear(&r->batch.vertex_arena);

    //Generate the indices for each draw call
    size_t cur_vertex = 0;
    size_t index_count = 0;
    uint32_t *indices;
    for(size_t i = 0; i < r->batch.num_draw_calls; i++) {
        BOBi_Draw_Call *call = (BOBi_Draw_Call *)r->batch.draw_call_arena.memory + i;
        call->index_offset = index_count;
        indices = BOB_arena_alloc(&r->batch.vertex_arena, sizeof(uint32_t) * call->num_indices, alignof(uint32_t));
        switch(call->type) {
            case BOBi_DRAW_CIRCLE: {
                size_t circ_index = 0;
                for(int i = 1; i < call->num_vertices; i++) {
                    indices[circ_index++] = cur_vertex;
                    indices[circ_index++] = cur_vertex + i;
                    indices[circ_index++] = cur_vertex + ((i+1) % call->num_vertices);
                }
            }
            break;
            case BOBi_DRAW_QUAD:
                indices[0] = cur_vertex;
                indices[1] = cur_vertex+1;
                indices[2] = cur_vertex+3;
                indices[3] = cur_vertex+1;
                indices[4] = cur_vertex+2;
                indices[5] = cur_vertex+3;
            break;
            case BOBi_DRAW_POLY: {
                uint32_t triangle_indices[(BOB_MAX_POLY_SIZE - 2) * 3]; //Ear clipping always produces n-2 triangles for a polygon with n vertices
                BOB_Vector2 base_vertices[BOB_MAX_POLY_SIZE];
                for(size_t j = 0; j < call->num_vertices; j++) {
                    base_vertices[j] = (BOB_Vector2){call->vertices[j].pos.x, call->vertices[j].pos.y};
                }

                size_t triangle_count = BOBi_triangulate_ec(base_vertices, call->num_vertices, triangle_indices);

                if(!triangle_count) return; //Early exit

                //Processing the returned vertex data into a more compact form so we can pass it to the renderer
                uint32_t vertex_map[BOB_MAX_POLY_SIZE];

                //Filling the map with dummy values
                for(size_t i = 0; i < call->num_vertices; i++)
                    vertex_map[i] = UINT32_MAX;

                BOBi_Render_Vertex compressed[BOB_MAX_POLY_SIZE]; //Holds the compressed vertex values
                size_t vertex_count = 0;

                //Copying the old verticies into compressed format
                for(size_t j = 0; j < triangle_count*3; j++) {
                    uint32_t old = triangle_indices[j];
                    if(vertex_map[old] == UINT32_MAX) {
                        vertex_map[old] = vertex_count;
                        compressed[vertex_count++] = call->vertices[old];
                    }
                    indices[j] = cur_vertex + vertex_map[old];
                }

                memcpy(call->vertices, compressed, vertex_count * sizeof(BOBi_Render_Vertex));
            }
            break;
        }

        index_count += call->num_indices;
        cur_vertex += call->num_vertices;
    }

    //Compress the draw calls:
    size_t num_unique_calls = 1;
    size_t i = 0;
    BOBi_Draw_Call *curr = (BOBi_Draw_Call *)r->batch.draw_call_arena.memory + i;
    while(i < r->batch.num_draw_calls-1) {
        BOBi_Draw_Call next = BOBi_get_arena_elem(r->batch.draw_call_arena, i+1, BOBi_Draw_Call);
        if(BOBi_compare_draw_calls(*curr, next, 0) != 0) {
            BOBi_get_arena_elem(r->batch.draw_call_arena, num_unique_calls, BOBi_Draw_Call) = next;
            curr = (BOBi_Draw_Call *)r->batch.draw_call_arena.memory + num_unique_calls;
            num_unique_calls++;
        }
        else {
            curr->num_indices += next.num_indices;
            curr->num_vertices += next.num_vertices;
        }
        i++;
    }

    r->batch.num_draw_calls = num_unique_calls;

    renderer_functions[r->type].draw(r);
}

void BOBi_flush_draw_calls(BOBi_Renderer_Impl *r) {
    BOBi_renderer_draw(r);
    BOBi_renderer_reset(r);
}

void BOBi_check_draw_capacity(BOB_Renderer_Handle renderer, uint32_t num_vertices, uint32_t num_indices) {
    BOBi_Renderer_Impl *r;
    BOBi_get_renderer(renderer, &r);
    if(num_indices + r->batch.num_indices >= BOB_MAX_INDEX_CAPACITY ||
       num_vertices + r->batch.num_vertices >= BOB_MAX_VERTEX_CAPACITY ||
       r->batch.num_draw_calls + 1 >= BOB_MAX_DRAW_CALL_CAPACITY) {
        BOBi_flush_draw_calls(r);
        r->batch.num_draw_calls = 0;
        r->batch.num_indices = 0;
        r->batch.num_vertices = 0;
    }
}

void BOBi_create_draw_call(BOB_Renderer_Handle renderer, BOB_Vector3 *vertices, size_t vertex_count, BOB_Vector2 *uv, size_t index_count, BOB_Vector4 colour, BOB_Texture_Handle tex, BOB_Material_Handle mat, uint8_t channel, BOBi_Draw_Type type) {
    BOBi_check_draw_capacity(renderer, vertex_count, index_count);

    BOBi_Renderer_Impl *r;
    BOBi_get_renderer(renderer, &r);
    BOBi_Draw_Call *dc = (BOBi_Draw_Call *)BOB_arena_alloc(&r->batch.draw_call_arena, sizeof(BOBi_Draw_Call), alignof(BOBi_Draw_Call));
    BOBi_Render_Vertex *alloc_vertices = (BOBi_Render_Vertex *)BOB_arena_alloc(&r->batch.vertex_arena, sizeof(BOBi_Render_Vertex) * vertex_count, alignof(BOBi_Render_Vertex));
    dc->num_indices = index_count;
    dc->num_vertices = vertex_count;
    dc->vertices = alloc_vertices;
    dc->type = type;
    dc->mat = mat;
    dc->tex = tex;
    dc->submission_id = r->batch.num_draw_calls;

    r->batch.num_draw_calls++;
    r->batch.num_indices += index_count;
    r->batch.num_vertices += vertex_count;

    for(size_t i = 0; i < vertex_count; i++) {
        dc->vertices[i] = (BOBi_Render_Vertex){colour, vertices[i], (uv == NULL) ? (BOB_Vector2){0} : uv[i], channel};
    }
}

BOB_Vector2 BOBi_rotate_about_point(BOB_Vector2 point, BOB_Vector2 rot_center, float rotation) {
    float c = cos(rotation);
    float s = sin(rotation);

    return (BOB_Vector2){
        rot_center.x + (point.x - rot_center.x) * c - (point.y - rot_center.y) * s,
        rot_center.y + (point.x - rot_center.x) * s + (point.y - rot_center.y) * c};
}

void BOBi_rotate_quad(BOB_Quad quad, BOB_Vector2 out[4], float rotation) {
    BOB_Vector2 center = (BOB_Vector2){quad.x + (quad.w/2.0f), quad.y + (quad.h/2.0f)};

    out[0] = BOBi_rotate_about_point((BOB_Vector2){quad.x, quad.y}, center, rotation);
    out[1] = BOBi_rotate_about_point((BOB_Vector2){quad.x, quad.y + quad.h}, center, rotation);
    out[2] = BOBi_rotate_about_point((BOB_Vector2){quad.x + quad.w, quad.y + quad.h}, center, rotation);
    out[3] = BOBi_rotate_about_point((BOB_Vector2){quad.x + quad.w, quad.y}, center, rotation);
}

void BOBi_rotate_polygon(BOB_Vector2 *poly_points, size_t poly_size, float rotation) {
    //Computing weighted centroid:
    float area = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;

    for (size_t i = 0; i < poly_size; i++)
    {
        size_t j = (i + 1) % poly_size;

        float cross = poly_points[i].x * poly_points[j].y -
                      poly_points[j].x * poly_points[i].y;

        area += cross;

        cx += (poly_points[i].x + poly_points[j].x) * cross;
        cy += (poly_points[i].y + poly_points[j].y) * cross;
    }

    area *= 0.5f;

    BOB_Vector2 center;
    center.x = cx / (6.0f * area);
    center.y = cy / (6.0f * area);

    //Rotating points in place
    for(size_t i = 0; i < poly_size; i++) {
        poly_points[i] = BOBi_rotate_about_point(poly_points[i], center, rotation);
    }
}

void BOBi_texture_free(BOBi_Renderer_Impl *renderer, uint32_t index) {
    renderer_functions[renderer->type].destroy_texture(renderer, index);
    renderer->texture_table[index] = (BOBi_Texture_Impl){0}; //Clear the data
}
void BOBi_pixelbuffer_free(BOBi_Renderer_Impl *renderer, uint32_t index) {
    renderer_functions[renderer->type].destroy_pixelbuffer(renderer, index);
    BOB_texture_free(&renderer->pixelbuffer_table[index].pixel_tex);
    renderer->pixelbuffer_table[index] = (BOBi_Pixelbuffer_Impl){0}; //Clear the data
}
void BOBi_material_free(BOBi_Renderer_Impl *renderer, uint32_t index) {
    renderer_functions[renderer->type].destroy_material(renderer, index);
    free(renderer->material_table[index].uniforms);
    renderer->material_table[index] = (BOBi_Material_Impl){0}; //Clear the data
}

#define BOBi_HASHMAP_DUMMY UINT64_MAX

uint32_t BOBi_hashmap_add(BOBi_Hashmap *h, uint64_t key, uint32_t value);

//Checks if n is prime
uint64_t BOBi_is_prime(uint64_t n) {
    if(n <= 1) return 0;
    if(n <= 3) return 1;
    if(0 == n % 2 || 0 == n % 3) return 0;

    for(size_t i = 5; i * i <= n; i +=6) {
        if(n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

//Gets the next prime number after n
uint64_t BOBi_next_prime(uint64_t n) {
    if(n <= 2) return 2;
    n = (0 == n % 2) ? n +1 : n; //Make sure n is odd

    while(!BOBi_is_prime(n)){
        n += 2; //Skip even numbers
    }
    return n;

}

uint8_t BOBi_hashmap_init(size_t init_capacity, BOBi_Hashmap *out) {
    out->capacity = BOBi_next_prime(init_capacity);
    out->keys = malloc(sizeof(uint64_t) * out->capacity);
    if(out->keys == NULL) return 0;
    memset(out->keys, 0xFF, sizeof(uint64_t) * out->capacity);
    out->values = malloc(sizeof(uint32_t) * out->capacity);
    if(out->values == NULL) return 0;
    memset(out->values, 0xFF, sizeof(uint32_t) * out->capacity);

    return 1;
}

//Primary hash funtion.
uint64_t BOBi_hash_int(uint64_t key, size_t length) {
    return (key & 0x7FFFFFFFFFFFFFFF) % length;
}

//Secondary hash funtion.
uint64_t BOBi_second_hash_int(uint64_t key, size_t length) {
    return 1 + (key & 0x7FFFFFFFFFFFFFFF) % (length - 1);
}

//Finds the next slot that we can put a value into in the hashmap
uint64_t BOBi_hashmap_find(const BOBi_Hashmap *h, uint64_t key) {
    uint64_t hash = BOBi_hash_int(key, h->capacity);
    uint64_t step = BOBi_second_hash_int(key, h->capacity);

    uint64_t j = hash;

    for (size_t i = 0; i < h->capacity; ++i) {
        if (h->keys[j] == BOBi_HASHMAP_DUMMY) //At an empty slot
            return UINT64_MAX;

        if (h->keys[j] == key)
            return j;

        j = (j + step) % h->capacity;
    }

    return UINT64_MAX;
}

uint64_t BOBi_hashmap_find_insert(const BOBi_Hashmap *h, uint64_t key) {
    uint64_t hash = BOBi_hash_int(key, h->capacity);
    uint64_t step = BOBi_second_hash_int(key, h->capacity);

    uint64_t j = hash;
    uint64_t first_deleted = UINT64_MAX;

    for (size_t i = 0; i < h->capacity; ++i) {
        if (h->keys[j] == BOBi_HASHMAP_DUMMY)
            return j;

        if (h->keys[j] == key)
            return j;
        j = (j + step) % h->capacity;
    }

    return first_deleted;
}

//Resizes the hashmap to a new size
void BOBi_hashmap_resize(BOBi_Hashmap *h, size_t newCap) {
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
        if(oldKeys[i] != BOBi_HASHMAP_DUMMY) {
            BOBi_hashmap_add(h, oldKeys[i], oldVals[i]);
        }
    }
    free(oldKeys);
    free(oldVals);
}

//Gets a value from a int_hashmap
uint32_t BOBi_hashmap_get(BOBi_Hashmap *h, uint64_t key) {
    uint64_t j = BOBi_hashmap_find(h, key);
    if(j == BOBi_HASHMAP_DUMMY) return UINT32_MAX;
    return h->values[j];
}

//Removes a kvp from the int_hashmap and returns its value
uint32_t BOBi_hashmap_remove(BOBi_Hashmap *h, uint64_t key) {
    uint64_t j = BOBi_hashmap_find(h, key);
    if(j == BOBi_HASHMAP_DUMMY) return UINT32_MAX;

    uint32_t val = h->values[j];
    h->keys[j] = BOBi_HASHMAP_DUMMY;
    h->values[j] = UINT32_MAX;
    h->size--;
    return val;
}

//Adds a kvp to the int_hashmap, replacing the value if the key already exists in the hashmap
uint32_t BOBi_hashmap_add(BOBi_Hashmap *h, uint64_t key, uint32_t value) {
    uint64_t j = BOBi_hashmap_find_insert(h, key);
    if(j == BOBi_HASHMAP_DUMMY) return UINT32_MAX;
    if (h->keys[j] == key) {
        uint32_t old = h->values[j];
        h->values[j] = value;
        return old;
    }

    h->keys[j] = key;
    h->values[j] = value;
    h->size++;

    if (h->size * 4 >= h->capacity * 3)
        BOBi_hashmap_resize(h, BOBi_next_prime(h->capacity * 2));
    return UINT32_MAX;
}

void BOBi_hashmap_free(BOBi_Hashmap *h) {
    if(h->keys) free(h->keys);
    h->keys = NULL;
    if(h->values) free(h->values);
    h->values = NULL;
}

void BOBi_font_free(BOBi_Renderer_Impl *renderer, uint32_t index) {
    if(renderer->font_table[index].glyphs) free(renderer->font_table[index].glyphs);
    if(renderer->font_table[index].kernings) free(renderer->font_table[index].kernings);
    if(renderer->font_table[index].glyph_map) {
        BOBi_hashmap_free(renderer->font_table[index].glyph_map);
        free(renderer->font_table[index].glyph_map);
    }
    if(renderer->font_table[index].kerning_map) {
        BOBi_hashmap_free(renderer->font_table[index].kerning_map);
        free(renderer->font_table[index].kerning_map);
    }
    renderer->font_table[index] = (BOBi_Font_Impl){0}; //Clear the data
}

void BOBi_destroy_renderer(uint32_t index) {
    BOBi_Renderer_Impl *renderer = &bob_state.renderers[index];

    //Free all of the object memory
    for(size_t i = 0; i < renderer->texture_capacity; i++) {
        if(renderer->texture_table[i].init)
            BOBi_texture_free(renderer, i);
    }
    for(size_t i = 0; i < renderer->material_capacity; i++) {
        if(renderer->material_table[i].init)
            BOBi_material_free(renderer, i);
    }
    for(size_t i = 0; i < renderer->pixelbuffer_capacity; i++) {
        if(renderer->pixelbuffer_table[i].init)
            BOBi_pixelbuffer_free(renderer, i);
    }
    for(size_t i = 0; i < renderer->font_capacity; i++) {
        if(renderer->font_table[i].init)
            BOBi_font_free(renderer, i);
    }
    BOB_destroy_arena(&renderer->renderer_memory);

    free(renderer->stack->elems);
    renderer->stack->elems = NULL;
    free(renderer->stack);
    renderer->stack = NULL;

    BOB_destroy_arena(&renderer->batch.vertex_arena);
    BOB_destroy_arena(&renderer->batch.vertex_arena_2);
    BOB_destroy_arena(&renderer->batch.draw_call_arena);

    renderer_functions[renderer->type].destroy_renderer(renderer);
    *renderer = (BOBi_Renderer_Impl){0}; //Clear all of the data
}

void BOB_destroy_renderer(BOB_Renderer_Handle *renderer) {
    if(*(renderer) & BOBi_MSB) return; //DO not work with already invalid handles
    BOBi_destroy_renderer(*renderer);
    if(*(renderer) < bob_state.next_renderer_slot) bob_state.next_renderer_slot = *(renderer);
    *(renderer) |= *(renderer) & (~BOBi_MSB); //Set the MSB to indicate this is an invalid handle
}

// ==================================== MISCELLANEOUS FUNCTIONS ========================================

//Calculates the projection matrix
void BOB_ortho_gl(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            dest->m[i][j] = 0;
        }
    }

    float rl = 1.0 / (right - left);
    float tb = 1.0 / (top - bottom);
    float mfn =-1.0 / (farZ - nearZ);

    dest->m[0][0] = 2.0 * rl;
    dest->m[1][1] = 2.0 * tb;
    dest->m[2][2] = 2.0 * mfn;
    dest->m[3][0] =-(right + left) * rl;
    dest->m[3][1] =-(top + bottom) * tb;
    dest->m[3][2] = (farZ + nearZ) * mfn;
    dest->m[3][3] = 1.0;
}
void BOB_ortho_vk(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            dest->m[i][j] = 0.0f;
        }
    }

    float rl = 1.0f / (right - left);
    float tb = 1.0f / (top - bottom);
    float fn = 1.0f / (farZ - nearZ);

    dest->m[0][0] = 2.0f * rl;
    dest->m[1][1] = 2.0f * tb;

    // Reversed Z:
    // nearZ -> 1
    // farZ  -> 0
    dest->m[2][2] = -fn;
    dest->m[3][2] = farZ * fn;

    dest->m[3][0] = -(right + left) * rl;
    dest->m[3][1] = -(top + bottom) * tb;
    dest->m[3][3] = 1.0f;
}

//Converts an angle in degrees to radians
float BOB_degrees_to_radians(float angle) {
    return angle * (M_PI / 180);
}

// ============================================= BOB STATE MANAGEMENT ============================================================

#ifdef BOB_INCLUDE_GLAD
#ifdef BOB_INCLUDE_VULKAN
uint8_t BOB_init(GLADloadproc proc, const char **required_extensions, size_t num_extensions, size_t num_renderers) {
    //Loading GLAD
    if(!gladLoadGLLoader(proc)) {
        printf("Failed to initialise GLAD");
        return 0;
    }

    //Loading Vulkan:
    if(!BOBi_vk_init_vulkan(required_extensions, num_extensions)) return 0;
#else
uint8_t BOB_init(GLADloadproc proc, size_t num_renderers) {
    //Loading GLAD
    if(!gladLoadGLLoader(proc)) {
        printf("Failed to initialise GLAD");
        return 0;
    }
#endif //BOB_INCLUDE_VULKAN
#else
uint8_t BOB_init(const char **required_extensions, size_t num_extensions, size_t num_renderers) {
    //Loading Vulkan:
    if(!BOBi_vk_init_vulkan(required_extensions, num_extensions)) return 0;
#endif //BOB_INCLUDE_GLAD
    bob_state.renderers = malloc(sizeof(BOBi_Renderer_Impl) * num_renderers);
    memset(bob_state.renderers, 0, sizeof(BOBi_Renderer_Impl) * num_renderers);
    bob_state.renderer_count = 0;
    bob_state.next_renderer_slot = UINT32_MAX;
    bob_state.renderer_capcity = num_renderers;

    return 1;
}

void BOB_terminate() {
    for(size_t i = 0; i < bob_state.renderer_capcity; i++) {
        if(bob_state.renderers[i].renderer_memory.memory != NULL) {
            BOBi_destroy_renderer(i);
        }
    }

    free(bob_state.renderers);
    bob_state = (BOBi_Internal_State){0};
}

//========================================================== RENDERER FUNCTIONS ===========================================

//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer_Handle renderer, float colour[4]) {
    BOBi_Renderer_Impl *r;
    if(!BOBi_get_renderer(renderer, &r)) return;
    BOBi_renderer_reset(r);
    r->colour = colour;
    renderer_functions[r->type].begin_frame(r);
}

//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer_Handle renderer) {
    //TODO: THIS SHOULD THROW A MASSIVE ERROR
    BOBi_Renderer_Impl *r;
    if(!BOBi_get_renderer(renderer, &r)) return;

    BOBi_renderer_draw(r);
    renderer_functions[r->type].end_frame(r);
}

//Updates the dimensions of the screen that the renderer renders to.
//Updates projection matrix
//NOTE: Not 100% sure that this works
void BOB_renderer_update_dimensions(BOB_Renderer_Handle renderer, uint32_t width, uint32_t height, uint32_t width_px, uint32_t height_px) {
    BOBi_Renderer_Impl *r;
    if(!BOBi_get_renderer(renderer, &r)) return;

    r->screen_width = width;
    r->screen_height = height;
    r->screen_width_px = width_px;
    r->screen_height_px = height_px;

    //Update the uv coordinates of the texture the renderer is rendering to
    float quadVertices[] = {
        0.0f, 0.0f,          0.0f, 0.0f,
        0.0f, height,        0.0f, 1.0f,
        width, height,       1.0f, 1.0f,
        width, 0.0f,         1.0f, 0.0f
    };

    renderer_functions->update_rend_dim(r, quadVertices, sizeof(quadVertices));
}

//================================================== TEXTURE FUNCTIONS ================================================

//Creates a new texture on the gpu
uint8_t BOB_create_texture(BOB_Renderer_Handle renderer, uint32_t width, uint32_t height, uint8_t *data, BOB_Format format, BOB_Texture_Handle *tex) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_textures >= intrn_renderer->texture_capacity) {
        printf("ERROR: Exceeded Texture Capacity");
        *tex |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_tex_slot == UINT32_MAX) {
        index = intrn_renderer->num_textures;
    }
    else {
        index = intrn_renderer->next_tex_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_renderer->next_tex_slot = index + 1; intrn_renderer->next_tex_slot < intrn_renderer->num_textures; intrn_renderer->next_tex_slot++) {
            if (!intrn_renderer->texture_table[intrn_renderer->next_tex_slot].init)
                break;
        }

        if (intrn_renderer->next_tex_slot >= intrn_renderer->num_textures)
            intrn_renderer->next_tex_slot = UINT32_MAX;
    }

    if(!renderer_functions[intrn_renderer->type].create_texture(intrn_renderer, index, width, height, data, format)) {
        *tex |= BOBi_MSB;
        return 0;
    }

    intrn_renderer->texture_table[index].init = 1; //Setting the value to be initialised
    intrn_renderer->texture_table[index].width = width;
    intrn_renderer->texture_table[index].height = height;
    intrn_renderer->texture_table[index].format = format;

    intrn_renderer->num_textures++;
    *tex = ((uint64_t)renderer << 32) | index;
    return 1;
}

void BOB_texture_free(BOB_Texture_Handle *tex) {
    if(*(tex) & BOBi_MSB) return; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(*tex, &renderer, &index)) return;
    if(index < renderer->next_tex_slot) renderer->next_tex_slot = index;
    *(tex) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_texture_free(renderer, index);
}

//====================================== MATERIAL FUNCTIONS ======================================

uint8_t get_uniform(BOB_Material_Handle mat, char *name, BOB_Uniform_Handle *uniform) {
    if(*uniform & BOBi_MSB) return 0;

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    BOBi_Material_Impl m = renderer->material_table[index];
    for(size_t i = 0; i < m.uniform_count; i++) {
        if(!strcmp(name, m.uniforms[i].name)){
            *uniform = i;
            return 1;
        }
    }

    *uniform |= BOBi_MSB;
    return 0;
}

//Reads the shader data from a file and creates a shader data object
uint8_t BOB_create_shader_data(const char *shader_path, const char *entrypoint_name, BOB_Shader_Type type, BOB_Shader_Data *out) {
    uint8_t *buf;
    int sz = BOBi_read_to_end(shader_path, &buf, 1);

    if(sz < 0) {
        free(buf);
        return 0;
    }
    *out = (BOB_Shader_Data){.shader_code = (const char *)buf, .entrypoint_name = entrypoint_name, .code_buf_sz = sz, .type = type};

    return 1;
}

//Destroys a shader data by freeing the shader code bytes and setting the memory region at the pointer to 0
void BOB_destroy_shader_data(BOB_Shader_Data *data) {
    free((void *)data->shader_code);
    *data = (BOB_Shader_Data){0};
}

uint8_t BOB_create_material(BOB_Renderer_Handle renderer, BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_materials >= intrn_renderer->material_capacity) {
        printf("ERROR: Exceeded Material Capacity\n");
        *mat |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_mat_slot == UINT32_MAX) {
        index = intrn_renderer->num_materials;
    }
    else {
        index = intrn_renderer->next_mat_slot;

        for (intrn_renderer->next_mat_slot = index + 1; intrn_renderer->next_mat_slot < intrn_renderer->num_materials; intrn_renderer->next_mat_slot++) {
            if (!intrn_renderer->material_table[intrn_renderer->next_mat_slot].init)
                break;
        }

        if (intrn_renderer->next_mat_slot >= intrn_renderer->num_materials)
            intrn_renderer->next_mat_slot = UINT32_MAX;
    }

    //TODO: Make an arena for this. Currently need to do this since cannot have references to stack memory in heap memory
    //otherwise will get pointer badness
    BOBi_Uniform_Impl *temp = malloc(sizeof(BOBi_Uniform_Impl) * num_uniforms);
    for(size_t i = 0; i < num_uniforms; i++) {
        temp[i] = (BOBi_Uniform_Impl){.name = uniforms[i].name, .type = uniforms[i].type, .is_reference = uniforms[i].is_reference};
        memcpy(&temp[i].value, &uniforms[i].value, sizeof(uniforms[i].value));
        #ifdef BOB_INCLUDE_VULKAN
        switch (uniforms[i].shader_stage) {
            case BOB_VERTEX_SHADER: temp[i].vulkan.stage = VK_SHADER_STAGE_VERTEX_BIT; break;
            case BOB_FRAGMENT_SHADER: temp[i].vulkan.stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
            default:
                printf("Invalid shader stage\n");
                *mat |= BOBi_MSB;
                intrn_renderer->material_table[index].init = 0;
                free(intrn_renderer->material_table[index].uniforms); //TODO: Change this to be arena based
                return 0;
        }
        #endif
    }

    intrn_renderer->material_table[index] = (BOBi_Material_Impl){.uniforms = temp, .uniform_count = num_uniforms};

    if(!renderer_functions[intrn_renderer->type].create_material(intrn_renderer, index, data, num_shaders)) {
        *mat |= BOBi_MSB;
        intrn_renderer->material_table[index].init = 0;
        free(intrn_renderer->material_table[index].uniforms); //TODO: Change this to be arena based
        return 0;
    }

    intrn_renderer->material_table[index].init = 1;

    intrn_renderer->num_materials++;
    *mat = ((uint64_t)renderer << 32) | index;
    return 1;
}

void BOB_material_free(BOB_Material_Handle *mat) {
    if(*(mat) & BOBi_MSB) return; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    BOBi_get_handle_data(*mat, &renderer, &index);
    if(index < renderer->next_mat_slot) renderer->next_mat_slot = index;
    *(mat) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_material_free(renderer, index);
}

uint8_t BOB_set_material_float(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        renderer->material_table[index].uniforms[uniform].value.f = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        renderer->material_table[index].uniforms[uniform].value.u32 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        renderer->material_table[index].uniforms[uniform].value.i32 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector2(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        renderer->material_table[index].uniforms[uniform].value.vec2 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector3(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        renderer->material_table[index].uniforms[uniform].value.vec3 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        renderer->material_table[index].uniforms[uniform].value.vec4 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_mat4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        renderer->material_table[index].uniforms[uniform].value.mat4 = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_float_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector2_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector3_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_mat4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &renderer, &index)) return 0;

    if(renderer->material_table[index].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        renderer->material_table[index].uniforms[uniform].value.ptr = value;
        renderer->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}

//================================================== TEXTURE ATLAS FUNCTIONS ========================================

//Initialises a texture atlas
uint8_t BOB_atlas_init(BOB_Renderer_Handle renderer, uint32_t width, uint32_t height, BOB_Format format, BOB_Atlas_Handle *a) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_atlases >= intrn_renderer->atlas_capacity) {
        printf("ERROR: Exceeded Atlas Capacity");
        *a |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_atlas_slot == UINT32_MAX) {
        index = intrn_renderer->num_atlases;
    }
    else {
        index = intrn_renderer->next_atlas_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_renderer->next_atlas_slot = index + 1; intrn_renderer->next_atlas_slot < intrn_renderer->num_atlases; intrn_renderer->next_atlas_slot++) {
            if (!intrn_renderer->atlas_table[intrn_renderer->next_atlas_slot].init)
                break;
        }

        if (intrn_renderer->next_atlas_slot >= intrn_renderer->num_atlases)
            intrn_renderer->next_atlas_slot = UINT32_MAX;
    }

    if(!BOB_create_texture(renderer, width, height, NULL, format, &intrn_renderer->atlas_table[index].texture)) {
        intrn_renderer->atlas_table[index] = (BOBi_Atlas_Impl){0};
        *a |= BOBi_MSB;
        return 0;
    }

    intrn_renderer->atlas_table[index].init = 1; //Setting the value to be initialised
    intrn_renderer->atlas_table[index].format = format;
    intrn_renderer->num_atlases++;
    *a = ((uint64_t)renderer << 32) | index;
    return 1;
}

void BOB_atlas_free(BOB_Atlas_Handle *a) {
    if(*a & BOBi_MSB) return; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(*a, &renderer, &index)) return;

    if(index < renderer->next_atlas_slot) renderer->next_atlas_slot = index;
    *(a) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOB_texture_free(&renderer->atlas_table[index].texture);
    renderer->atlas_table[index] = (BOBi_Atlas_Impl){0}; //Clear the data
}

//Returns the UV rect where the texture was placed
//pixel_size must be either 3 or 4. If it does not match the pixel size of the atlas,
//an empty quad will returned as the pixel formats are different
uint8_t BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h, BOB_Quad *out_quad) {
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(a, &renderer, &index)) return 0;

    if((a & BOBi_MSB) || (renderer->atlas_table[index].texture & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOBi_Texture_Impl tex = renderer->texture_table[renderer->atlas_table[index].texture];
    if(renderer->atlas_table[index].cursor_y + h > tex.height) return 0; //Early exit if we can't fit the texture in

    //Move to next row if this texture doesn't fit
    if(renderer->atlas_table[index].cursor_x + w > tex.width) {
       renderer->atlas_table[index].cursor_y += renderer->atlas_table[index].row_height;
       renderer->atlas_table[index].cursor_x = 0;
       renderer->atlas_table[index].row_height = 0;
    }

    BOB_Quad unnormalised = {
        (float)renderer->atlas_table[index].cursor_x / tex.width,
        (float)renderer->atlas_table[index].cursor_y / tex.height,
        (float) w,
        (float) h
    };

    renderer_functions[renderer->type].copy_data_to_tex(renderer, index, renderer->atlas_table[index].format, unnormalised, pixels);

    //Compute normalised UVs
    BOB_Quad uv = {
        (float)renderer->atlas_table[index].cursor_x / tex.width,
        (float)renderer->atlas_table[index].cursor_y / tex.height,
        (float) w / tex.width,
        (float) h / tex.height
    };

    renderer->atlas_table[index].cursor_x += w;
    if(h > renderer->atlas_table[index].row_height) renderer->atlas_table[index].row_height = h;

    *out_quad = uv;
    return 1;
}

//======================================================= PIXELBUFFER FUNCTIONS ==============================================

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
uint8_t BOB_pixelbuffer_init(BOB_Renderer_Handle renderer, size_t width, size_t height, BOB_Format format, BOB_Pixelbuffer_Handle *pb) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_pixelbuffers >= BOB_MAX_PIXELBUFFER_CAPACITY) {
        printf("ERROR: Exceeded Pixelbuffer Capacity");
        *pb |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_pixelbuf_slot == UINT32_MAX) {
        index = intrn_renderer->num_pixelbuffers;
    }
    else {
        index = intrn_renderer->next_pixelbuf_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_renderer->next_pixelbuf_slot = index + 1; intrn_renderer->next_pixelbuf_slot < intrn_renderer->num_pixelbuffers; intrn_renderer->next_pixelbuf_slot++) {
            if (!intrn_renderer->pixelbuffer_table[intrn_renderer->next_pixelbuf_slot].init)
                break;
        }

        if (intrn_renderer->next_pixelbuf_slot >= intrn_renderer->num_pixelbuffers)
            intrn_renderer->next_pixelbuf_slot = UINT32_MAX;
    }


    //Setting up the texture for the pixel simulations:
    if(!BOB_create_texture(renderer, width, height, NULL, format, &intrn_renderer->pixelbuffer_table[index].pixel_tex)) {
        intrn_renderer->pixelbuffer_table[index] = (BOBi_Pixelbuffer_Impl){0};
        return 0;
    }

    //Getting the number of bytes used to store pixel data
    uint8_t pixel_size;
    switch (format) {
        case BOB_RED: pixel_size = 1; break;
        case BOB_RG: pixel_size = 2; break;
        case BOB_RGB: pixel_size = 3; break;
        case BOB_RGBA: pixel_size = 4; break;
    }

    intrn_renderer->pixelbuffer_table[index].buf_sz = width * height * pixel_size;

    //Setting up the pbo for the pixel simulations
    if(!renderer_functions[intrn_renderer->type].create_pixelbuffer(intrn_renderer, index)) {
        *pb |= BOBi_MSB;
        return 0;
    }

    intrn_renderer->pixelbuffer_table[index].init = 1; //Setting the value to be initialised

    intrn_renderer->num_pixelbuffers++;
    *pb = ((uint64_t)renderer << 32) | index;
    return 1;
}
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_Pixelbuffer_Handle *pb) {
    if(*(pb) & BOBi_MSB) return; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(*pb, &renderer, &index)) return;

    if(index < renderer->next_pixelbuf_slot) renderer->next_pixelbuf_slot = index;
    *(pb) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_pixelbuffer_free(renderer, index);
}

//Binds the pixelbuffers gpu memory to cpu memory. This can currently only be done by one pixelbuffer at a time
uint8_t BOB_bind_pixelbuffer_memory(BOB_Pixelbuffer_Handle pb, void **mapped_mem_ptr, size_t *mem_sz) {
    if(pb & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &renderer, &index)) return 0;

    return renderer_functions[renderer->type].bind_memory(renderer, index, mapped_mem_ptr, mem_sz);
}
//Unbinds the pixelbuffer's gpu memory from cpu space
void BOB_unbind_pixelbuffer_memory(BOB_Pixelbuffer_Handle pb) {
    if(pb & BOBi_MSB) return; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &renderer, &index)) return;
    return renderer_functions[renderer->type].unbind_memory(renderer, index);
}
//Uploads the pixel data from the pixelbuffer into its associated texture
void BOB_pixelbuffer_upload(BOB_Pixelbuffer_Handle pb) {
    if(pb & BOBi_MSB) return; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &renderer, &index)) return;

    renderer_functions[renderer->type].update_tex_from_buf(renderer, index);
}

//============================================================= DRAWING FUNCTIONS ===========================================

uint8_t BOB_draw_texture(BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *r;
    BOBi_get_renderer_from_handle(texture, &r);
    return BOB_draw_texture_channel(texture, screen_quad, tex_sub_rect, colour, layer, rotation, r->default_mat, 0);
}

//Draws an atlas quad
uint8_t BOB_draw_atlas_quad(BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation) {
    if(atlas & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *r;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &r, &index)) return 0;
    return BOB_draw_atlas_quad_channel(screen_quad, tex_sub_rect, colour, r->atlas_table[index].texture, layer, rotation, r->default_mat, 0);
}

uint8_t BOB_draw_pixelbuffer(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *r;
    BOBi_get_renderer_from_handle(pb, &r);
    return BOB_draw_pixelbuffer_channel(pb, dimensions, sub_rect, colour, layer, rotation, r->default_mat, 0);
}

uint8_t BOB_draw_line(BOB_Renderer_Handle r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_line_mat(r, start_pos, end_pos, thickness, colour, layer, renderer->default_mat);
}

uint8_t BOB_draw_quad(BOB_Renderer_Handle r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_quad_mat(r, quad, colour, layer, rotation, renderer->default_mat);
}

uint8_t BOB_draw_unfilled_quad(BOB_Renderer_Handle r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_unfilled_quad_mat(r, quad, thickness, colour, layer, rotation, renderer->default_mat);
}

uint8_t BOB_draw_polygon(BOB_Renderer_Handle r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_polygon_mat(r, poly_points, poly_size, colour, layer, rotation, renderer->default_mat);
}

//Draws an unfilled polygon
uint8_t BOB_draw_unfilled_polygon(BOB_Renderer_Handle r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_unfilled_polygon_mat(r, poly_points, poly_size, colour, thickness, layer, rotation, renderer->default_mat);
}

uint8_t BOB_draw_circle(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_circle_mat(r, centre, radius, colour, layer, renderer->default_mat);
}

uint8_t BOB_draw_unfilled_circle(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer) {
    BOBi_Renderer_Impl *renderer;
    BOBi_get_renderer(r, &renderer);
    return BOB_draw_unfilled_circle_mat(r, centre, radius, thickness, colour, layer, renderer->default_mat);
}

//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_mat(BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    return BOB_draw_texture_channel(texture, screen_quad, tex_sub_rect, colour, layer, rotation, mat, 0);
}

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_mat(BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(atlas & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &renderer, &index)) return 0;
    return BOB_draw_atlas_quad_channel(screen_quad, tex_sub_rect, colour, renderer->atlas_table[index].texture, layer, rotation, mat, 0);
}

//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixelbuffer_mat(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    return BOB_draw_pixelbuffer_channel(pb, dimensions, sub_rect, colour, layer, rotation, mat, 0);
}

//Draws a filled circle with a specified material
uint8_t BOB_draw_circle_mat(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    if(!BOBi_get_renderer(r, &renderer)) return 0;

    float angle_step = 2.0f * M_PI / BOB_CIRCLE_LINE_SEGMENTS;
    BOB_Vector2 vertices[BOB_CIRCLE_LINE_SEGMENTS];
    uint32_t indices[BOB_CIRCLE_LINE_SEGMENTS * 3];
    size_t vertex_count = 0, index_count = 0;

    //Generating the vertices for the triangles that make up a circle
    for(int i = 0; i < BOB_CIRCLE_LINE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float x = centre.x + cosf(angle) * radius;
        float y = centre.y - sinf(angle) * radius;

        vertices[vertex_count++] = (BOB_Vector2){x, y};
    }

    BOB_Vector2 points2[BOB_MAX_POLY_SIZE];
    BOB_Vector3 points3[BOB_MAX_POLY_SIZE];
    memcpy(points2, vertices, vertex_count * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(renderer, points2, vertex_count);
    if(clipped_size < 3) return 1; //Early exit

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    //Generating the indecies for the triangle ebo
    for(int i = 0; i < clipped_size; i++) {
        points3[i] = (BOB_Vector3){points2[i].x, points2[i].y, layer};
        indices[index_count++] = 0;
        indices[index_count++] = i;
        indices[index_count++] = ((i+1) % clipped_size);
    }

    BOBi_create_draw_call(r, points3, clipped_size, NULL, clipped_size * 3, colour, renderer->default_tex, mat, 0, BOBi_DRAW_CIRCLE);
    return 1;
}

//Draws a filled quad with a specified material
uint8_t BOB_draw_quad_mat(BOB_Renderer_Handle r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    if(!BOBi_get_renderer(r, &renderer)) return 0;

    //If the quad is not rotated or no clip region, can just clip/draw it like a quad
    if(fabsf(rotation - 0.0f) < 1e-9 || BOB_peek_clip_rect(renderer->stack).empty) {
        if(!BOBi_clip_quad(renderer, &quad)) return 1; //Early exit
        BOB_Vector3 coords[4] = {
            {quad.x, quad.y, layer},
            {quad.x + quad.w, quad.y, layer},
            {quad.x + quad.w, quad.y + quad.h, layer},
            {quad.x, quad.y + quad.h, layer}
        };

        BOBi_create_draw_call(r, coords, BOB_VERTICIES_PER_QUAD, NULL, BOB_INDECIES_PER_QUAD, colour, renderer->default_tex, renderer->default_mat, 0, BOBi_DRAW_QUAD);
    }
    //Otherwise need to clip/draw it like a polygon
    else {
        BOBi_Clip_Rect clip = BOB_peek_clip_rect(renderer->stack);

        BOB_Vector2 rotated_coords[8];
        BOBi_rotate_quad(quad, rotated_coords, rotation);

        size_t new_size = BOBi_clip_polygon(renderer, rotated_coords, 4);
        if(new_size < 3) return 1; //No need to draw an empty polygon

        BOB_Vector3 coords[8];
        for(size_t i = 0; i < new_size; i++) {
            //Need to do this in reverse order to make sure the coords are counter-clockwise for ear clipping
            coords[new_size - i - 1] = (BOB_Vector3){rotated_coords[i].x, rotated_coords[i].y, layer};
        }

        BOBi_create_draw_call(r, coords, new_size, NULL, (new_size == 4) ? BOB_INDECIES_PER_QUAD : 3 * (new_size - 2), colour, renderer->default_tex, mat, 0, (new_size == 4) ? BOBi_DRAW_QUAD : BOBi_DRAW_POLY);
    }
    return 1;
}


//Draws a filled triangle with a specified material
uint8_t BOB_draw_polygon_mat(BOB_Renderer_Handle r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    if(!BOBi_get_renderer(r, &renderer)) return 0;

    BOBi_rotate_polygon(poly_points, poly_size, rotation);

    BOB_Vector2 points[BOB_MAX_POLY_SIZE];
    memcpy(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(renderer, points, poly_size);
    if(clipped_size < 3) return 1; //Early exit

    BOB_Vector3 vertices[BOB_MAX_POLY_SIZE]; //Holds the compressed vertex values
    for(size_t i = 0; i < clipped_size; i++) {
        vertices[i] = (BOB_Vector3){points[i].x, points[i].y, layer};
    }
    BOBi_create_draw_call(r, vertices, clipped_size, NULL, (clipped_size - 2) * 3, colour, renderer->default_tex, mat, 0, BOBi_DRAW_POLY);
    return 1;
}
//Draws an unfilled circle with a specified material
uint8_t BOB_draw_unfilled_circle_mat(BOB_Renderer_Handle r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

    float angle_step = 2.0f * M_PI / BOB_CIRCLE_LINE_SEGMENTS;
    BOB_Vector2 vertices[BOB_CIRCLE_LINE_SEGMENTS];
    size_t vertex_count = 0;

    //Generating the vertices for the lines that make up a circle
    for(size_t i = 0; i < BOB_CIRCLE_LINE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float x = centre.x + cosf(angle) * radius;
        float y = centre.y - sinf(angle) * radius;

        vertices[vertex_count++] = (BOB_Vector2){x, y};
    }

    //Drawing the outline lines
    for(size_t i = 0; i < vertex_count; i++) {
        size_t next = (i+1) % vertex_count;
        BOB_draw_line_mat(r, vertices[i], vertices[next], thickness, colour, layer, mat);
    }

    return 1;
}

//Draws an unfilled quad with a specified material
uint8_t BOB_draw_unfilled_quad_mat(BOB_Renderer_Handle r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Vector2 tl = {quad.x,          quad.y};
    BOB_Vector2 tr = {quad.x + quad.w, quad.y};
    BOB_Vector2 bl = {quad.x,          quad.y + quad.h};
    BOB_Vector2 br = {quad.x + quad.w, quad.y + quad.h};

    BOB_draw_line_mat(r, tl, tr, thickness, colour, layer, mat);
    BOB_draw_line_mat(r, tr, br, thickness, colour, layer, mat);
    BOB_draw_line_mat(r, br, bl, thickness, colour, layer, mat);
    BOB_draw_line_mat(r, bl, tl, thickness, colour, layer, mat);

    return 1;
}

//Draws an unfilled triange with a specified material
uint8_t BOB_draw_unfilled_polygon_mat(BOB_Renderer_Handle r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_Renderer_Impl *renderer;
    if(!BOBi_get_renderer(r, &renderer)) return 0;

    BOB_Vector2 points[BOB_MAX_POLY_SIZE];
    memcpy(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(renderer, points, poly_size);
    if(clipped_size < 2) return 1; //Early exit

    for(size_t i = 0; i < clipped_size; i++) {
        size_t next = (i+1) % clipped_size;
        BOB_draw_line_mat(r, points[i], points[next], thickness, colour, layer, mat);
    }

    return 1;
}

//Draws a line between two points with a specified material
uint8_t BOB_draw_line_mat(BOB_Renderer_Handle r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    if(!BOBi_get_renderer(r, &renderer)) return 0;

    if(!BOBi_clip_line(renderer, &start_pos, &end_pos)) return 1; //Early exit

    BOB_Vector2 delta = {end_pos.x - start_pos.x, end_pos.y - start_pos.y};
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);
    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    if(length > 0 && thickness > 0) {
        float scale = thickness/(2*length);

        BOB_Vector2 radius = {-scale*delta.y, scale*delta.x};
        BOB_Vector3 strip[4] = {
            {start_pos.x - radius.x, start_pos.y - radius.y, layer},
            {end_pos.x - radius.x, end_pos.y - radius.y, layer},
            {end_pos.x + radius.x, end_pos.y + radius.y, layer},
            {start_pos.x + radius.x, start_pos.y + radius.y, layer},
        };

        BOBi_create_draw_call(r, strip, 4, NULL, 6, colour, renderer->default_tex, mat, 0, BOBi_DRAW_QUAD);
    }

    return 1;
}

//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_channel(BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((texture & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(texture, &renderer, &index)) return 0;

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    float width = renderer->texture_table[index].width;
    float height = renderer->texture_table[index].height;

    //If the texture is not rotated or no clip region, can just clip/draw it like a quad
    if(fabsf(rotation - 0.0f) < 1e-9 || BOB_peek_clip_rect(renderer->stack).empty) {
        BOB_Quad clipped_quad = screen_quad;
        if(!BOBi_clip_quad(renderer, &clipped_quad)) return 1; //Early exit
        BOB_Vector3 coords[4] = {
            {clipped_quad.x, clipped_quad.y, layer},
            {clipped_quad.x, clipped_quad.y + clipped_quad.h, layer},
            {clipped_quad.x + clipped_quad.w, clipped_quad.y + clipped_quad.h, layer},
            {clipped_quad.x + clipped_quad.w, clipped_quad.y, layer}
        };

        //Getting the normalised difference of the screen quad after clipping
        BOB_Quad norm_diff = {
            (screen_quad.x - clipped_quad.x) / screen_quad.w,
            (screen_quad.y - clipped_quad.y) / screen_quad.h,
            (screen_quad.w - clipped_quad.w) / screen_quad.w,
            (screen_quad.h - clipped_quad.h) / screen_quad.h,
        };

        //And moving the sub rect region accordingly
        tex_sub_rect.x -= tex_sub_rect.x * norm_diff.x;
        tex_sub_rect.y -= tex_sub_rect.y * norm_diff.y;
        tex_sub_rect.w -= tex_sub_rect.w * norm_diff.w;
        tex_sub_rect.h -= tex_sub_rect.h * norm_diff.h;

        BOB_Vector2 uv[4] = {
            {(tex_sub_rect.x / width), (tex_sub_rect.y / height)},
            {(tex_sub_rect.x / width), ((tex_sub_rect.y + tex_sub_rect.h) / height)},
            {((tex_sub_rect.x + tex_sub_rect.w) / width), ((tex_sub_rect.y + tex_sub_rect.h) / height)},
            {((tex_sub_rect.x + tex_sub_rect.w) / width), (tex_sub_rect.y / height)}
        };

        BOBi_create_draw_call((texture >> 32), coords, BOB_VERTICIES_PER_QUAD, uv, BOB_INDECIES_PER_QUAD, colour, texture, mat, channel, BOBi_DRAW_QUAD);
    }
    //Otherwise need to clip/draw it like a polygon
    else {
        BOB_Vector2 rotated_coords[8];
        BOBi_rotate_quad(screen_quad, rotated_coords, rotation);

        size_t new_size = BOBi_clip_polygon(renderer, rotated_coords, 4);
        if(new_size < 3) return 1; //No need to draw an empty polygon

        //Rotate the clipped points back so we can calculate the new uv region
        BOB_Vector2 orig_clipped_coords[8];
        memcpy(orig_clipped_coords, rotated_coords, sizeof(rotated_coords));
        BOBi_rotate_polygon(orig_clipped_coords, new_size, -rotation);

        BOB_Vector2 uv[8];
        BOB_Vector3 coords[8];
        for(size_t i = 0; i < new_size; i++) {
            //Getting the uv coords after clipping
            uv[i].x = (tex_sub_rect.x + (tex_sub_rect.w * (orig_clipped_coords[i].x - screen_quad.x) / screen_quad.w)) / width;
            uv[i].y = (tex_sub_rect.y + (tex_sub_rect.h * (orig_clipped_coords[i].y - screen_quad.y) / screen_quad.h)) / height;

            //Need to do this in reverse order to make sure the coords are counter-clockwise for ear clipping
            coords[new_size - i - 1] = (BOB_Vector3){rotated_coords[i].x, rotated_coords[i].y, layer};
        }

        BOBi_create_draw_call((texture >> 32), coords, new_size, uv, (new_size == 4) ? BOB_INDECIES_PER_QUAD : 3 * (new_size - 2), colour, texture, mat, channel, (new_size == 4) ? BOBi_DRAW_QUAD : BOBi_DRAW_POLY);
    }

    return 1;
}

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_channel(BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((atlas & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &renderer, &index)) return 0;
    return BOB_draw_texture_channel(renderer->atlas_table[index].texture, screen_quad, tex_sub_rect, colour, layer, rotation, mat, channel);
}

//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixelbuffer_channel(BOB_Pixelbuffer_Handle pb, BOB_Quad dimensions, BOB_Quad sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((pb & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &renderer, &index)) return 0;

    if(!BOBi_clip_quad(renderer, &dimensions)) return 1; //Early exit

    return BOB_draw_texture_channel(renderer->pixelbuffer_table[index].pixel_tex, dimensions, sub_rect, colour, layer, rotation, mat, channel);
}

//=================================== CLIPPING FUNCTIONS =====================================

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer_Handle renderer, BOB_Quad rect, BOB_Clip_Dir dir) {
    BOBi_Renderer_Impl *r;
    if(!BOBi_get_renderer(renderer, &r)) return;

    BOBi_Clip_Stack *stack = r->stack;
    if(stack->size >= stack->capacity) {
        size_t newCap = (stack->capacity == 0) ? 4 : stack->capacity * 2;
        stack->elems = realloc(stack->elems, newCap);
        stack->capacity = newCap;
    }

    BOBi_Clip_Rect clip_rect = (BOBi_Clip_Rect) {
        rect.x, rect.x+rect.w, rect.y, rect.y+rect.h,
        (dir == BOB_CLIP_VERT || dir == BOB_CLIP_BOTH) ? 1 : 0,
        (dir == BOB_CLIP_HORZ || dir == BOB_CLIP_BOTH) ? 1 : 0,
        0
    };

    //Getting the intersection of the old and current rect
    if(stack->size > 0) {
        BOBi_Clip_Rect old_inter = stack->elems[stack->size-1];
        //Early return if the previous rect was empty
        if(old_inter.empty) {
            clip_rect.empty = 1;
            stack->elems[stack->size++] = clip_rect;
            return;
        }

        if(clip_rect.clip_horz && old_inter.clip_horz) {
            clip_rect.left = (clip_rect.left > old_inter.left) ? clip_rect.left : old_inter.left;
            clip_rect.right = (clip_rect.right < old_inter.right) ? clip_rect.right : old_inter.right;
        }
        else if(old_inter.clip_horz) {
            clip_rect.left = old_inter.left;
            clip_rect.right = old_inter.right;
        }

        if(clip_rect.clip_vert && old_inter.clip_vert) {
            clip_rect.top = (clip_rect.top > old_inter.top) ? clip_rect.top : old_inter.top;
            clip_rect.bottom = (clip_rect.bottom < old_inter.bottom) ? clip_rect.bottom : old_inter.bottom;
        }
        else if(old_inter.clip_vert) {
            clip_rect.top = old_inter.top;
            clip_rect.bottom = old_inter.bottom;
        }

        //Update the clipping diclip_rections
        clip_rect.clip_horz |= old_inter.clip_horz;
        clip_rect.clip_vert |= old_inter.clip_vert;
    }

    //Check if the clip_rect is empty
    clip_rect.empty = (clip_rect.left >= clip_rect.right || clip_rect.top >= clip_rect.bottom || (!clip_rect.clip_horz && !clip_rect.clip_vert)) ? 1 : 0;

    stack->elems[stack->size++] = clip_rect;
}

//Removes the first clipping intersection from the stack and returns its value
void BOB_end_clip(BOB_Renderer_Handle renderer) {
    BOBi_Renderer_Impl *r;
    if(!BOBi_get_renderer(renderer, &r)) return;
    assert(r->stack->size > 0 && "Popping an empty stack");

    BOBi_Clip_Rect rect = r->stack->elems[r->stack->size-1];
    r->stack->size--;
}

//===================================== BITMAP FONT RENDERING =============================================

//TODO: Get errors working for the parser
typedef struct {
    uint32_t error_line;
    uint32_t error_col;
    char error_char;
} BOBi_Parse_Error_Data;

BOBi_Parse_Error_Data error_data = {0};

void BOBi_append_glyph(BOBi_Font_Impl *font, BOB_Glyph g) {
    if(font->glyphs == NULL) font->glyphs = malloc(sizeof(BOB_Glyph) * font->glyph_capacity);
    if(font->glyph_count >= font->glyph_capacity) {
        size_t new_cap = (font->glyph_capacity > 0) ? font->glyph_capacity * 2 : 16;
        font->glyphs = realloc(font->glyphs, new_cap);
        font->glyph_capacity = new_cap;
    }

    BOBi_hashmap_add(font->glyph_map, g.codepoint, font->glyph_count);
    font->glyphs[font->glyph_count++] = g;
}
void BOBi_append_kerning(BOBi_Font_Impl *font, BOB_Kerning k) {
    if(font->kernings == NULL) font->kernings = malloc(sizeof(BOB_Kerning) * font->kerning_capacity);
    if(font->kerning_count >= font->kerning_capacity) {
        size_t new_cap = (font->kerning_capacity > 0) ? font->kerning_capacity * 2 : 16;
        font->kernings = realloc(font->kernings, new_cap);
        font->kerning_capacity = new_cap;
    }

    BOBi_hashmap_add(font->kerning_map, ((uint64_t)k.first << 32) | k.second, font->kerning_count);
    font->kernings[font->kerning_count++] = k;
}

uint8_t BOBi_parse_char(char *line, BOB_Glyph *g) {
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
uint8_t BOBi_parse_count(char *line, size_t *num_chars) {
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
uint8_t BOBi_parse_kerning(char *line, BOB_Kerning *k) {
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
uint8_t BOBi_parse_common(char *line, BOBi_Font_Impl *font) {
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

uint8_t BOBi_parse_line(char *line, BOBi_Font_Impl *font) {
    char *space = strchr(line, ' ');
    if (!space) return 0;

    *space = '\0';

    char *tag = line;
    char *rest = space + 1;

    if(!strcmp("info", tag)) return 1;
    else if(!strcmp("page", tag)) return 1; //Skip these two lines
    else if(!strcmp("common", tag)) return BOBi_parse_common(rest, font);
    else if(!strcmp("char", tag)) {
        BOB_Glyph g;
        if(BOBi_parse_char(rest, &g)) {
            BOBi_append_glyph(font, g);
            return 1;
        }
        return 0;
    }
    else if(!strcmp("chars", tag)) {
        if(BOBi_parse_count(rest, &font->glyph_capacity)) {
            font->glyph_map = malloc(sizeof(BOBi_Hashmap));
            if(!BOBi_hashmap_init(font->glyph_capacity, font->glyph_map)) return 0;
            return 1;
        }
        return 0;
    }
    else if(!strcmp("kerning", tag)) {
        BOB_Kerning k;
        if(BOBi_parse_kerning(rest, &k)) {
            BOBi_append_kerning(font, k);
            return 1;
        }
        return 0;
    }
    else if(!strcmp("kernings", tag)) {
        if(BOBi_parse_count(rest, &font->kerning_capacity)) {
            font->kerning_map = malloc(sizeof(BOBi_Hashmap));
            if(!BOBi_hashmap_init(font->kerning_capacity, font->kerning_map)) return 0;
            return 1;
        }
        return 0;
    }
    else return 0;
}

uint8_t BOBi_parse_text(BOBi_Font_Impl *font, uint8_t *data, size_t data_sz) {
    char *line = strtok((char *)data, "\r\n");

    while(line) {
        if(!BOBi_parse_line(line, font)) return 0;
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
} BOBi_BMF_Common_Block;
#pragma pack(pop)
uint8_t BOBi_parse_common_block(BOBi_Font_Impl *font, uint8_t *data, size_t data_sz) {
    if(data_sz != sizeof(BOBi_BMF_Common_Block)) {
        printf("ERROR: Incorrect Common Block size\n");
        return 0;
    }

    BOBi_BMF_Common_Block block;
    memcpy(&block, data, sizeof(BOBi_BMF_Common_Block));
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
} BOBi_BMF_Chars_Block;
#pragma pack(pop)
uint8_t BOBi_parse_chars_block(BOBi_Font_Impl *font, uint8_t *data, size_t data_sz) {
    if(data_sz % sizeof(BOBi_BMF_Chars_Block) != 0) {
        printf("ERROR: Incorrect Char Block size\n");
        return 0;
    }

    size_t num_chars = data_sz / sizeof(BOBi_BMF_Chars_Block);
    font->glyph_capacity = num_chars;
    font->glyph_map = malloc(sizeof(BOBi_Hashmap));
    if(!BOBi_hashmap_init(font->glyph_capacity, font->glyph_map)) return 0;

    for(size_t i = 0; i < num_chars; i++) {
        BOBi_BMF_Chars_Block block;
        memcpy(&block, data, sizeof(BOBi_BMF_Chars_Block));
        BOBi_append_glyph(font, (BOB_Glyph){block.id, (BOB_Quad){block.x, block.y, block.width, block.height}, block.x_offset, block.y_offset, block.x_advance, block.page, block.channel});

        data += sizeof(BOBi_BMF_Chars_Block);
    }

    return 1;
}
//Need to pack these structs since the data itself is packed
#pragma pack(push,1)
typedef struct {
    uint32_t first;
    uint32_t second;
    int16_t amount;
} BOBi_BMF_Kernings_Block;
#pragma pack(pop)
uint8_t BOBi_parse_kernings_block(BOBi_Font_Impl *font, uint8_t *data, size_t data_sz) {
    if(data_sz % sizeof(BOBi_BMF_Kernings_Block) != 0) {
        printf("ERROR: Incorrect Kerning Block size\n");
        return 0;
    }

    size_t num_kernings = data_sz / sizeof(BOBi_BMF_Kernings_Block);
    font->kerning_capacity = num_kernings;
    font->kerning_map = malloc(sizeof(BOBi_Hashmap));
    if(!BOBi_hashmap_init(font->kerning_capacity, font->kerning_map)) return 0;

    for(size_t i = 0; i < num_kernings; i++) {
        BOBi_BMF_Kernings_Block block;
        memcpy(&block, data, sizeof(BOBi_BMF_Kernings_Block));
        BOBi_append_kerning(font, (BOB_Kerning){block.first, block.second, block.amount});

        data += sizeof(BOBi_BMF_Kernings_Block);
    }

    return 1;
}

uint8_t BOBi_parse_binary(BOBi_Font_Impl *font, uint8_t *data, size_t data_sz) {
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
                if(!BOBi_parse_common_block(font, ptr, block_sz)) return 0;
                break;
            case 3: break; //Pages block. Do not need to parse;
            case 4: //Chars block
                if(!BOBi_parse_chars_block(font, ptr, block_sz)) return 0;
                break;
            case 5: //Kernings block
                if(!BOBi_parse_kernings_block(font, ptr, block_sz)) return 0;
                break;
            default:
                printf("ERROR: NON-Existent BMF Binary Block type\n");
                return 0;
        }

        ptr += block_sz;
    }

    return 1;
}

uint8_t BOB_load_bmf_font(BOB_Renderer_Handle renderer, const char *font_path, BOB_BMF_Format format, BOB_Font_Handle *font) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_fonts >= intrn_renderer->font_capacity) {
        printf("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_font_slot == UINT32_MAX) {
        index = intrn_renderer->num_fonts;
    }
    else {
        index = intrn_renderer->next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_renderer->next_font_slot = index + 1; intrn_renderer->next_font_slot < intrn_renderer->num_fonts; intrn_renderer->next_font_slot++) {
            if (!intrn_renderer->font_table[intrn_renderer->next_font_slot].init)
                break;
        }

        if (intrn_renderer->next_font_slot >= intrn_renderer->num_fonts)
            intrn_renderer->next_font_slot = UINT32_MAX;
    }

    intrn_renderer->font_table[index].init = 1; //Setting the value to be initialised

    uint8_t *buf;
    int size = BOBi_read_to_end(font_path, &buf, 1);
    if(size < 0) {
        *font |= BOBi_MSB;
        return 0;
    }

    uint8_t res = (format == BOB_BMF_TEXT) ? BOBi_parse_text(&intrn_renderer->font_table[index], buf, size) : BOBi_parse_binary(&intrn_renderer->font_table[index], buf, size);
    free(buf);
    if(!res) {
        *font |= BOBi_MSB;
        intrn_renderer->font_table[index] = (BOBi_Font_Impl){0}; //Clear all of the initially assigned font data
        return 0;
    }

    intrn_renderer->num_fonts++;
    *font = ((uint64_t)renderer << 32) | index;
    return 1;
}

uint8_t BOB_create_custom_font(BOB_Renderer_Handle renderer, size_t num_glyphs, size_t num_kernings, size_t line_height, size_t base, BOB_Font_Handle *font) {
    BOBi_Renderer_Impl *intrn_renderer;
    if(!BOBi_get_renderer(renderer, &intrn_renderer)) return 0;

    if(intrn_renderer->num_fonts >= intrn_renderer->font_capacity) {
        printf("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_renderer->next_font_slot == UINT32_MAX) {
        index = intrn_renderer->num_fonts;
    }
    else {
        index = intrn_renderer->next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_renderer->next_font_slot = index + 1; intrn_renderer->next_font_slot < intrn_renderer->num_fonts; intrn_renderer->next_font_slot++) {
            if (!intrn_renderer->font_table[intrn_renderer->next_font_slot].init)
                break;
        }

        if (intrn_renderer->next_font_slot >= intrn_renderer->num_fonts)
            intrn_renderer->next_font_slot = UINT32_MAX;
    }

    intrn_renderer->font_table[index].init = 1; //Setting the value to be initialised
    intrn_renderer->font_table[index].base = base;
    intrn_renderer->font_table[index].line_height = line_height;
    intrn_renderer->font_table[index].glyph_capacity = num_glyphs;
    intrn_renderer->font_table[index].glyph_count = 0;
    intrn_renderer->font_table[index].kerning_capacity = num_kernings;
    intrn_renderer->font_table[index].kerning_count = 0;
    intrn_renderer->font_table[index].page_count = 0;
    if(num_glyphs) {
        intrn_renderer->font_table[index].glyphs = malloc(sizeof(BOB_Glyph) * num_glyphs);
        if(!BOBi_hashmap_init(intrn_renderer->font_table[index].glyph_capacity, intrn_renderer->font_table[index].glyph_map)) return 0;
    }
    if(num_kernings) {
        intrn_renderer->font_table[index].kernings = malloc(sizeof(BOB_Kerning) * num_kernings);
        intrn_renderer->font_table[index].kerning_map = malloc(sizeof(BOBi_Hashmap));
        if(!BOBi_hashmap_init(intrn_renderer->font_table[index].kerning_capacity, intrn_renderer->font_table[index].kerning_map)) return 0;
    }

    intrn_renderer->num_fonts++;
    *font = ((uint64_t)renderer << 32) | index;
    return 1;
}

uint8_t BOB_add_font_page(BOB_Font_Handle font, uint32_t page_width, uint32_t page_height, uint8_t *page_data, BOB_Format page_format) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;
    uint32_t handle = (font & 0xFFFFFFFF00000000) >> 32;

     return BOB_create_texture(handle, page_width, page_height, page_data, page_format, &renderer->font_table[index].pages[renderer->font_table[index].page_count++]);
}

uint8_t BOB_draw_codepoint(BOB_Font_Handle font, uint32_t codepoint, BOB_Vector2 *pos, BOB_Vector4 colour, uint16_t layer) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;

    BOBi_Font_Impl f = renderer->font_table[index];
    uint32_t hash_index = BOBi_hashmap_get(f.glyph_map, codepoint);
    if(hash_index == UINT32_MAX) return 0; //Codepoint doesn't exist

    BOB_Glyph g = f.glyphs[hash_index];
    uint32_t tex_index;
    if(!BOBi_get_index_from_handle(f.pages[g.page], &tex_index)) return 0;
    //Setting the flags we pass to the shader
    uint8_t chnl_flags = g.channel | BOB_GLYPH_BIT;
    if(renderer->texture_table[tex_index].format == BOB_RED) chnl_flags |= BOB_GREYSCALE_BIT;

    BOB_draw_texture_channel(f.pages[g.page], (BOB_Quad){pos->x + g.x_offset, pos->y + g.y_offset, g.sub_rect.w, g.sub_rect.h}, g.sub_rect, colour, layer, 0.0f, renderer->default_mat, chnl_flags);
    pos->x += g.x_advance;
    return 1;
}

typedef uint32_t (*BOBi_Codepoint_Reader)(void *str, size_t index);
static uint32_t BOBi_read_char(void *str, size_t index) { return (uint32_t)((char *)str)[index]; }
static uint32_t BOBi_read_codepoint(void *str, size_t index) { return ((uint32_t *)str)[index]; }

static uint8_t BOBi_draw_string(BOB_Font_Handle font, void *str, size_t str_len, BOBi_Codepoint_Reader reader, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;

    BOBi_Font_Impl f = renderer->font_table[index];
    float start_x = start->x;
    uint32_t prev = 0;

    for(size_t i = 0; i < str_len; i++) {
        uint32_t codepoint = reader(str, i);
        switch (codepoint) {
            case '\n':
                start->x = start_x;
                start->y += f.line_height;
                prev = 0;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                start->x += f.glyphs[index].x_advance * 4;
                prev = 0;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && prev != 0) {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)prev << 32) | codepoint);
            if(index != UINT32_MAX) start->x += f.kernings[index].amount;
        }

        if(!BOB_draw_codepoint(font, codepoint, start, colour, layer)) return 0;
        prev = codepoint;
    }

    return 1;
}

uint8_t BOB_draw_char_string(BOB_Font_Handle font, char *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    return BOBi_draw_string(font, str, str_len, BOBi_read_char, start, colour, layer);
}

uint8_t BOB_draw_codepoint_string(BOB_Font_Handle font, uint32_t *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    return BOBi_draw_string(font, str, str_len, BOBi_read_codepoint, start, colour, layer);
}

uint8_t BOB_append_glyph(BOB_Font_Handle font, BOB_Glyph glyph) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;

    BOBi_append_glyph(&renderer->font_table[index], glyph);
    return 1;
}
uint8_t BOB_append_kerning(BOB_Font_Handle font, BOB_Kerning kerning) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;

    BOBi_append_kerning(&renderer->font_table[index], kerning);
    return 1;
}

static uint8_t BOBi_measure_string(void *str, size_t str_len, BOBi_Codepoint_Reader reader, BOB_Font_Handle font, BOB_Vector2 *out) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &renderer, &index)) return 0;

    BOBi_Font_Impl f = renderer->font_table[index];
    float max_w = 0;
    float h = f.line_height;
    float cur_w = 0;
    uint32_t prev = 0;

    for(size_t i = 0; i < str_len; i++) {
        uint32_t codepoint = reader(str, i);
        switch(codepoint) {
            case '\n':
                if(cur_w > max_w) max_w = cur_w;
                cur_w = 0;
                h += f.line_height;
                prev = 0;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                cur_w +=  f.glyphs[index].x_advance * 4;
                prev = 0;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && prev != 0) {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)prev << 32) | codepoint);
            if(index != UINT32_MAX) cur_w += f.kernings[index].amount;
        }
        uint32_t index = BOBi_hashmap_get(f.glyph_map, codepoint);
        if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
        cur_w += f.glyphs[index].x_advance;
        prev = codepoint;
    }

    if(cur_w > max_w) max_w = cur_w;
    *out = (BOB_Vector2){max_w, h};

    return 1;
}

uint8_t BOB_measure_char_string(char *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out) {
    return BOBi_measure_string(str, str_len, BOBi_read_char, font, out);
}

uint8_t BOB_measure_codepoint_string(uint32_t *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out) {
    return BOBi_measure_string(str, str_len, BOBi_read_codepoint, font, out);
}

void BOB_print_parsing_error(void) {
    printf("Error Line: %u\nError Column: %u\nError Char: %c\n", error_data.error_line, error_data.error_col, error_data.error_char);
}

uint8_t BOB_font_free(BOB_Font_Handle *font) {
    if((*font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOBi_Renderer_Impl *renderer;
    uint32_t index;
    if(!BOBi_get_handle_data(*font, &renderer, &index)) return 0;

    BOBi_Font_Impl f = renderer->font_table[index];

    for(size_t i = 0; i < f.page_count; i++) {
        BOB_texture_free(&f.pages[i]);
    }

    BOBi_font_free(renderer, *font);

    *font |= BOBi_MSB;
    return 1;
}
#endif
