#include "bob.h"
#include <ctype.h>
#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

//Return the value of the element at the top of the stack without popping it
#define BOB_peek_clip_rect(stack) (((stack)->size > 0) ? (stack)->elems[(stack)->size-1] : (BOBi_Clip_Rect){0})
#define BOBi_MSB 0x80000000

typedef struct {
    BOB_Context *contexts;
    size_t context_count;
    size_t context_capcity;
    size_t next_context_slot;
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

struct BOBi_Context_t {
    // void *context_memory;
    BOBi_Arena context_memory; //Memory arena that this context uses. Each table is just a pointer into this arena

    BOB_Atlas *atlas_table;
    BOB_PixelBuffer *pixelbuffer_table;
    BOB_Texture *texture_table;
    BOB_Material *material_table;
    BOB_Font *font_table;

    //Vulkan members
    #ifdef BOB_INCLUDE_VULKAN
    BOBi_Vulkan_Frame_Resources resources[MAX_FRAMES_IN_FLIGHT]; //Frames in flight

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

    VkCommandPool command_pool;

    BOBi_Vulkan_Image depth;
    uint8_t framebuffer_resized;
    uint8_t frame_index;
    #endif

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

    BOB_Context_Type type;
};

#define BOBi_get_arena_elem(arena, index, type) ((type *)(arena).memory)[(index)]

uint8_t BOBi_get_context_from_handle(uint64_t handle, BOB_Context **out) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles

    uint32_t index = (handle & 0xFFFFFFFF00000000) >> 32;
    if(bob_state.contexts[index].context_memory.memory == NULL) return 0; //Invalid context
    *out = &bob_state.contexts[index];
    return 1;
}

uint8_t BOBi_get_index_from_handle(uint64_t handle, uint32_t *out) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles
    *out = handle & (~0xFFFFFFFF80000000);
    return 1;
}

uint8_t BOBi_get_handle_data(uint64_t handle, BOB_Context **context, uint32_t *index) {
    if(handle & BOBi_MSB) return 0; //Do not work with invalid handles

    uint32_t context_index = (handle & 0xFFFFFFFF00000000) >> 32;
    if(bob_state.contexts[context_index].context_memory.memory == NULL) return 0; //Invalid context
    *context = &bob_state.contexts[context_index];

    *index = handle & (~0xFFFFFFFF80000000);
    return 1;
}

uint8_t BOBi_get_context(BOB_Context_Handle handle, BOB_Context **out) {
    if(handle & BOBi_MSB) {
        printf("Invalid context handle\n");
        return 0;
    }

    BOB_Context *context = &bob_state.contexts[handle];
    if(context->context_memory.memory == NULL) {
        printf("Invalid context handle\n");
        return 0;
    }

    *out = context;
    return 1;
}

uint8_t BOBi_create_context(BOB_Context_Type type, size_t atlas_capacity, size_t pixelbuf_capacity,
                           size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t width, size_t height, BOB_Context_Handle *context);

typedef enum {
    BOBi_DRAW_QUAD,
    BOBi_DRAW_CIRCLE,
    BOBi_DRAW_POLY,
} BOBi_Draw_Type;

typedef struct {
    BOB_Render_Vertex *vertices; //Pointer into the vertex arena where this draw call's vertices start
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

void BOBi_gl_update_uniform(BOB_Uniform uniform) {
    switch(uniform.type) {
        case BOB_UNIFORM_FLOAT:
            glUniform1f(uniform.location, (uniform.is_reference) ? *(float *)uniform.ptr : uniform.f);
            break;
        case BOB_UNIFORM_UNSIGNED_INT:
            glUniform1ui(uniform.location, (uniform.is_reference) ? *(uint32_t *)uniform.ptr : uniform.u32);
            break;
        case BOB_UNIFORM_SIGNED_INT:
            glUniform1i(uniform.location, (uniform.is_reference) ? *(int32_t *)uniform.ptr : uniform.i32);
            break;
        case BOB_UNIFORM_VEC2:
            glUniform2fv(uniform.location, 1, (uniform.is_reference) ? &(*(BOB_Vector2 *)uniform.ptr).x : &uniform.vec2.x);
            break;
        case BOB_UNIFORM_VEC3:
            glUniform3fv(uniform.location, 1, (uniform.is_reference) ? &(*(BOB_Vector3 *)uniform.ptr).x : &uniform.vec3.x);
            break;
        case BOB_UNIFORM_VEC4:
            glUniform4fv(uniform.location, 1, (uniform.is_reference) ? &(*(BOB_Vector4 *)uniform.ptr).x : &uniform.vec4.x);
            break;
        case BOB_UNIFORM_TEXTURE: {
            BOB_Texture_Handle handle = (uniform.is_reference) ? *(BOB_Texture_Handle *)uniform.ptr : uniform.tex_index;
            BOB_Context *context;
            uint32_t index;
            if(BOBi_get_handle_data(handle, &context, &index)) return;
            glUniform1i(uniform.location, context->texture_table[index].opengl.texture);
            break;
        }
        case BOB_UNIFORM_MAT4:
            glUniformMatrix4fv(uniform.location, 1, GL_FALSE, (uniform.is_reference) ? (float *)(*(BOB_Mat4 *)uniform.ptr).m : (float *)uniform.mat4.m);
            break;
    }
}

uint32_t BOBi_gl_convert_format(BOB_Format format) {
    switch (format) {
        case BOB_RED: return GL_RED;
        case BOB_RG: return GL_RG;
        case BOB_RGB: return GL_RGB;
        case BOB_RGBA: return GL_RGBA;
    }
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

void BOBi_gl_delete_texture(uint32_t *tex) {
    glDeleteTextures(1, tex);
}

void BOBi_gl_delete_buffer(uint32_t *buf) {
    glDeleteBuffers(1, buf);
}

void BOBi_gl_delete_program(uint32_t program) {
    glDeleteProgram(program);
}

void BOBi_gl_clear_color(BOB_Vector4 colour) {
    glClearColor(colour.x, colour.y, colour.z, colour.w);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Need to clear the depth buffer as well
}

uint8_t BOB_create_opengl_context(size_t atlas_capacity, size_t pixelbuf_capacity,
                           size_t tex_capacity, size_t mat_capacity, size_t font_capacity, BOB_Context_Handle *context) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);

    return BOBi_create_context(BOB_OPENGL_CONTEXT, atlas_capacity, pixelbuf_capacity, tex_capacity, mat_capacity, font_capacity, 0, 0, context);
}

void BOBi_gl_init_gpu_renderer_mem(BOB_Renderer *r, size_t vert_buf_sz, size_t index_buf_sz) {
    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);

    //Getting the vbo
    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, vert_buf_sz, NULL, GL_DYNAMIC_DRAW);

    //Getting the ebo
    glGenBuffers(1, &r->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_buf_sz, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, colour)); //Vertex Colour
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, pos)); //Vertex Position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, uv)); //UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, flags));
    glEnableVertexAttribArray(3);
}

void BOBi_gl_draw(BOB_Context *context, BOB_Renderer *r) {
    //Bind all of the arrays and buffers we will reuse over time
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);

    BOBi_Draw_Call start = BOBi_get_arena_elem(r->batch.draw_call_arena, 0, BOBi_Draw_Call);
    glBufferSubData(GL_ARRAY_BUFFER, 0, r->batch.num_vertices * sizeof(BOB_Render_Vertex), r->batch.vertex_arena_2.memory); //Copies the data from renderer's triangle data into the vbo
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, r->batch.num_indices * sizeof(uint32_t), r->batch.vertex_arena.memory); //Copies the quad data into the vbo

    //TODO: At the very least make a texture array so we don't keep switching textures, but would also be nice to make an SSBO for the materials
    for(size_t i = 0; i < r->batch.num_draw_calls; i++) {
        BOBi_Draw_Call call = BOBi_get_arena_elem(r->batch.draw_call_arena, i, BOBi_Draw_Call);
        uint32_t mat_index, tex_index;
        if(!BOBi_get_index_from_handle(call.mat, &mat_index)) return;
        if(!BOBi_get_index_from_handle(call.tex, &tex_index)) return;
        glUseProgram(context->material_table[mat_index].shader);
        //Setting the uniforms
        for(size_t j = 0; j < context->material_table[mat_index].uniform_count; j++) {
            BOBi_gl_update_uniform(context->material_table[mat_index].uniforms[j]);
        }

        //Bind the atlas texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, context->texture_table[tex_index].opengl.texture);

        glDrawElements(GL_TRIANGLES, call.num_indices, GL_UNSIGNED_INT, (void *)(call.index_offset * sizeof(uint32_t))); //Make the draw call
    }
}

void BOBi_gl_copy_buffer_data(uint32_t buf, void *data, size_t data_sz) {
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, data_sz, data);
}

void BOBi_gl_create_tex(uint32_t *tex, size_t width, size_t height, uint8_t *data, BOB_Format format) {
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
}

uint8_t BOBi_gl_create_material(BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat, uint32_t *shader) {
    //TODO: Figure out how to get rid of this VLA without using malloc
    uint32_t shader_buf[num_shaders]; //Array to store the ids of the loaded shader sub-programs
    *shader = glCreateProgram();

    //Attaching all of the shaders together
    for(int i = 0; i < num_shaders; i++) {
        shader_buf[i] = BOBi_gl_create_shader(data[i]);
        glAttachShader(*shader, shader_buf[i]);
    }

    glLinkProgram(*shader);
    int result;
    char infolog[512];

    //Print errors if any:
    glGetProgramiv(*shader, GL_LINK_STATUS, &result);
    if(!result) {
        glGetProgramInfoLog(*shader, 512, NULL, infolog);
        printf("ERROR::SHADER::LINKING_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            printf("%c", infolog[i]);
        }
        printf("\n");
        *mat |= BOBi_MSB;
        return 0;
    }

    //Cleanup
    for(int i = 0; i < num_shaders; i++) {
        glDeleteShader(shader_buf[i]);
    }

    //Setting the uniforms
    for(size_t i = 0; i < num_uniforms; i++) {
        uniforms[i].location = glGetUniformLocation(*shader, uniforms[i].name);
    }
    return 1;
}

void BOBi_gl_copy_data_tex(uint32_t tex, BOB_Format format, BOB_Quad region, uint8_t *pixels) {
    GLenum gl_format = BOBi_gl_convert_format(format);

    //Upload the subregion
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, region.x, region.y, region.w, region.h, gl_format, GL_UNSIGNED_BYTE, pixels);
}

uint8_t BOBi_gl_init_pbo(uint32_t *pbo, size_t buf_sz, BOB_PixelBuffer_Handle *pb) {
    glGenBuffers(1, pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, buf_sz, NULL, GL_STREAM_DRAW);
    uint8_t *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if(ptr == NULL) {
        printf("Failed to map GPU to CPU memory\n");
        *pb |= BOBi_MSB;
        return 0;
    }
    memset(ptr, 0x00, buf_sz); //Setting all of the pixels to be colourless initially
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 1;
}

uint8_t BOBi_gl_copy_pbo(uint32_t pbo, size_t buf_sz, uint32_t tex, size_t width, size_t height, uint8_t *data) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if(ptr == NULL) {
        printf("Failed to map GPU to CPU memory\n");
        return 0;
    }
    memcpy(ptr, data, buf_sz);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 1;
}

uint8_t BOBi_gl_get_pbo_data(uint32_t pbo, size_t buf_sz, uint8_t *dest) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
    if(ptr == NULL) {
        printf("Failed to map GPU to CPU memory\n");
        return 0;
    }
    memcpy(dest, ptr, buf_sz);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return 1;
}

void BOBi_gl_upload_pbo_data(uint32_t pbo, uint32_t tex, size_t width, size_t height) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

#endif //BOB_INCLUDE_GLAD

//================================================= VULKAN FUNCTIONS ================================================

#ifdef BOB_INCLUDE_VULKAN

//UBO object we send of as a uniform to the shaders
//Needs to be aligned to 16 so that Vulkan reads the data properly
typedef struct {
    alignas(16) BOB_Mat4 model;
    alignas(16) BOB_Mat4 view;
    alignas(16) BOB_Mat4 proj;
} BOBi_Vulkan_UniformBufferObject;

//Validation layers we are using. TODO: Enable
const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
size_t num_validation_layers = 1;
//Extensions we are using
const char *required_device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; //Need the swapchain extension for drawing surfaces to a window
size_t num_required_device_extensions = 1;

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
    return (VkVertexInputBindingDescription){.binding = 0, .stride = sizeof(BOB_Render_Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
}

//Returns the VAO for our Vertex struct
void BOBi_vk_get_attrib_descs(VkVertexInputAttributeDescription *out_list, size_t *sz) {
    *sz = 3;
    out_list[0] = (VkVertexInputAttributeDescription){.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(BOB_Render_Vertex, colour)};
    out_list[1] = (VkVertexInputAttributeDescription){.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(BOB_Render_Vertex, pos)};
    out_list[2] = (VkVertexInputAttributeDescription){.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(BOB_Render_Vertex, uv)};
    out_list[3] = (VkVertexInputAttributeDescription){.location = 3, .binding = 0, .format = VK_FORMAT_R8_UINT, .offset = offsetof(BOB_Render_Vertex, flags)};
}

//Allocates and begins a given command buffer. Should only be used if a command buffer needs to be used once
uint8_t BOBi_vk_begin_single_time_commands(BOB_Context *context, VkCommandBuffer *out) {
    //Allocate the command buffer
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = NULL,
        .commandPool = context->command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1
    };
    VULKAN_ERROR(vkAllocateCommandBuffers(context->log_device, &alloc_info, out), "Failed to allocate a command buffer");

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
uint8_t BOBi_vk_end_single_time_commands(BOB_Context *context, VkCommandBuffer buf) {
    //End the command buffer
    VULKAN_ERROR(vkEndCommandBuffer(buf), "Failed to end copy command buffer",
                          vkFreeCommandBuffers(context->log_device, context->command_pool, 1, &buf));

    //Submit the command buffer's instructions to the graphics_queue
    VkSubmitInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL,
        .commandBufferCount = 1, .pCommandBuffers = &buf
    };
    VULKAN_ERROR(vkQueueSubmit(context->graphics_queue, 1, &queue_info, VK_NULL_HANDLE), "Failed to submit commands to graphics queue",
                          vkFreeCommandBuffers(context->log_device, context->command_pool, 1, &buf));
    //Wait until the queue is idle to continue with the program
    VULKAN_ERROR(vkQueueWaitIdle(context->graphics_queue), "Failed to wait for commands to complete",
                          vkFreeCommandBuffers(context->log_device, context->command_pool, 1, &buf));

    //Free command buffer memory
    vkFreeCommandBuffers(context->log_device, context->command_pool, 1, &buf);

    return 1;
}

//Gets the index of the memory type that matches our desired properties
uint8_t BOBi_vk_find_memory_type(BOB_Context *context, uint32_t type_filter, VkMemoryPropertyFlags properties, uint32_t *out) {
    //Getting the properties used on our current physical device
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(context->phy_device, &mem_properties);

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

void BOBi_vk_destroy_image(BOB_Context *context, BOBi_Vulkan_Image *image) {
    if(image->view != VK_NULL_HANDLE) vkDestroyImageView(context->log_device, image->view, NULL);
    if(image->memory != VK_NULL_HANDLE) vkFreeMemory(context->log_device, image->memory, NULL);
    if(image->image != VK_NULL_HANDLE) vkDestroyImage(context->log_device, image->image, NULL);
}

//Creates an image and its allocated memory
uint8_t BOBi_vk_create_image(BOB_Context *context, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
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
    VULKAN_ERROR(vkCreateImage(context->log_device, &image_info, NULL, &out_image->image), "Failed to create image");

    //Get the memory requirements to store the image
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(context->log_device, out_image->image, &mem_req);

    //Allocate the memory to store the image data
    VkMemoryAllocateInfo alloc_info = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL, .allocationSize = mem_req.size };
    if(!BOBi_vk_find_memory_type(context, mem_req.memoryTypeBits, properties, &alloc_info.memoryTypeIndex)) return 0;
    VULKAN_ERROR(vkAllocateMemory(context->log_device, &alloc_info, NULL, &out_image->memory), "Failed to create image memory", BOBi_vk_destroy_image(context, out_image));
    //Bind the memory to the image properties
    VULKAN_ERROR(vkBindImageMemory(context->log_device, out_image->image, out_image->memory, 0), "Failed to bind image memory", BOBi_vk_destroy_image(context, out_image));

    return 1;
}

//Creates a view for an image
uint8_t BOBi_vk_create_image_view(BOB_Context *context, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags, VkImageView *out) {
    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = NULL,
        .image = image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = format,
        .subresourceRange = {.aspectMask = aspect_flags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}
    };
    VULKAN_ERROR(vkCreateImageView(context->log_device, &view_info, NULL, out), "Failed to create image view");

    return 1;
}

void BOBi_vk_destroy_image_view(BOB_Context *context, VkImageView *view) {
    if(*view != VK_NULL_HANDLE) vkDestroyImageView(context->log_device, *view, NULL);
    *view = VK_NULL_HANDLE;
}

//Returns a format that supports our given features from a list of candidates, or throws an error on failure
uint8_t BOBi_vk_find_supported_format(BOB_Context *context, VkFormat *candidates, size_t num_candidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkFormat *out) {
    for(size_t i = 0; i < num_candidates; i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(context->phy_device, candidates[i], &props);

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
uint8_t BOBi_vk_find_depth_format(BOB_Context *context, VkFormat *out) {
    return BOBi_vk_find_supported_format(context, (VkFormat[3]){VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, 3, 
                                 VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, out);
}

//Destroys a BOBi_Vulkan_Buffer
void BOBi_vk_destroy_buffer(VkDevice device, BOBi_Vulkan_Buffer *buf) {
    vkFreeMemory(device, buf->memory, NULL);
    vkDestroyBuffer(device, buf->buffer, NULL);
    buf->memory = VK_NULL_HANDLE;
    buf->buffer = VK_NULL_HANDLE;
}

//Creates buffers in GPU memory
uint8_t BOBi_vk_create_buffer(BOB_Context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BOBi_Vulkan_Buffer *out_buf) {
    //Creates the VKBuffer struct that stores the buffer's properties
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = NULL,
        .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VULKAN_ERROR(vkCreateBuffer(context->log_device, &buf_info, NULL, &out_buf->buffer), "Failed to create a buffer");

    //Getting the memory requirements for this buffer
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(context->log_device, out_buf->buffer, &mem_req);

    //Allocating the memory region to store this buffer
    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = NULL,
        .allocationSize = mem_req.size,
    };

    if(!BOBi_vk_find_memory_type(context, mem_req.memoryTypeBits, properties, &mem_alloc_info.memoryTypeIndex)) return 0;
    VULKAN_ERROR(vkAllocateMemory(context->log_device, &mem_alloc_info, NULL, &out_buf->memory), "Failed to allocate vertex buffer memory",
                          vkDestroyBuffer(context->log_device, out_buf->buffer, NULL));
    //Bind the memory to this buffer properties struct
    VULKAN_ERROR(vkBindBufferMemory(context->log_device, out_buf->buffer, out_buf->memory, 0), "Failed to bind buffer memory", 
                          BOBi_vk_destroy_buffer(context->log_device, out_buf));

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
void BOBi_vk_copy_buffer_to_image(VkCommandBuffer command_buf, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkBufferImageCopy region = {
        .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyBufferToImage(command_buf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

//Copies a certain amount of data from one buffer to another buffer. Assumes that the copy/sources ranges always start at 0
uint8_t BOBi_vk_copy_buffer(BOB_Context *context, VkBuffer src_buf, VkBuffer dst_buf, VkDeviceSize sz) {
    //Begin a local command buffer
    VkCommandBuffer command_copy_buffer;
    BOBi_vk_begin_single_time_commands(context, &command_copy_buffer);

    //Copy the data
    VkBufferCopy copy_region = {0, 0, sz};
    vkCmdCopyBuffer(command_copy_buffer, src_buf, dst_buf, 1, &copy_region);

    //Destroy the local command buffer
    BOBi_vk_end_single_time_commands(context, command_copy_buffer);
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
        .enabledExtensionCount = num_extensions, .ppEnabledExtensionNames = required_extensions, .ppEnabledLayerNames = required_layers
    };
    VULKAN_ERROR(vkCreateInstance(&create_info, NULL, &bob_state.instance), "Failed to create a vulkan instance");

    return 1;
}

uint8_t BOB_create_vulkan_context(size_t atlas_capacity, size_t pixelbuf_capacity,
                           size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t width, size_t height, BOB_Context_Handle *context) {
    return BOBi_create_context(BOB_VULKAN_CONTEXT, atlas_capacity, pixelbuf_capacity, tex_capacity, mat_capacity, font_capacity, width, height, context);
}

//Picks the physical device to use for this application from all available options
uint8_t BOBi_vk_pick_physical_device(BOB_Context *context) {
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
            context->phy_device = devices[i];
            free(devices);
            return 1;
        }
    }
    free(devices);

    printf("Failed to find a suitable GPU\n");
    return 0;
}

//Create a virtual device representation to liase with the physical hardware
uint8_t BOBi_vk_create_logical_device(BOB_Context *context) {
    //Get all of the queue families present on the physical device
    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context->phy_device, &num_queue_families, NULL);
    VkQueueFamilyProperties *family_properties = malloc(sizeof(VkQueueFamilyProperties) * num_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(context->phy_device, &num_queue_families, family_properties);

    //Check if any of the queue families support graphics operations
    context->queue_family = num_queue_families;
    for(size_t i = 0; i < num_queue_families; i++) {
        uint32_t res;
        VULKAN_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(context->phy_device, i, context->surface, &res), "Could not get device surface support");
        if(family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && res) {
            context->queue_family = i;
            break;
        }
    }
    free(family_properties);

    if(context->queue_family == num_queue_families) { //Early exit if no condition is met
        printf("No device queue supports graphics operations\n");
        return 0;
    }

    //Info for creating the graphics queue
    float queue_priority = 0.5f;
    VkDeviceQueueCreateInfo device_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .pNext = NULL,
        .queueFamilyIndex = context->queue_family, .queueCount = 1,
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
    VULKAN_ERROR(vkCreateDevice(context->phy_device, &device_create_info, NULL, &context->log_device), "Failed to create logical device");
    vkGetDeviceQueue(context->log_device, context->queue_family, 0, &context->graphics_queue); //Get the reference to the graphics queue

    return 1;
}

//Creates the vulkan surface used to represent the window
//TODO: FIX
// uint8_t BOBi_vk_create_surface(BOB_Context *context, GLFWwindow *window) {
//     VULKAN_ERROR(glfwCreateWindowSurface(bob_state.instance, window, NULL, &context->surface), "Failed to create window surface");
//     return 1;
// }

//Create the swapchain used to render images to the screen
uint8_t BOBi_vk_create_swapchain(BOB_Context *context, size_t width, size_t height) {
    //Get the surface capabilities
    VkSurfaceCapabilitiesKHR sur_cap;
    VULKAN_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->phy_device, context->surface, &sur_cap), "Could not get surface capabilities");

    //Get swapchain properties
    context->extent = BOBi_vk_choose_swap_extent(&sur_cap, width, height);
    uint32_t min_image_count = BOBi_vk_choose_swap_min_image_count(&sur_cap);

    //Get the available surface formats
    uint32_t surface_format_count = 0;
    VkSurfaceFormatKHR *surface_formats;
    VULKAN_ENUMERATE(vkGetPhysicalDeviceSurfaceFormatsKHR(context->phy_device, context->surface, &surface_format_count, NULL),
                     vkGetPhysicalDeviceSurfaceFormatsKHR(context->phy_device, context->surface, &surface_format_count, surface_formats),
                     surface_formats, surface_format_count * sizeof(VkSurfaceFormatKHR), "Failed to get surface formats\n");
    //Pick the swapchain surface format we will use
    if(!BOBi_vk_choose_swap_surface_format(surface_formats, surface_format_count, &context->format)) {
        printf("Failed to choose the swap chain surface format\n");
        free(surface_formats);
        return 0;
    }

    //Get the available present modes
    uint32_t available_present_modes = 0;
    VkPresentModeKHR *present_modes;
    VULKAN_ENUMERATE(vkGetPhysicalDeviceSurfacePresentModesKHR(context->phy_device, context->surface, &available_present_modes, NULL),
                     vkGetPhysicalDeviceSurfacePresentModesKHR(context->phy_device, context->surface, &available_present_modes, present_modes),
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
        .surface = context->surface, .minImageCount = min_image_count,
        .imageFormat = context->format.format,
        .imageColorSpace = context->format.colorSpace,
        .imageExtent = context->extent,
        .imageArrayLayers = 1, //Specifies num layers each image consists of. Always 1 unless making a stereoscopic 3D app
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, //Specifies what kind of operations we use the images in the swap chain for
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, //Specifies how to handle swap chain images that might be used across multiple queue families
        .preTransform = sur_cap.currentTransform, //Can specify that certain transforms can be applied to images in the swap chain
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, //Specifies if the alpha channel should be used for blending with other windows
        .presentMode = chosen_mode,
        .clipped = 1, .oldSwapchain = VK_NULL_HANDLE,
    };
    VULKAN_ERROR(vkCreateSwapchainKHR(context->log_device, &swapchain_create_info, NULL, &context->swapchain), "Failed to create swapchain");

    //Get a reference to the swapchain images
    VULKAN_ENUMERATE(vkGetSwapchainImagesKHR(context->log_device, context->swapchain, &context->num_images, NULL),
                     vkGetSwapchainImagesKHR(context->log_device, context->swapchain, &context->num_images, context->images),
                     context->images, context->num_images * sizeof(VkImage), "Failed to get the swapchain images");


    return 1;
}

//Creates views for the swapchain images
uint8_t BOBi_vk_create_image_views(BOB_Context *context) {
    context->views = malloc(sizeof(VkImageView) * context->num_images);

    for(size_t i = 0; i < context->num_images; i++) {
        if(!BOBi_vk_create_image_view(context, context->images[i], context->format.format, VK_IMAGE_ASPECT_COLOR_BIT, &context->views[i])) return 0;
    }

    return 1;
}

//Creates a layout for the descriptor set for the data we will be sending to our shader
uint8_t BOBi_vk_create_descriptor_set_layout(BOB_Context *context, BOB_Renderer *r) {
    //Setting the binding data
    VkDescriptorSetLayoutBinding bindings[2] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}
    };

    //Create the descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = NULL,
        .bindingCount = 2, .pBindings = bindings
    };
    VULKAN_ERROR(vkCreateDescriptorSetLayout(context->log_device, &layout_info, NULL, &r->descriptor_set_layout), "Failed to create descriptor set layout");

    return 1;
}

//Creates our graphics pipeline
uint8_t BOBi_vk_create_graphics_pipeline(BOB_Context *context, BOB_Renderer *r) {
    //TODO: Spin shader creation out into its own function
    //Get the shader code
    uint8_t *shader_code;
    int sz = BOBi_read_to_end("../shaders/shader.spv", &shader_code, 0);
    if(sz < 0) {
        printf("Failed to read file\n");
        return 0;
    }

    //Create the shader module to hold the shader
    VkShaderModule shader_module;
    if(!BOBi_vk_create_shader_module(shader_code, sz, &shader_module, context->log_device)) {
        free(shader_code);
        return 0;
    }

    //Telling the pipleine what shader stages we are using
    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = NULL,
                                                            .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = shader_module, .pName = "vertMain"};

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = NULL,
                                                            .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = shader_module, .pName = "fragMain"};

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    //Can tell the pipeline what stages we want to be able to change at runtime without having to recreate the whole program
    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .pNext = NULL,
                                                    .dynamicStateCount = 2, .pDynamicStates = dynamic_states};

    //Get the binding and attribute descriptions
    VkVertexInputBindingDescription binding_desc = BOBi_vk_get_binding_desc();
    VkVertexInputAttributeDescription attrib_descs[3];
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

    VkViewport viewport = {0.0f, 0.0f, context->extent.width, context->extent.height, 0.0f, 1.0f}; //Viewport rectangle
    VkRect2D scissor = {(VkOffset2D){0, 0}, context->extent}; //Scissor rectangle
    VkPipelineViewportStateCreateInfo viewport_state = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .pNext = NULL, .viewportCount = 1, .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo rasteriser = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .pNext = NULL,
        .depthClampEnable = VK_FALSE, //If set to true, fragments beyond near and far planes are clamped to them instead of discarded
        .rasterizerDiscardEnable = VK_FALSE, //If set to true, then geometry never passes through rasteriser stage. Disables output to framebuffer
        .polygonMode = VK_POLYGON_MODE_FILL, //Determines how fragments are generated for geometry. Can also be drawn as lines or points
        .cullMode = VK_CULL_MODE_BACK_BIT, //Determines what kind of face culling to use
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, //Determines the vertex order for the faces to be considered front facing and can be clockwise or counter-clockwise
        .depthBiasEnable = VK_FALSE, //Rasteriser can alter the depth values by adding a constant value or biasing them based on a fragments slope. Not necessary
        .lineWidth = 1.0}; //Determines the thickness of lines in terms of fragments

    //Configure Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .pNext = NULL,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT, .sampleShadingEnable = VK_FALSE
    };

    //Configuring Colour Blending
    VkPipelineColorBlendAttachmentState colour_blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colour_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .pNext = NULL,
        .logicOpEnable = VK_FALSE, .logicOp = VK_LOGIC_OP_COPY, .attachmentCount = 1, .pAttachments = &colour_blend_attachment
    };

    //Creating the pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = NULL,
        .setLayoutCount = 1, .pSetLayouts = &r->descriptor_set_layout, .pushConstantRangeCount = 0
    };
    VULKAN_ERROR(vkCreatePipelineLayout(context->log_device, &pipeline_layout_info, NULL, &r->layout),
                          "Failed to create pipeline layout", free(shader_code); vkDestroyShaderModule(context->log_device, shader_module, NULL));

    //Specify the depth stencil data
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .pNext = NULL,
        .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = VK_FALSE
    };

    //Specify the formats of the attachments used during rendering
    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = NULL,
        .colorAttachmentCount = 1, .pColorAttachmentFormats = &context->format.format,
    };
    if(!BOBi_vk_find_depth_format(context, &pipeline_rendering_create_info.depthAttachmentFormat)) return 0;

    //Creating the graphics pipeline
    VkGraphicsPipelineCreateInfo graphics_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &pipeline_rendering_create_info,
        .stageCount = 2, .pStages = shader_stages, .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &input_assembly, .pViewportState = &viewport_state,
        .pRasterizationState = &rasteriser, .pMultisampleState = &multisampling,
        .pColorBlendState = &colour_blending, .pDynamicState = &dynamic_state,
        .layout = r->layout, .renderPass = NULL, .pDepthStencilState = &depth_stencil
    };

    VULKAN_ERROR(vkCreateGraphicsPipelines(context->log_device, VK_NULL_HANDLE, 1, &graphics_create_info, NULL, &r->pipeline),
                          "Failed to create graphics pipeline", free(shader_code); vkDestroyShaderModule(context->log_device, shader_module, NULL););

    free(shader_code);
    vkDestroyShaderModule(context->log_device, shader_module, NULL);

    return 1;
}

//Creates a command pool
uint8_t BOBi_vk_create_command_pool(BOB_Context *context) {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = NULL,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = context->queue_family
    };
    VULKAN_ERROR(vkCreateCommandPool(context->log_device, &pool_info, NULL, &context->command_pool), "Failed to create command pool");
    return 1;
}

//Creates the resources used to do depth culling
uint8_t BOBi_vk_create_depth_resources(BOB_Context *context) {
    VkFormat depth_format;
    if(!BOBi_vk_find_depth_format(context, &depth_format)) return 0;

    BOBi_vk_create_image(context, context->extent.width, context->extent.height, depth_format, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &context->depth);
    BOBi_vk_create_image_view(context, context->depth.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT, &context->depth.view);

    return 1;
}

//Creates the descriptor pools that hold the information on the data we send to the GPU
uint8_t BOBi_vk_create_descriptor_pool(BOB_Context *context, BOB_Renderer *r) {
    VkDescriptorPoolSize pool_size[2] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = MAX_FRAMES_IN_FLIGHT},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MAX_FRAMES_IN_FLIGHT}
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, 
        .maxSets = MAX_FRAMES_IN_FLIGHT, .poolSizeCount = 2,
        .pPoolSizes = pool_size
    };

    VULKAN_ERROR(vkCreateDescriptorPool(context->log_device, &pool_info, NULL, &r->descriptor_pool), "Failed to create descriptor pool");
    return 1;
}

//Creates descriptor sets that hold the actual data on what is sent to the shader
uint8_t BOBi_vk_create_descriptor_set(BOB_Context *context, BOB_Renderer *r, BOBi_Vulkan_Frame_Resources *frame) {
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        layouts[i] = r->descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = NULL,
        .descriptorPool = r->descriptor_pool,
        .descriptorSetCount = 1, .pSetLayouts = layouts
    };

    VULKAN_ERROR(vkAllocateDescriptorSets(context->log_device, &alloc_info, &r->descriptor_set[context->frame_index]), "Failed to allocate descriptor sets");

    VkDescriptorBufferInfo buf_info = { .buffer = frame->uniform_buffer.buffer, .offset = 0, .range = sizeof(BOBi_Vulkan_UniformBufferObject) };
    //TODO: FIX
    VkDescriptorImageInfo img_info;
    // VkDescriptorImageInfo img_info = {.sampler = state->tex_image_sampler, .imageView = state->tex.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet descriptor_write[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL,
         .dstSet = r->descriptor_set[context->frame_index], .dstBinding = 0, .dstArrayElement = 0,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         .pBufferInfo = &buf_info},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = NULL,
         .dstSet = r->descriptor_set[context->frame_index], .dstBinding = 1, .dstArrayElement = 0,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .pImageInfo = &img_info}
    };

    vkUpdateDescriptorSets(context->log_device, 2, descriptor_write, 0, NULL);

    return 1;
}

//Creates a vertex buffer that is sent to the GPU
uint8_t BOBi_vk_create_vertex_buffer(BOB_Context *context, BOB_Renderer *r, size_t num_vertices) {
    //Create the staging buffer
    VkDeviceSize buf_sz = sizeof(BOB_Render_Vertex) * num_vertices;
    BOBi_Vulkan_Buffer staging_buf;
    if(!BOBi_vk_create_buffer(context, buf_sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_buf)) return 0;

    //Map the staging buffer to CPU memory and copy the vertex data into it
    //TODO: Move this into the render function
    // VULKAN_ERROR(!BOBi_vk_stream_to_buffer(context->log_device, vertices, buf_sz, &staging_buf), "Failed to stream data into a Vulkan Buffer");

    //Create the destination VBO and map the data from the staging buffer into it
    VULKAN_ERROR(!BOBi_vk_create_buffer(context, buf_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                &r->vertex_buffer), "Failed to create vertex buffer", BOBi_vk_destroy_buffer(context->log_device, &staging_buf));
    BOBi_vk_copy_buffer(context, staging_buf.buffer, r->vertex_buffer.buffer, buf_sz);

    //Destroy the staging buffer
    BOBi_vk_destroy_buffer(context->log_device, &staging_buf);

    return 1;
}

//Creates an index buffer to be sent to the GPU
uint8_t BOBi_vk_create_index_buffer(BOB_Context *context, BOB_Renderer *r, size_t num_indicies) {
    //Create the staging buffer
    VkDeviceSize buf_sz = sizeof(uint32_t) * num_indicies;
    BOBi_Vulkan_Buffer staging_buf;
    if(!BOBi_vk_create_buffer(context, buf_sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_buf)) return 0;

    //Map the staging buffer to CPU memory and copy the index data into it
    //TODO: Move this into the render function
    // VULKAN_ERROR(!BOBi_vk_stream_to_buffer(state->device.log_device, indices, buf_sz, &staging_buf), "Failed to stream data into a Vulkan Buffer");

    //Create the actual destination buffer and copy the staging buffer data into it
    VULKAN_ERROR(!BOBi_vk_create_buffer(context, buf_sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  &r->index_buffer), "Failed to create index buffer", BOBi_vk_destroy_buffer(context->log_device, &staging_buf));
    BOBi_vk_copy_buffer(context, staging_buf.buffer, r->index_buffer.buffer, buf_sz);

    //Free the staging buffer
    BOBi_vk_destroy_buffer(context->log_device, &staging_buf);

    return 1;
}

//TODO: FIX
uint8_t BOBi_vk_create_texture(BOB_Context *context, size_t width, size_t height, uint8_t *data, BOB_Format format, BOBi_Vulkan_Image *tex) {
    //Create the staging buffer
    BOBi_Vulkan_Buffer staging_buf;
    BOBi_vk_create_buffer(context, width * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buf);

    //Map the staging buffer memory into CPU memory and copy the pixel data into it
    VULKAN_ERROR(!BOBi_vk_stream_to_buffer(context->log_device, data, width * height, &staging_buf), "Failed to stream data into a Vulkan Buffer");

    //Create the image
    if(!BOBi_vk_create_image(context, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex)) return 0;

    VkCommandBuffer command_buf;
    VULKAN_ERROR(!BOBi_vk_begin_single_time_commands(context, &command_buf), "Failed to begin command buffer",
        BOBi_vk_destroy_buffer(context->log_device, &staging_buf);
    );
    VULKAN_ERROR(!BOBi_vk_transition_tex_layout(command_buf, tex->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), "Failed to transition layout",
        BOBi_vk_destroy_buffer(context->log_device, &staging_buf);
    );
    BOBi_vk_copy_buffer_to_image(command_buf, staging_buf.buffer, tex->image, width, height);
    VULKAN_ERROR(!BOBi_vk_transition_tex_layout(command_buf, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        "Failed to transition layout", BOBi_vk_destroy_buffer(context->log_device, &staging_buf);
    );
    VULKAN_ERROR(!BOBi_vk_end_single_time_commands(context, command_buf), "Failed to end command buffer",
        BOBi_vk_destroy_buffer(context->log_device, &staging_buf);
    );

    BOBi_vk_destroy_buffer(context->log_device, &staging_buf);

    //Create view for the image
    BOBi_vk_create_image_view(context, tex->image, VK_FORMAT_R8G8B8A8_SRGB,  VK_IMAGE_ASPECT_COLOR_BIT, &tex->view);
    return 1;
}

//Creates a texture sampler for the one texture we are using to be sent to the GPU
uint8_t create_texture_sampler(BOB_Context *context, VkSampler *sampler) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(context->phy_device, &properties);
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
    VULKAN_ERROR(vkCreateSampler(context->log_device, &sampler_info, NULL, sampler), "Failed to create texture sampler");
    return 1;
}

//Creates the general command buffers used to generate draw calls
uint8_t BOBi_vk_create_command_buffer(BOB_Context *context, BOBi_Vulkan_Frame_Resources *frame) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = NULL,
        .commandPool = context->command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1
    };
        VULKAN_ERROR(vkAllocateCommandBuffers(context->log_device, &alloc_info, &frame->command_buffer), "Failed to allocate command buffers");
    return 1;
}

//Creates the semaphores and fences required to synchronise operations
uint8_t BOBi_vk_create_sync_objects(BOBi_Vulkan_Frame_Resources *frame, VkDevice device) {
    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = NULL, .flags = 0 };

    VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = NULL, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        VULKAN_ERROR(vkCreateSemaphore(device, &sem_info, NULL, &frame->render_finished_semaphore), "Failed to create a semaphore");
        VULKAN_ERROR(vkCreateSemaphore(device, &sem_info, NULL, &frame->present_complete_semaphore), "Failed to create a semaphore");
        VULKAN_ERROR(vkCreateFence(device, &fence_info, NULL, &frame->draw_fence), "Failed to create a fence");

    return 1;
}

uint8_t BOBi_vk_create_uniform_buffer(BOB_Context *context, BOBi_Vulkan_Frame_Resources *frame) {
        VkDeviceSize buffer_sz = sizeof(BOBi_Vulkan_UniformBufferObject);
        VULKAN_ERROR(!BOBi_vk_create_buffer(context, buffer_sz, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &frame->uniform_buffer), "Failed to create uniform buffer");

        VULKAN_ERROR(vkMapMemory(context->log_device, frame->uniform_buffer.memory, 0, buffer_sz, 0, &frame->uniform_buffer_mapped),
                     "Failed to map uniform buffer memory", BOBi_vk_destroy_buffer(context->log_device, &frame->uniform_buffer));

    return 1;
}

void BOBi_vk_destroy_frame_resources(BOB_Context *context, BOB_Renderer *r) {
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkFreeCommandBuffers(context->log_device, context->command_pool, 1, &context->resources[i].command_buffer);
        vkUnmapMemory(context->log_device, context->resources[i].uniform_buffer.memory);
        BOBi_vk_destroy_buffer(context->log_device, &context->resources[i].uniform_buffer);
        // vkFreeDescriptorSets(context->log_device, r->descriptor_pool, 1, &r->descriptor_set[i]);
        vkDestroySemaphore(context->log_device, context->resources[i].render_finished_semaphore, NULL);
        vkDestroySemaphore(context->log_device, context->resources[i].present_complete_semaphore, NULL);
        vkDestroyFence(context->log_device, context->resources[i].draw_fence, NULL);
    }
}

uint8_t BOBi_vk_create_frame_resources(BOB_Context *context, BOB_Renderer *r) {
    uint8_t succeeded = 1;
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if(!(BOBi_vk_create_sync_objects(&context->resources[i], context->log_device) &&
             BOBi_vk_create_uniform_buffer(context, &context->resources[i]) &&
             // BOBi_vk_create_descriptor_set(context, r, &context->resources[i]) &&
             BOBi_vk_create_command_buffer(context, &context->resources[i]))) {
            succeeded = 0;
            break;
        }
    }

    if(!succeeded) BOBi_vk_destroy_frame_resources(context, r);

    return succeeded;
}

//Initialises vulkan
uint8_t BOBi_vk_init_vulkan_context(BOB_Context *context, size_t width, size_t height) {
    return BOBi_vk_pick_physical_device(context) && BOBi_vk_create_logical_device(context)
    && BOBi_vk_create_swapchain(context, width, height) && BOBi_vk_create_image_views(context)
    && BOBi_vk_create_command_pool(context) && BOBi_vk_create_depth_resources(context);
}

// ============================================ DESTRUCTION FUNCTIONS ========================================

//Destroys existing memory used by the swapchain
void BOBi_vk_cleanup_swapchain(BOB_Context *context) {
    for(size_t i = 0; i < context->num_images; i++) {
        vkDestroyImageView(context->log_device, context->views[i], NULL);
    }
    free(context->views);
    free(context->images);
    vkDestroySwapchainKHR(context->log_device, context->swapchain, NULL);
}

//Rebuilds the swapchain on framebuffer resize
uint8_t BOBi_vk_recreate_swapchain(BOB_Context *context, size_t width, size_t height) {
    VULKAN_ERROR(vkDeviceWaitIdle(context->log_device), "Failed to wait for signal");
    BOBi_vk_cleanup_swapchain(context);
    vkDestroyImageView(context->log_device, context->depth.view, NULL);
    vkFreeMemory(context->log_device, context->depth.memory, NULL);
    vkDestroyImage(context->log_device, context->depth.image, NULL);
    return BOBi_vk_create_swapchain(context, width, height) && BOBi_vk_create_image_views(context) && BOBi_vk_create_depth_resources(context);
}

// ===================================== DRAWING FUNCTIONS =========================================

//Records draw calls into the general command buffer
uint8_t BOBi_vk_record_command_buffer(BOB_Context *context, BOB_Renderer *r, uint32_t image_index) {
    //Begin writing to the command buffer
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = NULL
    };
    VkCommandBuffer buffer = context->resources[context->frame_index].command_buffer; //Getting a reference to the command buffer so that don't have to write out full code every time
    VULKAN_ERROR(vkBeginCommandBuffer(buffer, &begin_info), "Failed to begin command buffer operations");

    //Transition the image to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    BOBi_vk_transition_image_layout(context->images[image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, (VkAccessFlags2){},
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, buffer);

    //Transition depth image to depth attachment optimal layout
    BOBi_vk_transition_image_layout(context->depth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                            VK_IMAGE_ASPECT_DEPTH_BIT, buffer);

    VkClearValue clear_color = {.color = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}}}; //Set the colour the screen gets cleared to
    VkClearValue clear_depth = {.depthStencil = {.depth = 1.0f, .stencil = 0}}; //Set the depth the screen is cleared at

    VkRenderingAttachmentInfo attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = NULL,
        .imageView = context->views[image_index], //Specifies which view to render to
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, //Specifies what to do with the image during rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, //Specifies what to do with the image after rendering
        .clearValue = clear_color
    };
    VkRenderingAttachmentInfo depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = NULL,
        .imageView = context->depth.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, //Specifies what to do with the image during rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //Specifies what to do with the image after rendering
        .clearValue = clear_depth
    };

    //Set the rendering info
    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = NULL,
        .renderArea = {.offset = {0, 0}, .extent = context->extent},
        .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &attachment_info,
        .pDepthAttachment = &depth_attachment_info
    };

    //Begin rendering
    vkCmdBeginRendering(buffer, &rendering_info);

    //Bind the graphics pipeline
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);
    //Bind the vertex buffer to the command buffer
    vkCmdBindVertexBuffers(buffer, 0, 1, &r->vertex_buffer.buffer, (VkDeviceSize[]){0});
    //Bind the index buffer to the command buffer
    vkCmdBindIndexBuffer(buffer, r->index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    //Need to set the viewport and scissor since we're doing it dynamically
    VkViewport viewport = {0.0f, 0.0f, context->extent.width, context->extent.height, 0.0f, 1.0f};
    vkCmdSetViewport(buffer, 0, 1, &viewport);
    VkRect2D scissor = {(VkOffset2D){0, 0}, context->extent};
    vkCmdSetScissor(buffer, 0, 1, &scissor);

    //Bind correct descriptor set for each frame to the descriptors in the shader
    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->layout, 0, 1, &r->descriptor_set[context->frame_index], 0, NULL);
    //Draw to the screen
    vkCmdDrawIndexed(buffer, num_indices, 1, 0, 0, 0);

    vkCmdEndRendering(buffer); //End rendering

    //After rendering, transition the swapchain image to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR so it can be presented to the screen
    BOBi_vk_transition_image_layout(context->images[image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, (VkAccessFlags2){}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, buffer);

    VULKAN_ERROR(vkEndCommandBuffer(buffer), "Failed to end command buffer");
    return 1;
}

//Updates the uniform buffer with new texture position
void BOBi_vk_update_uniform_buffer(BOB_Context *context, BOB_Renderer *r) {
}

//Draws every frame
uint8_t draw_frame(BOB_Context *context, BOB_Renderer *r) {
    //Wait until operations from previous frame have completed
    VULKAN_ERROR(vkWaitForFences(context->log_device, 1, &context->resources[context->frame_index].draw_fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence");
    vkResetFences(context->log_device, 1, &context->resources[context->frame_index].draw_fence);

    //Get the next swapchain image
    uint32_t image_index;
    VkResult res = vkAcquireNextImageKHR(context->log_device, context->swapchain, UINT64_MAX,
                                         context->resources[context->frame_index].present_complete_semaphore, VK_NULL_HANDLE, &image_index);
    //If the swapchain data is invalid, remake it
    if(res == VK_ERROR_OUT_OF_DATE_KHR) {
        BOBi_vk_recreate_swapchain(context, 0, 0); //TODO: Fix
        return 1;
    }
    //If we haven't been able to get a valid image, throw an error
    else if(res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        assert(res == VK_TIMEOUT || res == VK_NOT_READY);
        printf("Failed to acquire swap chain image\n");
        return 0;
    }

    //CLear the command buffer and record the draw commands for the current frame
    vkResetCommandBuffer(context->resources[context->frame_index].command_buffer, 0);
    if(!BOBi_vk_record_command_buffer(context, r, image_index)) {
        printf("Failed to record command buffer\n");
        return 0;
    }

    BOBi_vk_update_uniform_buffer(context, r); //Update the uniforms sent to the shader

    //Submit render commands to the graphics queue
    VkPipelineStageFlagBits wait_dest_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo sub_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .pNext = NULL,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &context->resources[context->frame_index].present_complete_semaphore, .pWaitDstStageMask = &wait_dest_stage_mask,
        .commandBufferCount = 1, .pCommandBuffers = &context->resources[context->frame_index].command_buffer,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &context->resources[context->frame_index].render_finished_semaphore
    };
    VULKAN_ERROR(vkQueueSubmit(context->graphics_queue, 1, &sub_info, context->resources[context->frame_index].draw_fence), "Failed to Submit render data to the queue");

    //Get the present status
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .pNext = NULL,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &context->resources[context->frame_index].render_finished_semaphore,
        .swapchainCount = 1, .pSwapchains = &context->swapchain, .pImageIndices = &image_index
    };
    res = vkQueuePresentKHR(context->graphics_queue, &present_info);

    //If its invalid, recreate the swapchain
    if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || context->framebuffer_resized) {
        context->framebuffer_resized = 0;
        BOBi_vk_recreate_swapchain(context, 0, 0); //TODO: Fix
        return 1;
    }
    VULKAN_ERROR(res, "Failed to acquire swap chain image");

    context->frame_index = (context->frame_index + 1) % MAX_FRAMES_IN_FLIGHT; //Update the index of the frame we are currently updating

    return 1;
}

#endif

//================================================= INTERNAL HELPER FUNCTIONS ===================================================

uint8_t BOBi_create_context(BOB_Context_Type type, size_t atlas_capacity, size_t pixelbuf_capacity,
                           size_t tex_capacity, size_t mat_capacity, size_t font_capacity, size_t width, size_t height, BOB_Context_Handle *context) {
    if(bob_state.context_count >= bob_state.context_capcity) {
        printf("ERROR: Exceeded context capacity\n");
        return 0;
    }

    uint32_t index;
    if(bob_state.next_context_slot == UINT32_MAX) {
        index = bob_state.context_count;
    }
    else {
        index = bob_state.next_context_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (bob_state.next_context_slot = index + 1; bob_state.next_context_slot < bob_state.context_count; bob_state.next_context_slot++) {
            //Use the allocation status of the context's memory region as an initialisation tell
            //Relies on setting pointer to NULL on context destruction and zeroing memory on creating the BOB instance
            if (bob_state.contexts[bob_state.next_context_slot].context_memory.memory == NULL)
                break;
        }

        if (bob_state.next_context_slot >= bob_state.context_count)
            bob_state.next_context_slot = UINT32_MAX;
    }

    BOB_Context *intrn_context = &bob_state.contexts[index];

    #ifdef BOB_INCLUDE_VULKAN
    if(type == BOB_VULKAN_CONTEXT) BOBi_vk_init_vulkan_context(intrn_context, width, height);
    #endif //BOB_INCLUDE_VULKAN

    //Calculating the size of the memory regions each buffer will end up using
    size_t atlas_sz = atlas_capacity * sizeof(BOB_Atlas);
    size_t pixelbuf_sz = pixelbuf_capacity * sizeof(BOB_PixelBuffer);
    size_t tex_sz = tex_capacity * sizeof(BOB_Texture);
    size_t mat_sz = mat_capacity * sizeof(BOB_Material);
    size_t font_sz = font_capacity * sizeof(BOB_Font);

    //Figuring out how much aligned memory we will need
    char *p = (char *)0;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOB_Atlas));
    p += atlas_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOB_PixelBuffer));
    p += pixelbuf_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOB_Texture));
    p += tex_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOB_Material));
    p += mat_sz;
    p = (char *)BOBi_align_up((uintptr_t)p, alignof(BOB_Font));
    p += font_sz;

    size_t total = (size_t)p;

    //Allocating the memory used for the object buffers and checking the allocation
    if(!BOB_init_arena(&intrn_context->context_memory, total)) return 0;
    memset(intrn_context->context_memory.memory, 0, total);

    //Assigning the start pointers from the general memory buffer
    intrn_context->atlas_table = BOB_arena_alloc(&intrn_context->context_memory, atlas_sz, alignof(BOB_Atlas));
    intrn_context->pixelbuffer_table = BOB_arena_alloc(&intrn_context->context_memory, pixelbuf_sz, alignof(BOB_PixelBuffer));
    intrn_context->texture_table = BOB_arena_alloc(&intrn_context->context_memory, tex_sz, alignof(BOB_Texture));
    intrn_context->material_table = BOB_arena_alloc(&intrn_context->context_memory, mat_sz, alignof(BOB_Material));
    intrn_context->font_table = BOB_arena_alloc(&intrn_context->context_memory, font_sz, alignof(BOB_Font));

    //Assiging the capacity values
    intrn_context->atlas_capacity = atlas_capacity;
    intrn_context->pixelbuffer_capacity = pixelbuf_capacity;
    intrn_context->texture_capacity = tex_capacity;
    intrn_context->material_capacity = mat_capacity;
    intrn_context->font_capacity = font_capacity;

    //Setting the sizes to be 0
    intrn_context->num_atlases = 0;
    intrn_context->num_pixelbuffers = 0;
    intrn_context->num_textures = 0;
    intrn_context->num_materials = 0;
    intrn_context->num_fonts = 0;

    //Setting the next free slot to point to the first one
    intrn_context->next_atlas_slot = UINT32_MAX;
    intrn_context->next_pixelbuf_slot = UINT32_MAX;
    intrn_context->next_tex_slot = UINT32_MAX;
    intrn_context->next_mat_slot = UINT32_MAX;
    intrn_context->next_font_slot = UINT32_MAX;

    //Create the default texture used
    intrn_context->type = type;
    if(!BOB_create_texture(index, 1, 1, (uint8_t[4]){255, 255, 255, 255}, BOB_RGBA, &intrn_context->default_tex)) return 0;

    *context = index;
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

size_t BOBi_quicksort_median_of_three(BOB_Renderer *r, size_t lo, size_t hi)
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
size_t BOBi_quicksort_partition(BOB_Renderer *r, size_t subarr_start, size_t subarr_end) {
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

void BOBi_quicksort_draw_calls(BOB_Renderer *r, size_t subarr_start, size_t subarr_end) {
    if(subarr_start < subarr_end) {
        size_t pos_pivot = BOBi_quicksort_partition(r, subarr_start, subarr_end);
        BOBi_quicksort_draw_calls(r, subarr_start, pos_pivot);
        BOBi_quicksort_draw_calls(r, pos_pivot+1, subarr_end);
    }
}

//Helper function to clip a quad
uint8_t BOBi_clip_quad(BOB_Renderer *r, BOB_Quad *quad) {
    if(r->stack->size == 0) return 1; //No need to clip if no clip rects
    BOBi_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
    if(clip.empty) return 0;

    if(clip.clip_horz) {
        if(quad->x < clip.left) {
            quad->w -= clip.left - quad->x;
            quad->x = clip.left;
        }
        if(quad->x + quad->w > clip.right) quad->w = clip.right - clip.left;
    }
    if(clip.clip_vert) {
        if(quad->y < clip.top) {
            quad->h -= clip.top - quad->y;
            quad->y = clip.top;
        }
        if(quad->y + quad->h > clip.bottom) quad->h = clip.bottom - clip.top;
    }

    if(!quad->h || !quad->w) return 0; //If the clipped region is empty, return;

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

uint8_t BOBi_clip_line(BOB_Renderer *r, BOB_Vector2 *start, BOB_Vector2* end) {
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

#define BOBi_MAX_POLY_SIZE 256

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
    BOB_Vector2 new_points[BOBi_MAX_POLY_SIZE]; //Allow up to 256 vertex polygons

    //Iterate over all points
    for(size_t i = 0; i < poly_size; i++) {
        //Getting the point that forms the end of the current line
        size_t j = (i + 1) % poly_size;
        BOB_Vector2 start = poly_points[i];
        BOB_Vector2 end = poly_points[j];

        uint8_t start_inside = BOBi_inside(start, edge, value);
        uint8_t end_inside = BOBi_inside(end, edge, value);

        if(start_inside && end_inside) {
            if(new_poly_size >= BOBi_MAX_POLY_SIZE) {
                printf("Exceeded new polygon point capacity\n");
                break;
            }
            //Only second point is added
            new_points[new_poly_size++] = end;
        }
        else if(!start_inside && end_inside) {
            if(new_poly_size+1 >= BOBi_MAX_POLY_SIZE) {
                printf("Exceeded new polygon point capacity\n");
                break;
            }
            //Point of intersection with edge and second point is added
            new_points[new_poly_size++] = BOBi_get_intersection(start, end, edge, value);
            new_points[new_poly_size++] = end;
        }
        //When only second point is outside
        else if(start_inside && !end_inside) {
            if(new_poly_size >= BOBi_MAX_POLY_SIZE) {
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

size_t BOBi_clip_polygon(BOB_Renderer *r, BOB_Vector2 *poly_points, size_t poly_size) {
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

    BOBi_PartitionVertex vertices[BOBi_MAX_POLY_SIZE];
    uint32_t processed[BOBi_MAX_POLY_SIZE];
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
    processed_size = write;

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

void BOBi_flush_draw_calls(BOB_Renderer *r) {
    BOB_renderer_end(r);
    BOB_renderer_begin(r);
}

void BOBi_check_draw_capacity(BOB_Renderer *r, uint32_t num_vertices, uint32_t num_indices) {
    if(num_indices + r->batch.num_indices >= BOB_MAX_INDEX_CAPACITY ||
       num_vertices + r->batch.num_vertices >= BOB_MAX_VERTEX_CAPACITY ||
       r->batch.num_draw_calls + 1 >= BOB_MAX_DRAW_CALL_CAPACITY) {
        BOBi_flush_draw_calls(r);
        r->batch.num_draw_calls = 0;
        r->batch.num_indices = 0;
        r->batch.num_vertices = 0;
    }
}

void BOBi_create_draw_call(BOB_Renderer *r, BOB_Vector3 *vertices, size_t vertex_count, BOB_Vector2 *uv, size_t index_count, BOB_Vector4 colour, BOB_Texture_Handle tex, BOB_Material_Handle mat, uint8_t channel, BOBi_Draw_Type type) {
    BOBi_check_draw_capacity(r, vertex_count, index_count);

    BOBi_Draw_Call *dc = (BOBi_Draw_Call *)BOB_arena_alloc(&r->batch.draw_call_arena, sizeof(BOBi_Draw_Call), alignof(BOBi_Draw_Call));
    BOB_Render_Vertex *alloc_vertices = (BOB_Render_Vertex *)BOB_arena_alloc(&r->batch.vertex_arena, sizeof(BOB_Render_Vertex) * vertex_count, alignof(BOB_Render_Vertex));
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
        dc->vertices[i] = (BOB_Render_Vertex){colour, vertices[i], (uv == NULL) ? (BOB_Vector2){0} : uv[i], channel};
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

void BOBi_texture_free(BOB_Context *context, uint32_t index) {
    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_delete_texture(&context->texture_table[index].opengl.texture);
    #endif //BOB_INCLUDE_GLAD
    context->texture_table[index] = (BOB_Texture){0}; //Clear the data
}
void BOBi_pixelbuffer_free(BOB_Context *context, uint32_t index) {
    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_delete_buffer(&context->pixelbuffer_table[index].pbo);
    #endif //BOB_INCLUDE_GLAD
    BOB_texture_free(&context->pixelbuffer_table[index].pixel_tex);
    context->pixelbuffer_table[index] = (BOB_PixelBuffer){0}; //Clear the data
}
void BOBi_material_free(BOB_Context *context, uint32_t index) {
    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_delete_program(context->material_table[index].shader);
    #endif //BOB_INCLUDE_GLAD
    free(context->material_table[index].uniforms);
    context->material_table[index] = (BOB_Material){0}; //Clear the data
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

void BOBi_font_free(BOB_Context *context, uint32_t index) {
    if(context->font_table[index].glyphs) free(context->font_table[index].glyphs);
    if(context->font_table[index].kernings) free(context->font_table[index].kernings);
    if(context->font_table[index].glyph_map) {
        BOBi_hashmap_free(context->font_table[index].glyph_map);
        free(context->font_table[index].glyph_map);
    }
    if(context->font_table[index].kerning_map) {
        BOBi_hashmap_free(context->font_table[index].kerning_map);
        free(context->font_table[index].kerning_map);
    }
    context->font_table[index] = (BOB_Font){0}; //Clear the data
}

void BOBi_destroy_context(uint32_t index) {
    BOB_Context *context = &bob_state.contexts[index];

    //Free all of the object memory
    for(size_t i = 0; i < context->texture_capacity; i++) {
        if(context->texture_table[i].init)
            BOBi_texture_free(context, i);
    }
    for(size_t i = 0; i < context->material_capacity; i++) {
        if(context->material_table[i].init)
            BOBi_material_free(context, i);
    }
    for(size_t i = 0; i < context->pixelbuffer_capacity; i++) {
        if(context->pixelbuffer_table[i].init)
            BOBi_pixelbuffer_free(context, i);
    }
    for(size_t i = 0; i < context->font_capacity; i++) {
        if(context->font_table[i].init)
            BOBi_font_free(context, i);
    }
    BOB_destroy_arena(&context->context_memory);

    *context = (BOB_Context){0}; //Clear all of the data
}

void BOB_destroy_context(BOB_Context_Handle *context) {
    if(*(context) & BOBi_MSB) return; //DO not work with already invalid handles
    BOBi_destroy_context(*context);
    if(*(context) < bob_state.next_context_slot) bob_state.next_context_slot = *(context);
    *(context) |= *(context) & (~BOBi_MSB); //Set the MSB to indicate this is an invalid handle
}

// ==================================== MISCELLANEOUS FUNCTIONS ========================================

//Calculates the projection matrix
void BOB_ortho(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 *dest) {
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

void BOB_clear_colour(BOB_Context_Handle handle, BOB_Vector4 colour) {
    BOB_Context *context;
    BOBi_get_context_from_handle(handle, &context);
    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_clear_color(colour);
    #endif // BOB_INCLUDE_GLAD
}

//Converts an angle in degrees to radians
float BOB_degrees_to_radians(float angle) {
    return angle * (M_PI / 180);
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

// ============================================= BOB STATE MANAGEMENT ============================================================

#ifdef BOB_INCLUDE_GLAD
#ifdef BOB_INCLUDE_VULKAN
uint8_t BOB_init(GLADloadproc proc, const char **required_extensions, size_t num_extensions, size_t num_contexts, size_t num_renderers) {
    //Loading GLAD
    if(!gladLoadGLLoader(proc)) {
        printf("Failed to initialise GLAD");
        return 0;
    }

    //Loading Vulkan:
    if(!BOBi_vk_init_vulkan(required_extensions, num_extensions)) return 0;
#else
uint8_t BOB_init(GLADloadproc proc, size_t num_contexts, size_t num_renderers) {
    //Loading GLAD
    if(!gladLoadGLLoader(proc)) {
        printf("Failed to initialise GLAD");
        return 0;
    }
#endif //BOB_INCLUDE_VULKAN
#else
uint8_t BOB_init(const char **required_extensions, size_t num_extensions, size_t num_contexts, size_t num_renderers) {
    //Loading Vulkan:
    if(!BOBi_vk_init_vulkan(required_extensions, num_extensions)) return 0;
#endif //BOB_INCLUDE_GLAD
    bob_state.contexts = malloc(sizeof(BOB_Context) * num_contexts);
    memset(bob_state.contexts, 0, sizeof(BOB_Context) * num_contexts);
    bob_state.context_count = 0;
    bob_state.next_context_slot = UINT32_MAX;
    bob_state.context_capcity = num_contexts;

    return 1;
}

void BOB_terminate() {
    for(size_t i = 0; i < bob_state.context_capcity; i++) {
        if(bob_state.contexts[i].context_memory.memory != NULL) {
            BOBi_destroy_context(i);
        }
    }

    free(bob_state.contexts);
    bob_state = (BOBi_Internal_State){0};
}


//========================================================== RENDERER FUNCTIONS ===========================================

//Initialises the pixel renderer
uint8_t BOB_renderer_init(BOB_Context_Handle context, size_t width, size_t height, size_t vertex_capacity, size_t index_capacity, size_t draw_call_capacity, BOB_Renderer *out) {
    if(context & BOBi_MSB) return 0;

    BOB_Context *intrn_context;
    if(!BOBi_get_context_from_handle(context, &intrn_context)) return 0;

    size_t vert_buf_sz = vertex_capacity * sizeof(BOB_Render_Vertex);
    size_t index_buf_sz = index_capacity * sizeof(uint32_t);

    out->screen_height = height;
    out->screen_width = width;

    #ifdef BOB_INCLUDE_GLAD
    if(intrn_context->type == BOB_OPENGL_CONTEXT) BOBi_gl_init_gpu_renderer_mem(out, vert_buf_sz, index_buf_sz);
    #endif //BOB_INCLUDE_GLAD

    //Setting the projection matrix
    BOB_ortho(0.0f, out->screen_width, out->screen_height, 0.0f, -BOB_MAX_LAYER, 0.0f, &out->projection);
    if(!BOB_create_material(context, (BOB_Shader_Data[2]){(BOB_Shader_Data){vertex_shader, BOB_VERTEX_SHADER},
                            (BOB_Shader_Data){fragment_shader, BOB_FRAGMENT_SHADER}}, 2,
                            (BOB_Uniform[2]){BOB_uniform_mat4("uProjection", out->projection), BOB_uniform_signed_int("screenTexture", 0)}, 2, &out->default_mat)) return 0;

    //Initialise the stack of clip rects
    out->stack = malloc(sizeof(BOBi_Clip_Stack));
    out->stack->elems = malloc(sizeof(BOBi_Clip_Rect) * INIT_STACK_CAPACITY);
    out->stack->capacity = INIT_STACK_CAPACITY;
    out->stack->size = 0;
    out->context = context;

    BOB_init_arena(&out->batch.vertex_arena, (vert_buf_sz > index_buf_sz) ? vert_buf_sz : index_buf_sz); //Since this dual use need to take the max of the two
    BOB_init_arena(&out->batch.vertex_arena_2, vert_buf_sz);
    BOB_init_arena(&out->batch.draw_call_arena, draw_call_capacity * sizeof(BOBi_Draw_Call));

    return 1;
}

//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r) {
    BOB_Context *intrn_context;
    if(BOBi_get_context(r->context, &intrn_context)) return;

    #ifdef BOB_INCLUDE_GLAD
    if(intrn_context->type == BOB_OPENGL_CONTEXT) {
        glDeleteBuffers(1, &r->vbo);
        glDeleteVertexArrays(1, &r->vao);
    }
    #endif

    free(r->stack->elems);
    r->stack->elems = NULL;
    free(r->stack);
    r->stack = NULL;

    BOB_destroy_arena(&r->batch.vertex_arena);
    BOB_destroy_arena(&r->batch.vertex_arena_2);
    BOB_destroy_arena(&r->batch.draw_call_arena);
}

//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r) {
    BOB_arena_clear(&r->batch.vertex_arena);
    BOB_arena_clear(&r->batch.vertex_arena_2);
    BOB_arena_clear(&r->batch.draw_call_arena);

    r->batch.num_draw_calls = 0;
    r->batch.num_indices = 0;
    r->batch.num_vertices = 0;
}

//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer *r) {
    //Sort the draw calls
    BOBi_quicksort_draw_calls(r, 0, r->batch.num_draw_calls-1);

    //Copy the vertices to be in sorted order
    for(size_t i = 0; i < r->batch.num_draw_calls; i++) {
        BOBi_Draw_Call *call = (BOBi_Draw_Call *)r->batch.draw_call_arena.memory + i;
        BOB_Render_Vertex *new_pos = BOB_arena_alloc(&r->batch.vertex_arena_2, call->num_vertices * sizeof(BOB_Render_Vertex), alignof(BOB_Render_Vertex));
        memcpy(new_pos, call->vertices, call->num_vertices * sizeof(BOB_Render_Vertex));
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
                uint32_t triangle_indices[(BOBi_MAX_POLY_SIZE - 2) * 3]; //Ear clipping always produces n-2 triangles for a polygon with n vertices
                BOB_Vector2 base_vertices[BOBi_MAX_POLY_SIZE];
                for(size_t j = 0; j < call->num_vertices; j++) {
                    base_vertices[j] = (BOB_Vector2){call->vertices[j].pos.x, call->vertices[j].pos.y};
                }

                size_t triangle_count = BOBi_triangulate_ec(base_vertices, call->num_vertices, triangle_indices);

                if(!triangle_count) return; //Early exit

                //Processing the returned vertex data into a more compact form so we can pass it to the renderer
                uint32_t vertex_map[BOBi_MAX_POLY_SIZE];

                //Filling the map with dummy values
                for(size_t i = 0; i < call->num_vertices; i++)
                    vertex_map[i] = UINT32_MAX;

                BOB_Render_Vertex compressed[BOBi_MAX_POLY_SIZE]; //Holds the compressed vertex values
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

                memcpy(call->vertices, compressed, vertex_count * sizeof(BOB_Render_Vertex));
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

    //TODO: THIS SHOULD THROW A MASSIVE ERROR
    BOB_Context *context = &bob_state.contexts[r->context];
    if(context->context_memory.memory == NULL) return;

    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_draw(context, r);
    #endif //BOB_INCLUDE_GLAD
}

//Updates the dimensions of the screen that the renderer renders to.
//Updates projection matrix
//NOTE: Not 100% sure that this works
void BOB_renderer_update_dimensions(BOB_Renderer *r, uint32_t width, uint32_t height) {
    BOB_Context *context;
    if(!BOBi_get_context(r->context, &context)) return;

    r->screen_width = width;
    r->screen_height = height;

    //Update projection matrix for renderer
    BOB_ortho(0.0f, width, height, 0.0f, -BOB_MAX_LAYER, 0.0f, &r->projection);
    BOB_set_material_mat4(r->default_mat, 0, r->projection);

    #ifdef BOB_INCLUDE_GLAD
    //Update the uv coordinates of the texture the renderer is rendering to
    float quadVertices[] = {
        0.0f, 0.0f,          0.0f, 0.0f,
        0.0f, height,        0.0f, 1.0f,
        width, height,       1.0f, 1.0f,
        width, 0.0f,         1.0f, 0.0f
    };

    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_copy_buffer_data(r->vbo, quadVertices, sizeof(quadVertices));
    #endif //BOB_INCLUDE_GLAD
    #ifdef BOB_INCLUDE_VULKAN
    if(context->type == BOB_VULKAN_CONTEXT) context->framebuffer_resized = 1;
    #endif //BOB_INCLUDE_VULKAN
}

//================================================== TEXTURE FUNCTIONS ================================================

//Creates a new texture on the gpu
uint8_t BOB_create_texture(BOB_Context_Handle context, uint32_t width, uint32_t height, uint8_t *data, BOB_Format format, BOB_Texture_Handle *tex) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_textures >= intrn_context->texture_capacity) {
        printf("ERROR: Exceeded Texture Capacity");
        *tex |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_tex_slot == UINT32_MAX) {
        index = intrn_context->num_textures;
    }
    else {
        index = intrn_context->next_tex_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_context->next_tex_slot = index + 1; intrn_context->next_tex_slot < intrn_context->num_textures; intrn_context->next_tex_slot++) {
            if (!intrn_context->texture_table[intrn_context->next_tex_slot].init)
                break;
        }

        if (intrn_context->next_tex_slot >= intrn_context->num_textures)
            intrn_context->next_tex_slot = UINT32_MAX;
    }

    intrn_context->texture_table[index].init = 1; //Setting the value to be initialised
    intrn_context->texture_table[index].width = width;
    intrn_context->texture_table[index].height = height;
    intrn_context->texture_table[index].format = format;

    #ifdef BOB_INCLUDE_GLAD
    if(intrn_context->type == BOB_OPENGL_CONTEXT) BOBi_gl_create_tex(&intrn_context->texture_table[index].opengl.texture, width, height, data, format);
    #endif //BOB_INCLUDE_GLAD

    intrn_context->num_textures++;
    *tex = ((uint64_t)context << 32) | index;
    return 1;
}

void BOB_texture_free(BOB_Texture_Handle *tex) {
    if(*(tex) & BOBi_MSB) return; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(*tex, &context, &index)) return;
    if(index < context->next_tex_slot) context->next_tex_slot = index;
    *(tex) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_texture_free(context, index);
}

BOB_Texture *BOB_get_tex_ref(BOB_Texture_Handle tex) {
    if(tex & BOBi_MSB) return NULL; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(tex, &context, &index)) return NULL;

    return &context->texture_table[index];
}

//====================================== MATERIAL FUNCTIONS ======================================

uint8_t get_uniform(BOB_Material_Handle mat, char *name, BOB_Uniform_Handle *uniform) {
    if(*uniform & BOBi_MSB) return 0;

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    BOB_Material m = context->material_table[index];
    for(size_t i = 0; i < m.uniform_count; i++) {
        if(!strcmp(name, m.uniforms[i].name)){
            *uniform = i;
            return 1;
        }
    }

    *uniform |= BOBi_MSB;
    return 0;
}

uint8_t BOB_create_material(BOB_Context_Handle context, BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_materials >= intrn_context->material_capacity) {
        printf("ERROR: Exceeded Material Capacity\n");
        *mat |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_mat_slot == UINT32_MAX) {
        index = intrn_context->num_materials;
    }
    else {
        index = intrn_context->next_mat_slot;

        for (intrn_context->next_mat_slot = index + 1; intrn_context->next_mat_slot < intrn_context->num_materials; intrn_context->next_mat_slot++) {
            if (!intrn_context->material_table[intrn_context->next_mat_slot].init)
                break;
        }

        if (intrn_context->next_mat_slot >= intrn_context->num_materials)
            intrn_context->next_mat_slot = UINT32_MAX;
    }

    uint32_t s;
    //TODO: Make an arena for this. Currently need to do this since cannot have references to stack memory in heap memory
    //otherwise will get pointer badness
    BOB_Uniform *temp = malloc(sizeof(BOB_Uniform) * num_uniforms);
    memcpy(temp, uniforms, num_uniforms * sizeof(BOB_Uniform));

    #ifdef BOB_INCLUDE_GLAD
    if(intrn_context->type == BOB_OPENGL_CONTEXT)
        if(!BOBi_gl_create_material(data, num_shaders, temp, num_uniforms, mat, &s)) return 0;
    #endif //BOB_INCLUDE_GLAD

    intrn_context->material_table[index] = (BOB_Material){.uniform_count = num_uniforms, .shader = s, .init = 1};

    intrn_context->material_table[index].uniforms = temp;

    intrn_context->num_materials++;
    *mat = ((uint64_t)context << 32) | index;
    return 1;
}

void BOB_material_free(BOB_Material_Handle *mat) {
    if(*(mat) & BOBi_MSB) return; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    BOBi_get_handle_data(*mat, &context, &index);
    if(index < context->next_mat_slot) context->next_mat_slot = index;
    *(mat) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_material_free(context, index);
}

BOB_Material *BOB_get_mat_ref(BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return NULL; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return NULL;

    return &context->material_table[index];
}

uint8_t BOB_set_material_float(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        context->material_table[index].uniforms[uniform].f = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        context->material_table[index].uniforms[uniform].u32 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        context->material_table[index].uniforms[uniform].i32 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector2(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        context->material_table[index].uniforms[uniform].vec2 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector3(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        context->material_table[index].uniforms[uniform].vec3 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        context->material_table[index].uniforms[uniform].vec4 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_texture(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_TEXTURE) {
        context->material_table[index].uniforms[uniform].tex_index = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_mat4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        context->material_table[index].uniforms[uniform].mat4 = value;
        context->material_table[index].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_float_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector2_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector3_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_texture_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_TEXTURE) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_mat4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(mat, &context, &index)) return 0;

    if(context->material_table[index].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        context->material_table[index].uniforms[uniform].ptr = value;
        context->material_table[index].uniforms[uniform].is_reference = 1;
    }
    return 1;
}

//================================================== TEXTURE ATLAS FUNCTIONS ========================================

//Initialises a texture atlas
uint8_t BOB_atlas_init(BOB_Context_Handle context, uint32_t width, uint32_t height, BOB_Format format, BOB_Atlas_Handle *a) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_atlases >= intrn_context->atlas_capacity) {
        printf("ERROR: Exceeded Atlas Capacity");
        *a |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_atlas_slot == UINT32_MAX) {
        index = intrn_context->num_atlases;
    }
    else {
        index = intrn_context->next_atlas_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_context->next_atlas_slot = index + 1; intrn_context->next_atlas_slot < intrn_context->num_atlases; intrn_context->next_atlas_slot++) {
            if (!intrn_context->atlas_table[intrn_context->next_atlas_slot].init)
                break;
        }

        if (intrn_context->next_atlas_slot >= intrn_context->num_atlases)
            intrn_context->next_atlas_slot = UINT32_MAX;
    }

    intrn_context->atlas_table[index].init = 1; //Setting the value to be initialised
    intrn_context->atlas_table[index].format = format;
    if(!BOB_create_texture(context, width, height, NULL, format, &intrn_context->atlas_table[index].texture)) {
        intrn_context->atlas_table[index] = (BOB_Atlas){0};
        *a |= BOBi_MSB;
        return 0;
    }

    intrn_context->num_atlases++;
    *a = ((uint64_t)context << 32) | index;
    return 1;
}

void BOB_atlas_free(BOB_Atlas_Handle *a) {
    if(*a & BOBi_MSB) return; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(*a, &context, &index)) return;

    if(index < context->next_atlas_slot) context->next_atlas_slot = index;
    *(a) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOB_texture_free(&context->atlas_table[index].texture);
    context->atlas_table[index] = (BOB_Atlas){0}; //Clear the data
}

BOB_Atlas *BOB_get_atlas_ref(BOB_Atlas_Handle a) {
    if(a & BOBi_MSB) return NULL; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(a, &context, &index)) return NULL;

    return &context->atlas_table[index];
}

//Returns the UV rect where the texture was placed
//pixel_size must be either 3 or 4. If it does not match the pixel size of the atlas,
//an empty quad will returned as the pixel formats are different
uint8_t BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h, BOB_Quad *out_quad) {
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(a, &context, &index)) return 0;

    if((a & BOBi_MSB) || (context->atlas_table[index].texture & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOB_Texture tex = context->texture_table[context->atlas_table[index].texture];
    if(context->atlas_table[index].cursor_y + h > tex.height) return 0; //Early exit if we can't fit the texture in

    //Move to next row if this texture doesn't fit
    if(context->atlas_table[index].cursor_x + w > tex.width) {
       context->atlas_table[index].cursor_y += context->atlas_table[index].row_height;
       context->atlas_table[index].cursor_x = 0;
       context->atlas_table[index].row_height = 0;
    }

    BOB_Quad unnormalised = {
        (float)context->atlas_table[index].cursor_x / tex.width,
        (float)context->atlas_table[index].cursor_y / tex.height,
        (float) w,
        (float) h
    };

    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_copy_data_tex(tex.opengl.texture, context->atlas_table[index].format, unnormalised, pixels);
    #endif //BOB_INCLUDE_GLAD

    //Compute normalised UVs
    BOB_Quad uv = {
        (float)context->atlas_table[index].cursor_x / tex.width,
        (float)context->atlas_table[index].cursor_y / tex.height,
        (float) w / tex.width,
        (float) h / tex.height
    };

    context->atlas_table[index].cursor_x += w;
    if(h > context->atlas_table[index].row_height) context->atlas_table[index].row_height = h;

    *out_quad = uv;
    return 1;
}

//======================================================= PIXELBUFFER FUNCTIONS ==============================================

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
uint8_t BOB_pixelbuffer_init(BOB_Context_Handle context, size_t width, size_t height, BOB_Format format, BOB_PixelBuffer_Handle *pb) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_pixelbuffers >= BOB_MAX_PIXELBUFFER_CAPACITY) {
        printf("ERROR: Exceeded PixelBuffer Capacity");
        *pb |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_pixelbuf_slot == UINT32_MAX) {
        index = intrn_context->num_pixelbuffers;
    }
    else {
        index = intrn_context->next_pixelbuf_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_context->next_pixelbuf_slot = index + 1; intrn_context->next_pixelbuf_slot < intrn_context->num_pixelbuffers; intrn_context->next_pixelbuf_slot++) {
            if (!intrn_context->pixelbuffer_table[intrn_context->next_pixelbuf_slot].init)
                break;
        }

        if (intrn_context->next_pixelbuf_slot >= intrn_context->num_pixelbuffers)
            intrn_context->next_pixelbuf_slot = UINT32_MAX;
    }


    //Setting up the texture for the pixel simulations:
    if(!BOB_create_texture(context, width, height, NULL, format, &intrn_context->pixelbuffer_table[index].pixel_tex)) {
        intrn_context->pixelbuffer_table[index] = (BOB_PixelBuffer){0};
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

    intrn_context->pixelbuffer_table[index].buf_sz = width * height * pixel_size;

    //Setting up the pbo for the pixel simulations
    #ifdef BOB_INCLUDE_GLAD
    if(intrn_context->type == BOB_OPENGL_CONTEXT)
        if(!BOBi_gl_init_pbo(&intrn_context->pixelbuffer_table[index].pbo, intrn_context->pixelbuffer_table[index].buf_sz, pb)) return 0;
    #endif

    intrn_context->pixelbuffer_table[index].init = 1; //Setting the value to be initialised

    intrn_context->num_pixelbuffers++;
    *pb = ((uint64_t)context << 32) | index;
    return 1;
}
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer_Handle *pb) {
    if(*(pb) & BOBi_MSB) return; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(*pb, &context, &index)) return;

    if(index < context->next_pixelbuf_slot) context->next_pixelbuf_slot = index;
    *(pb) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle

    BOBi_pixelbuffer_free(context, index);
}

BOB_PixelBuffer *BOB_get_pixelbuf_ref(BOB_PixelBuffer_Handle pb) {
    if(pb & BOBi_MSB) return NULL; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &context, &index)) return NULL;

    return &context->pixelbuffer_table[index];
}

//Draws the entire frame directly on the screen by copying its entire contents into the renderer's pixel buffer
uint8_t BOB_pixelbuffer_updload_data(BOB_PixelBuffer_Handle pb, uint8_t *data) {
    if(pb & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &context, &index)) return 0;

    uint32_t tex_index;
    if(!BOBi_get_index_from_handle(context->pixelbuffer_table[pb].pixel_tex, &tex_index)) return 0;
    BOB_Texture tex = context->texture_table[tex_index];

    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT)
        if(!BOBi_gl_copy_pbo(context->pixelbuffer_table[index].pbo, context->pixelbuffer_table[index].buf_sz, tex.opengl.texture, tex.width, tex.height, data)) return 0;
    #endif
    return 1;
}

//Gets the pixel data from a PixelBuffer
uint8_t BOB_pixelbuffer_get_data(BOB_PixelBuffer_Handle pb, uint8_t *dest) {
    if(pb & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &context, &index)) return 0;

    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT)
        if(!BOBi_gl_get_pbo_data(context->pixelbuffer_table[index].pbo, context->pixelbuffer_table[index].buf_sz, dest)) return 0;
    #endif

    return 1;
}

//============================================================= DRAWING FUNCTIONS ===========================================

uint8_t BOB_draw_texture(BOB_Renderer *r, BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_texture_channel(r, texture, screen_quad, tex_sub_rect, colour, layer, rotation, r->default_mat, 0);
}

//Draws an atlas quad
uint8_t BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation) {
    if(atlas & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &context, &index)) return 0;
    return BOB_draw_atlas_quad_channel(r, screen_quad, tex_sub_rect, colour, context->atlas_table[index].texture, layer, rotation, r->default_mat, 0);
}

uint8_t BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_pixel_buffer_channel(r, pb, dimensions, uv_dimensions, colour, layer, rotation, r->default_mat, 0);
}

uint8_t BOB_draw_line(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer) {
    return BOB_draw_line_mat(r, start_pos, end_pos, thickness, colour, layer, r->default_mat);
}

uint8_t BOB_draw_quad(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_quad_mat(r, quad, colour, layer, rotation, r->default_mat);
}

uint8_t BOB_draw_unfilled_quad(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_unfilled_quad_mat(r, quad, thickness, colour, layer, rotation, r->default_mat);
}

uint8_t BOB_draw_polygon(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_polygon_mat(r, poly_points, poly_size, colour, layer, rotation, r->default_mat);
}

//Draws an unfilled polygon
uint8_t BOB_draw_unfilled_polygon(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation) {
    return BOB_draw_unfilled_polygon_mat(r, poly_points, poly_size, colour, thickness, layer, rotation, r->default_mat);
}

uint8_t BOB_draw_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer) {
    return BOB_draw_circle_mat(r, centre, radius, colour, layer, r->default_mat);
}

uint8_t BOB_draw_unfilled_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer) {
    return BOB_draw_unfilled_circle_mat(r, centre, radius, thickness, colour, layer, r->default_mat);
}

//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_mat(BOB_Renderer *r, BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    return BOB_draw_texture_channel(r, texture, screen_quad, tex_sub_rect, colour, layer, rotation, mat, 0);
}

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_mat(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(atlas & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &context, &index)) return 0;
    return BOB_draw_atlas_quad_channel(r, screen_quad, tex_sub_rect, colour, context->atlas_table[index].texture, layer, rotation, mat, 0);
}

//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixel_buffer_mat(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    return BOB_draw_pixel_buffer_channel(r, pb, dimensions, uv_dimensions, colour, layer, rotation, mat, 0);
}

//Draws a filled circle with a specified material
uint8_t BOB_draw_circle_mat(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    if(!BOBi_get_context(r->context, &context)) return 0;

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

    BOB_Vector2 points2[BOBi_MAX_POLY_SIZE];
    BOB_Vector3 points3[BOBi_MAX_POLY_SIZE];
    memcpy(points2, vertices, vertex_count * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points2, vertex_count);
    if(clipped_size < 3) return 1; //Early exit

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    //Generating the indecies for the triangle ebo
    for(int i = 0; i < clipped_size; i++) {
        points3[i] = (BOB_Vector3){points2[i].x, points2[i].y, layer};
        indices[index_count++] = 0;
        indices[index_count++] = i;
        indices[index_count++] = ((i+1) % clipped_size);
    }

    BOBi_create_draw_call(r, points3, clipped_size, NULL, clipped_size * 3, colour, context->default_tex, mat, 0, BOBi_DRAW_CIRCLE);
    return 1;
}

//Draws a filled quad with a specified material
uint8_t BOB_draw_quad_mat(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    if(!BOBi_get_context(r->context, &context)) return 0;

    if(!BOBi_clip_quad(r, &quad)) return 1; //Early exit

    BOB_Vector2 rotated_coords[4];
    BOBi_rotate_quad(quad, rotated_coords, rotation);
    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    BOB_Vector3 strip[4] = {
        {rotated_coords[0].x, rotated_coords[0].y, layer},
        {rotated_coords[1].x, rotated_coords[1].y, layer},
        {rotated_coords[2].x, rotated_coords[2].y, layer},
        {rotated_coords[3].x, rotated_coords[3].y, layer}
    };

    // BOBi_draw_mesh(r, strip, 4, NULL, (uint32_t[6]){0,1,3,1,2,3}, 6, colour, intrn_data.default_tex, mat, 0);
    BOBi_create_draw_call(r, strip, 4, NULL, 6, colour, context->default_tex, mat, 0, BOBi_DRAW_QUAD);
    return 1;
}

//Draws a filled triangle with a specified material
uint8_t BOB_draw_polygon_mat(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    if(!BOBi_get_context(r->context, &context)) return 0;

    BOBi_rotate_polygon(poly_points, poly_size, rotation);

    BOB_Vector2 points[BOBi_MAX_POLY_SIZE];
    memcpy(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points, poly_size);
    if(clipped_size < 3) return 1; //Early exit

    BOB_Vector3 vertices[BOBi_MAX_POLY_SIZE]; //Holds the compressed vertex values
    for(size_t i = 0; i < clipped_size; i++) {
        vertices[i] = (BOB_Vector3){points[i].x, points[i].y, layer};
    }
    BOBi_create_draw_call(r, vertices, clipped_size, NULL, (clipped_size - 2) * 3, colour, context->default_tex, mat, 0, BOBi_DRAW_POLY);
    return 1;
}
//Draws an unfilled circle with a specified material
uint8_t BOB_draw_unfilled_circle_mat(BOB_Renderer *r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
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
uint8_t BOB_draw_unfilled_quad_mat(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
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
uint8_t BOB_draw_unfilled_polygon_mat(BOB_Renderer *r, BOB_Vector2 *poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Vector2 points[BOBi_MAX_POLY_SIZE];
    memcpy(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points, poly_size);
    if(clipped_size < 2) return 1; //Early exit

    for(size_t i = 0; i < clipped_size; i++) {
        size_t next = (i+1) % clipped_size;
        BOB_draw_line_mat(r, points[i], points[next], thickness, colour, layer, mat);
    }

    return 1;
}

//Draws a line between two points with a specified material
uint8_t BOB_draw_line_mat(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    if(!BOBi_get_context(r->context, &context)) return 0;

    if(!BOBi_clip_line(r, &start_pos, &end_pos)) return 1; //Early exit

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

        BOBi_create_draw_call(r, strip, 4, NULL, 6, colour, context->default_tex, mat, 0, BOBi_DRAW_QUAD);
    }

    return 1;
}

//Draws a dynamically allocated texture with a specified material
uint8_t BOB_draw_texture_channel(BOB_Renderer *r, BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((texture & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(texture, &context, &index)) return 0;

    if(!BOBi_clip_quad(r, &screen_quad)) return 1; //Early exit

    BOB_Vector2 rotated_coords[4];
    BOBi_rotate_quad(screen_quad, rotated_coords, rotation);

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    BOB_Vector3 coords[4] = {
        {rotated_coords[0].x, rotated_coords[0].y, layer},
        {rotated_coords[1].x, rotated_coords[1].y, layer},
        {rotated_coords[2].x, rotated_coords[2].y, layer},
        {rotated_coords[3].x, rotated_coords[3].y, layer}
    };

    float width = context->texture_table[index].width;
    float height = context->texture_table[index].height;

    BOB_Vector2 uv[4] = {
        {tex_sub_rect.x / width, tex_sub_rect.y / height},
        {tex_sub_rect.x / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width , tex_sub_rect.y / height}
    };

    BOBi_create_draw_call(r, coords, BOB_VERTICIES_PER_QUAD, uv, BOB_INDECIES_PER_QUAD, colour, texture, mat, channel, BOBi_DRAW_QUAD);

    return 1;
}

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_channel(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((atlas & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(atlas, &context, &index)) return 0;
    return BOB_draw_texture_channel(r, context->atlas_table[index].texture, screen_quad, tex_sub_rect, colour, layer, rotation, mat, channel);
}

//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixel_buffer_channel(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat, uint8_t channel) {
    if((pb & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(pb, &context, &index)) return 0;

    if(!BOBi_clip_quad(r, &dimensions)) return 1; //Early exit
    BOB_Texture tex = context->texture_table[context->pixelbuffer_table[index].pixel_tex];

    #ifdef BOB_INCLUDE_GLAD
    if(context->type == BOB_OPENGL_CONTEXT) BOBi_gl_upload_pbo_data(context->pixelbuffer_table[index].pbo, tex.opengl.texture, tex.width, tex.height);
    #endif //BOB_INCLUDE_GLAD

    return BOB_draw_texture_channel(r, context->pixelbuffer_table[index].pixel_tex, dimensions, uv_dimensions, colour, layer, rotation, mat, channel);
}

//=================================== CLIPPING FUNCTIONS =====================================

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer *r, BOB_Quad rect, BOB_Clip_Dir dir) {
    BOBi_Clip_Stack *stack = r->stack;
    if(stack->size >= stack->capacity) {
        size_t newCap = (stack->capacity == 0) ? 4 : stack->capacity * 2;
        stack->elems = realloc(stack->elems, newCap);
        stack->capacity = newCap;
    }

    BOBi_Clip_Rect clip_rect = (BOBi_Clip_Rect) {
        rect.x, rect.x+rect.w, rect.y, rect.y+rect.w,
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
void BOB_end_clip(BOB_Renderer *r) {
    BOB_ASSERT(r->stack->size > 0 && "Popping an empty stack");

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

void BOBi_append_glyph(BOB_Font *font, BOB_Glyph g) {
    if(font->glyphs == NULL) font->glyphs = malloc(sizeof(BOB_Glyph) * font->glyph_capacity);
    if(font->glyph_count >= font->glyph_capacity) {
        size_t new_cap = (font->glyph_capacity > 0) ? font->glyph_capacity * 2 : 16;
        font->glyphs = realloc(font->glyphs, new_cap);
        font->glyph_capacity = new_cap;
    }

    BOBi_hashmap_add(font->glyph_map, g.codepoint, font->glyph_count);
    font->glyphs[font->glyph_count++] = g;
}
void BOBi_append_kerning(BOB_Font *font, BOB_Kerning k) {
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
uint8_t BOBi_parse_common(char *line, BOB_Font *font) {
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

uint8_t BOBi_parse_line(char *line, BOB_Font *font) {
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

uint8_t BOBi_parse_text(BOB_Font *font, uint8_t *data, size_t data_sz) {
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
uint8_t BOBi_parse_common_block(BOB_Font *font, uint8_t *data, size_t data_sz) {
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
uint8_t BOBi_parse_chars_block(BOB_Font *font, uint8_t *data, size_t data_sz) {
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
uint8_t BOBi_parse_kernings_block(BOB_Font *font, uint8_t *data, size_t data_sz) {
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

uint8_t BOBi_parse_binary(BOB_Font *font, uint8_t *data, size_t data_sz) {
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

uint8_t BOB_load_bmf_font(BOB_Context_Handle context, const char *font_path, BOB_Font_Handle *font, BOB_BMF_Format format) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_fonts >= intrn_context->font_capacity) {
        printf("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_font_slot == UINT32_MAX) {
        index = intrn_context->num_fonts;
    }
    else {
        index = intrn_context->next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_context->next_font_slot = index + 1; intrn_context->next_font_slot < intrn_context->num_fonts; intrn_context->next_font_slot++) {
            if (!intrn_context->font_table[intrn_context->next_font_slot].init)
                break;
        }

        if (intrn_context->next_font_slot >= intrn_context->num_fonts)
            intrn_context->next_font_slot = UINT32_MAX;
    }

    intrn_context->font_table[index].init = 1; //Setting the value to be initialised

    uint8_t *buf;
    int size = BOBi_read_to_end(font_path, &buf, 1);
    if(size < 0) {
        *font |= BOBi_MSB;
        return 0;
    }

    uint8_t res = (format == BOB_BMF_TEXT) ? BOBi_parse_text(&intrn_context->font_table[index], buf, size) : BOBi_parse_binary(&intrn_context->font_table[index], buf, size);
    free(buf);
    if(!res) {
        *font |= BOBi_MSB;
        intrn_context->font_table[index] = (BOB_Font){0}; //Clear all of the initially assigned font data
        return 0;
    }

    intrn_context->num_fonts++;
    *font = ((uint64_t)context << 32) | index;
    return 1;
}

uint8_t BOB_create_custom_font(BOB_Context_Handle context, BOB_Font_Handle *font, size_t num_glyphs, size_t num_kernings, size_t line_height, size_t base) {
    BOB_Context *intrn_context;
    if(!BOBi_get_context(context, &intrn_context)) return 0;

    if(intrn_context->num_fonts >= intrn_context->font_capacity) {
        printf("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_context->next_font_slot == UINT32_MAX) {
        index = intrn_context->num_fonts;
    }
    else {
        index = intrn_context->next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_context->next_font_slot = index + 1; intrn_context->next_font_slot < intrn_context->num_fonts; intrn_context->next_font_slot++) {
            if (!intrn_context->font_table[intrn_context->next_font_slot].init)
                break;
        }

        if (intrn_context->next_font_slot >= intrn_context->num_fonts)
            intrn_context->next_font_slot = UINT32_MAX;
    }

    intrn_context->font_table[index].init = 1; //Setting the value to be initialised
    intrn_context->font_table[index].base = base;
    intrn_context->font_table[index].line_height = line_height;
    intrn_context->font_table[index].glyph_capacity = num_glyphs;
    intrn_context->font_table[index].glyph_count = 0;
    intrn_context->font_table[index].kerning_capacity = num_kernings;
    intrn_context->font_table[index].kerning_count = 0;
    intrn_context->font_table[index].page_count = 0;
    if(num_glyphs) {
        intrn_context->font_table[index].glyphs = malloc(sizeof(BOB_Glyph) * num_glyphs);
        if(!BOBi_hashmap_init(intrn_context->font_table[index].glyph_capacity, intrn_context->font_table[index].glyph_map)) return 0;
    }
    if(num_kernings) {
        intrn_context->font_table[index].kernings = malloc(sizeof(BOB_Kerning) * num_kernings);
        intrn_context->font_table[index].kerning_map = malloc(sizeof(BOBi_Hashmap));
        if(!BOBi_hashmap_init(intrn_context->font_table[index].kerning_capacity, intrn_context->font_table[index].kerning_map)) return 0;
    }

    intrn_context->num_fonts++;
    *font = ((uint64_t)context << 32) | index;
    return 1;
}

uint8_t BOB_add_font_page(BOB_Font_Handle font, uint32_t page_width, uint32_t page_height, uint8_t *page_data, BOB_Format page_format) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;
    uint32_t handle = (font & 0xFFFFFFFF00000000) >> 32;

     return BOB_create_texture(handle, page_width, page_height, page_data, page_format, &context->font_table[index].pages[context->font_table[index].page_count++]);
}

uint8_t BOB_draw_codepoint(BOB_Renderer *r, BOB_Font_Handle font, uint32_t codepoint, BOB_Vector2 *pos, BOB_Vector4 colour, uint16_t layer) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;

    BOB_Font f = context->font_table[index];
    uint32_t hash_index = BOBi_hashmap_get(f.glyph_map, codepoint);
    if(hash_index == UINT32_MAX) return 0; //Codepoint doesn't exist

    BOB_Glyph g = f.glyphs[hash_index];
    //Setting the flags we pass to the shader
    uint8_t chnl_flags = g.channel | BOB_GLYPH_BIT;
    if(context->texture_table[f.pages[g.page]].format == BOB_RED) chnl_flags |= BOB_GREYSCALE_BIT;

    BOB_draw_texture_channel(r, f.pages[g.page], (BOB_Quad){pos->x + g.x_offset, pos->y + g.y_offset, g.sub_rect.w, g.sub_rect.h}, g.sub_rect, colour, layer, 0.0f, r->default_mat, chnl_flags);
    pos->x += g.x_advance;
    return 1;
}

typedef uint32_t (*BOBi_Codepoint_Reader)(void *str, size_t index);
static uint32_t BOBi_read_char(void *str, size_t index) { return (uint32_t)((char *)str)[index]; }
static uint32_t BOBi_read_codepoint(void *str, size_t index) { return ((uint32_t *)str)[index]; }

static uint8_t BOBi_draw_string(BOB_Renderer *r, BOB_Font_Handle font, void *str, size_t str_len, BOBi_Codepoint_Reader reader, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;

    BOB_Font f = context->font_table[index];
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

        if(!BOB_draw_codepoint(r, font, codepoint, start, colour, layer)) return 0;
        prev = codepoint;
    }

    return 1;
}

uint8_t BOB_draw_char_string(BOB_Renderer *r, BOB_Font_Handle font, char *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    return BOBi_draw_string(r, font, str, str_len, BOBi_read_char, start, colour, layer);
}

uint8_t BOB_draw_codepoint_string(BOB_Renderer *r, BOB_Font_Handle font, uint32_t *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    return BOBi_draw_string(r, font, str, str_len, BOBi_read_codepoint, start, colour, layer);
}

uint8_t BOB_append_glyph(BOB_Font_Handle font, BOB_Glyph glyph) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;

    BOBi_append_glyph(&context->font_table[index], glyph);
    return 1;
}
uint8_t BOB_append_kerning(BOB_Font_Handle font, BOB_Kerning kerning) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;

    BOBi_append_kerning(&context->font_table[index], kerning);
    return 1;
}

static uint8_t BOBi_measure_string(void *str, size_t str_len, BOBi_Codepoint_Reader reader, BOB_Font_Handle font, BOB_Vector2 *out) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(font, &context, &index)) return 0;

    BOB_Font f = context->font_table[index];
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
    BOB_Context *context;
    uint32_t index;
    if(!BOBi_get_handle_data(*font, &context, &index)) return 0;

    BOB_Font f = context->font_table[index];

    for(size_t i = 0; i < f.page_count; i++) {
        BOB_texture_free(&f.pages[i]);
    }

    BOBi_font_free(context, *font);

    *font |= BOBi_MSB;
    return 1;
}
