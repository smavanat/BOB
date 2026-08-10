#ifndef BOB_H
#define BOB_H
#include <stdint.h>
#include <stddef.h>

//TODO: Hide all of the structs in the .c file and use handles everywhere
//
//      Use macros to hide getting index and context from a handle and finding index where next object is placed
//      Do proper error reporting and document what each error code means somewhere
//      Debug mode with statistics
//      Reduce number of memory allocations cpu-side and in the Vulkan backend
//      Allow more customisability in the shaders in general
//      Use texture arrays instead of binding textures every draw call (and ssbos for shaders (opengl))
//      Allow the user to define render passes -> Custom framebuffers
//      Allow the user to define their own pipeline and sampler layout
//      Allow the user to select what kind of device you want vulkan to use
//      Custom vertex layout
//      Compute shader support

#define BOB_INCLUDE_GLAD
// #define BOB_INCLUDE_VULKAN

typedef uint64_t BOB_Texture_Handle;
typedef uint64_t BOB_Material_Handle;
typedef uint64_t BOB_Atlas_Handle;
typedef uint64_t BOB_PixelBuffer_Handle;
typedef uint64_t BOB_Uniform_Handle;
typedef uint64_t BOB_Font_Handle;
typedef uint32_t BOB_Renderer_Handle;

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

//Callback function to create the vulkan surface used to represent the window
typedef uint8_t (*BOB_vk_create_surface)(VkInstance, VkSurfaceKHR *);

//Can only be done in Vulkan contexts
void BOB_create_pipeline(BOB_Renderer_Handle r);
#endif // BOB_INCLUDE_VULKAN

#ifndef BOB_ASSERT
#include <assert.h>
#define BOB_ASSERT assert
#endif //BOB_ASSERT

typedef struct {
    float m[4][4];
} BOB_Mat4;

//Arbitrary constants for now
#define BOB_INIT_TRIANGLES 2048
#define BOB_INIT_QUADS 4096

#define BOB_VERTICIES_PER_QUAD 4
#define BOB_VERTICIES_PER_TRIANGLE 3
#define BOB_INDECIES_PER_QUAD 6
#define BOB_INDECIES_PER_TRIANGLE 3
#define BOB_INVALID_TEX_INDEX 1248
#define BOB_CIRCLE_LINE_SEGMENTS 64 //Number of line segments that make up the circumference of a circle
#define BOB_MAX_VERTEX_CAPACITY 1048576
#define BOB_MAX_INDEX_CAPACITY 2097152
#define BOB_MAX_TEX_CAPACITY 32
#define BOB_MAX_ATLAS_CAPACITY 16
#define BOB_MAX_PIXELBUFFER_CAPACITY 8
#define BOB_MAX_MATERIAL_CAPACITY 16
#define BOB_MAX_FONT_CAPACITY 32
#define BOB_MAX_DRAW_CALL_CAPACITY BOB_MAX_VERTEX_CAPACITY / 3 //Since minimum number of vertices in a draw call is 3
#define INIT_STACK_CAPACITY 64
#define BOB_MAX_LAYER 1024
#define BOB_MAX_SHADERS 32
#define BOB_MAX_UNIFORMS 64

typedef struct BOBi_Arena_t BOBi_Arena;

struct BOBi_Arena_t{
    void *memory;
    size_t capacity;
    size_t offset;
};

uint8_t BOB_init_arena(BOBi_Arena *arena, size_t capacity);
void BOB_destroy_arena(BOBi_Arena *arena);
void *BOB_arena_alloc(BOBi_Arena *arena, size_t size, size_t alignment);
void BOB_arena_clear(BOBi_Arena *arena);

typedef enum {
    #ifdef BOB_INCLUDE_GLAD
    BOB_OPENGL_RENDERER,
    #endif //BOB_INCLUDE_GLAD
    #ifdef BOB_INCLUDE_VULKAN
    BOB_VULKAN_RENDERER,
    #endif //BOB_INCLUDE_VULKAN
    BOB_NUM_RENDERER_TYPES,
} BOB_Renderer_Type;

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

//Data structure to hold data about a single render vertex
typedef struct {
    BOB_Vector4 colour; //The colour of the vertex
    BOB_Vector3 pos; //The on-screen position of the render vertex
    BOB_Vector2 uv; //The (u,v) coordinates of the vertex
    uint8_t flags; //Flag order: 0:red, 1:green, 2:blue, 3:alpha, 4:glyph, 5:greyscale.
} BOB_Render_Vertex;

#define BOB_RED_CHNL_BIT 1
#define BOB_GREEN_CHNL_BIT 2
#define BOB_BLUE_CHNL_BIT 4
#define BOB_ALPHA_CHNL_BIT 8
#define BOB_GLYPH_BIT 16
#define BOB_GREYSCALE_BIT 32

typedef enum {
    BOB_RED,
    BOB_RG,
    BOB_RGB,
    BOB_RGBA
} BOB_Format;

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
} BOB_Texture;

//Creates a new texture on the gpu
uint8_t BOB_create_texture(BOB_Renderer_Handle r, uint32_t width, uint32_t height, uint8_t *data, BOB_Format format, BOB_Texture_Handle *tex);
void BOB_texture_free(BOB_Texture_Handle *tex);
BOB_Texture *BOB_get_tex_ref(BOB_Texture_Handle tex);

typedef struct {
    size_t buf_sz;
    uint32_t pbo; //pbo this renderer uses
    BOB_Texture_Handle pixel_tex; //The texture the pbo is rendered to
    uint8_t init;
} BOB_PixelBuffer;

//Creates a pixel buffer to hold the pixels representing a texture of size width * height
uint8_t BOB_pixelbuffer_init(BOB_Renderer_Handle r, size_t width, size_t height, BOB_Format format, BOB_PixelBuffer_Handle *pb);
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer_Handle *pb);
//Binds the pixelbuffers gpu memory to cpu memory. This can currently only be done by one pixelbuffer at a time
//TODO: See if we can support multiple buffers having their gpu memory bound to cpu memory
uint8_t BOB_bind_pixelbuffer_memory(BOB_PixelBuffer_Handle pb);
//Unbinds the pixelbuffer's gpu memory from cpu space
void BOB_unbind_pixelbuffer_memory(BOB_PixelBuffer_Handle pb);
//NOTE: The following three functions must be called between BOB_bind_pixelbuffer_memory and BOB_unbind_pixelbuffer_memory otherwise they will fail/cause undefined behaviour

//Updates the pixel data stored in a pixelbuffer
void BOB_pixelbuffer_send_data(BOB_PixelBuffer_Handle pb, uint8_t *data, size_t data_sz);
//Gets the pixel data from a PixelBuffer
void BOB_pixelbuffer_get_data(BOB_PixelBuffer_Handle pb, uint8_t *dest, size_t data_sz);
//Uploads the pixel data from the pixelbuffer into its associated texture
void BOB_pixelbuffer_updload(BOB_PixelBuffer_Handle pb);
BOB_PixelBuffer *BOB_get_pixelbuf_ref(BOB_PixelBuffer_Handle pb);

typedef struct {
    BOB_Texture_Handle texture; //GL index of the atlas texture
    uint32_t cursor_x, cursor_y; //Current packing position
    uint32_t row_height; //Height of the tallest texture on the current row
    BOB_Format format; //Pixel format of the texture
    uint8_t init;
} BOB_Atlas;

//Initialises a blank texture atlas
uint8_t BOB_atlas_init(BOB_Renderer_Handle r, uint32_t width, uint32_t height, BOB_Format format, BOB_Atlas_Handle *a);
//Returns the UV rect where the texture was placed
uint8_t BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h, BOB_Quad *out_quad);
//Frees a texture atlas
void BOB_atlas_free(BOB_Atlas_Handle *a);
BOB_Atlas *BOB_get_atlas_ref(BOB_Atlas_Handle a);

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

typedef enum {
    BOB_UNIFORM_FLOAT,
    BOB_UNIFORM_UNSIGNED_INT,
    BOB_UNIFORM_SIGNED_INT,
    BOB_UNIFORM_VEC2,
    BOB_UNIFORM_VEC3,
    BOB_UNIFORM_VEC4,
    BOB_UNIFORM_MAT4,
} BOB_Uniform_Type;

//TODO: Make this struct hidden and create a public one that
//only holds name, value, and type, not any api specific stuff. Won't matter since we memcpy everything anyway
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
            VkShaderStageFlags stage; //TODO: Make these settable by the user using an API enum
        } vulkan;
        #endif //BOB_INCLUDE_VULKAN
    };

    BOB_Uniform_Type type;
    uint8_t is_reference;
} BOB_Uniform;

#define BOB_uniform_float(u_name, value) (BOB_Uniform){.name = (u_name), .f = (value), .type = BOB_UNIFORM_FLOAT, .is_reference = 0}
#define BOB_uniform_unsigned_int(u_name, value) (BOB_Uniform){.name = (u_name), .u32 = (value), .type = BOB_UNIFORM_UNSIGNED_INT, .is_reference = 0}
#define BOB_uniform_signed_int(u_name, value) (BOB_Uniform){.name = (u_name), .i32 = (value), .type = BOB_UNIFORM_SIGNED_INT, .is_reference = 0}
#define BOB_uniform_vector2(u_name, value) (BOB_Uniform){.name = (u_name), .vec2 = (value), .type = BOB_UNIFORM_VEC2, .is_reference = 0}
#define BOB_uniform_vector3(u_name, value) (BOB_Uniform){.name = (u_name), .vec3 = (value), .type = BOB_UNIFORM_VEC3, .is_reference = 0}
#define BOB_uniform_vector4(u_name, value) (BOB_Uniform){.name = (u_name), .vec4 = (value), .type = BOB_UNIFORM_VEC4, .is_reference = 0}
#define BOB_uniform_mat4(u_name, value) (BOB_Uniform){.name = (u_name), .mat4 = (value), .type = BOB_UNIFORM_MAT4, .is_reference = 0}
#define BOB_uniform_float_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_FLOAT, .is_reference = 1}
#define BOB_uniform_unsigned_int_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_UNSIGNED_INT, .is_reference = 1}
#define BOB_uniform_signed_int_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_SIGNED_INT, .is_reference = 1}
#define BOB_uniform_vector2_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC2, .is_reference = 1}
#define BOB_uniform_vector3_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC3, .is_reference = 1}
#define BOB_uniform_vector4_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_VEC4, .is_reference = 1}
#define BOB_uniform_mat4_ref(u_name, value) (BOB_Uniform){.name = (u_name), .ptr = (value), .type = BOB_UNIFORM_MAT4, .is_reference = 1}

uint8_t get_uniform(BOB_Material_Handle mat, char *name, BOB_Uniform_Handle *uniform);

//TODO: Make the shader/pipeline independent of the material?
//      Only when we add other stuff to the material like blend modes etc
//      Because right now the material is the shader
typedef struct {
    BOB_Uniform *uniforms;
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
} BOB_Material;

uint8_t BOB_create_material(BOB_Renderer_Handle r, BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat);
void BOB_material_free(BOB_Material_Handle *mat);
BOB_Material *BOB_get_mat_ref(BOB_Material_Handle mat);

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
    BOBi_Arena vertex_arena; //Used for indices as well
    BOBi_Arena vertex_arena_2;
    BOBi_Arena draw_call_arena;
    size_t num_vertices;
    size_t num_indices;
    size_t num_draw_calls;
} RenderBatch;

typedef struct BOB_Font BOB_Font;

//TODO: Change renderers to also just return handles to the user and keep this struct internal
//      Store renderers in bob_state alongside contexts
typedef struct {
    BOBi_Clip_Stack *stack; //Stores the current clipping rect and the history
    BOB_Mat4 projection; //projection matrix for this renderer
    RenderBatch batch; //Vertex/Index/Draw call memory of the renderer

    BOBi_Arena renderer_memory; //Memory arena that this context uses. Each table is just a pointer into this arena

    BOB_Atlas *atlas_table;
    BOB_PixelBuffer *pixelbuffer_table;
    BOB_Texture *texture_table;
    BOB_Material *material_table;
    BOB_Font *font_table;

    void *mapped_mem_ptr; //Pointer to cpu memory mapped from gpu memory
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

    BOB_Renderer_Type type;
    uint8_t frame_state; //Holds the state of the renderer: 0 - first ever frame 1 - start of new frame 2 - in the middle of drawing a frame
} BOB_Renderer;

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer_Handle r, BOB_Quad rect, BOB_Clip_Dir dir);
//Removes the first clipping intersection from the stack and returns its value
void BOB_end_clip(BOB_Renderer_Handle r);

// //Initialises the pixel renderer
// uint8_t BOB_renderer_init(BOB_Renderer_Handle context, size_t width, size_t height, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, BOB_Renderer_Handle out);
// //Frees a pixel renderer
// void BOB_renderer_free(BOB_Renderer_Handle r);
//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer_Handle r, float clear_colour[4]);
//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer_Handle r);
//Updates the dimensions of the screen that the renderer renders to.
//Updates projection matrix
void BOB_renderer_update_dimensions(BOB_Renderer_Handle r, uint32_t width, uint32_t height, uint32_t width_px, uint32_t height_px);

//Draws a quad
uint8_t BOB_draw_atlas_quad(BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation);
//Draws a dynamically allocated texture
uint8_t BOB_draw_texture(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation);
//Draws a pixel buffer
uint8_t BOB_draw_pixel_buffer(BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation);
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
uint8_t BOB_draw_atlas_quad_mat(BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_mat(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixel_buffer_mat(BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat);
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
uint8_t BOB_draw_atlas_quad_channel(BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);
//Draws a dynamically allocated texture with a specified material and channel
uint8_t BOB_draw_texture_channel(BOB_Texture_Handle texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);
//Draws a pixel buffer with a specified erial and channel
uint8_t BOB_draw_pixel_buffer_channel(BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel);

//Determines the projection matrix
void BOB_ortho_gl(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest);
void BOB_ortho_vk(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest);
//Converts an angle in degrees to radians
float BOB_degrees_to_radians(float angle);

//Font structs
typedef struct {
    uint32_t codepoint; //Unicode codepoint
    BOB_Quad sub_rect;
    float x_offset, y_offset, x_advance;
    uint8_t page;
    uint8_t channel;
} BOB_Glyph;

typedef struct {
    uint32_t first, second; //Codepoints of the chars involved in the kerning
    float amount; //How much the xpos should be adjusted when drawing the second char immediately following the first
} BOB_Kerning;

//TODO: Make this hidden
typedef struct {
    size_t size;
    size_t capacity;
    uint64_t *keys;
    uint32_t *values;
} BOBi_Hashmap;

struct BOB_Font {
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
};

typedef enum {
    BOB_BMF_BINARY,
    BOB_BMF_TEXT,
} BOB_BMF_Format;

uint8_t BOB_create_custom_font(BOB_Renderer_Handle r, BOB_Font_Handle *font, size_t num_glyphs, size_t num_kernings, size_t line_height, size_t base);
uint8_t BOB_load_bmf_font(BOB_Renderer_Handle r, const char *font_path, BOB_Font_Handle *font, BOB_BMF_Format format);
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

#endif //BOB_H
