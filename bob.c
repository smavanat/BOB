#include "bob.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>

//Return the value of the element at the top of the stack without popping it
#define BOB_peek_clip_rect(stack) (((stack)->size > 0) ? (stack)->elems[(stack)->size-1] : (BOBi_Clip_Rect){0})

//Calculates the projection matrix
void BOB_ortho(float left, float right, float bottom, float top, float nearZ, float farZ, BOB_Mat4 dest) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            dest[i][j] = 0;
        }
    }

    float rl = 1.0 / (right  - left);
    float tb = 1.0 / (top    - bottom);
    float mfn =-1.0 / (farZ - nearZ);

    dest[0][0] = 2.0 * rl;
    dest[1][1] = 2.0 * tb;
    dest[2][2] = 2.0 * mfn;
    dest[3][0] =-(right  + left) * rl;
    dest[3][1] =-(top    + bottom) * tb;
    dest[3][2] = (farZ + nearZ) * mfn;
    dest[3][3] = 1.0;
}

//Compiles a shader from a source file given the desired shader type
unsigned int BOBi_create_shader(const char **src, int shader_type) {
    unsigned int shader;
    shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, src, NULL);
    glCompileShader(shader);

    int result;
    char infolog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if(!result) {
        glGetShaderInfoLog(shader, 512, NULL, infolog);
        BOB_PRINT("ERROR::SHADER::COMPILATION_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            BOB_PRINT("%c", infolog[i]);
        }
        BOB_PRINT("\n");
    }

    return shader;
}

//Shaders for this program are simple enough that we can just encode them as strings
//to avoid annoying file loading/reading every startup
const char *vertex_shader = "#version 330 core\n"
                            "layout (location = 0) in vec3 aPos;\n"
                            "layout (location = 1) in vec4 aColor;\n"
                            "layout (location = 2) in vec2 aTexCoord;\n"
                            "uniform mat4 uProjection;\n"
                            "out vec4 ourColor;\n"
                            "out vec2 TexCoord;\n"
                            "void main() {\n"
                            "    gl_Position = uProjection * vec4(aPos, 1.0);\n"
                            "    ourColor = aColor;"
                            "    TexCoord = aTexCoord;\n"
                            "}\n";
const char *fragment_shader = "#version 330 core\n"
                              "out vec4 FragColor;\n"
                              "in vec2 TexCoord;\n"
                              "in vec4 ourColor;\n"
                              "uniform sampler2D screenTexture;\n"
                              "void main() {\n"
                              "    FragColor = texture(screenTexture, TexCoord) * ourColor;\n"
                              "}\n";

//Initialises the pixel renderer
BOB_Renderer BOB_renderer_init(size_t width, size_t height) {
    BOB_Renderer r = {0};
    r.screen_height = height;
    r.screen_width = width;

    //Getting the shader for this renderer
    r.shader = glCreateProgram();
    unsigned int vert = BOBi_create_shader(&vertex_shader, GL_VERTEX_SHADER);
    unsigned int frag = BOBi_create_shader(&fragment_shader, GL_FRAGMENT_SHADER);
    glAttachShader(r.shader, vert);
    glAttachShader(r.shader, frag);
    glLinkProgram(r.shader);
    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &r.vao);
    glBindVertexArray(r.vao);

    //Getting the vbo
    glGenBuffers(1, &r.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    glBufferData(GL_ARRAY_BUFFER, BOB_MAX_VERTEX_CAPACITY * sizeof(BOB_Render_Vertex), NULL, GL_DYNAMIC_DRAW);

    //Getting the ebo
    glGenBuffers(1, &r.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * BOB_MAX_INDEX_CAPACITY, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, pos)); //Vertex Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, colour)); //Vertex Colour
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, uv)); //UV
    glEnableVertexAttribArray(2);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glClearDepth(0.0);

    //Setting the projection matrix
    BOB_ortho(0.0f, r.screen_width, r.screen_height, 0.0f, 1.0f, -1.0f, r.projection);

    r.num_atlas_batches = 0;
    r.atlas_batch_capacity = 2;
    // r.layer.atlas_batches = calloc(r.atlas_batch_capacity, sizeof(BOB_AtlasRenderBatch));
    r.layer.earliest_atlas_used = -1;
    r.layer.dynamic_texture_count = 0;

    //Make the special debug atlas as the first one
    BOB_TextureAtlas *ta = BOB_MALLOC(sizeof(BOB_TextureAtlas));
    *ta = BOB_atlas_init_blank(1, 1, 4);
    uint8_t white[4] = {255, 255, 255, 255};
    BOB_atlas_pack(ta, white, 1, 1, 4);

    BOB_add_texture_atlas(&r, ta);

    //Initialise the stack of clip rects
    r.stack = BOB_MALLOC(sizeof(BOBi_Clip_Stack));
    r.stack->elems = BOB_MALLOC(sizeof(BOBi_Clip_Rect) * INIT_STACK_CAPACITY);
    r.stack->capacity = INIT_STACK_CAPACITY;
    r.stack->size = 0;

    return r;
}

//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r) {
    glDeleteBuffers(1, &r->vbo);
    glDeleteVertexArrays(1, &r->vao);
    glDeleteProgram(r->shader);
    for(int j = 0; j < r->num_atlas_batches; j++) {
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, j).vertex_data);
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, j).index_data);
        BOB_atlas_free(BOB_GET_ATLAS_BATCH(r, j).a);
    }
    BOB_FREE(BOB_GET_ATLAS_BATCH(r, 0).a);
    // if(r->layer.atlas_batches) BOB_FREE(r->layer.atlas_batches);

    BOB_FREE(r->stack->elems);
    r->stack->elems = NULL;
    BOB_FREE(r->stack);
    r->stack = NULL;
}

//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r) {
    for(int j = 0; j < r->num_atlas_batches; j++) {
        BOB_GET_ATLAS_BATCH(r, j).index_count = 0;
        BOB_GET_ATLAS_BATCH(r, j).vertex_count = 0;
    }
    r->layer.dynamic_texture_count = 0;
    r->layer.earliest_atlas_used = -1;
}

//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer *r) {
    //Shifting the positions according to the projection matrix
    glUseProgram(r->shader);
    int  proj_loc = glGetUniformLocation(r->shader, "uProjection");
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float *)r->projection);

    //Bind all of the arrays and buffers we will reuse over time
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);

    for(int j = 0; j < r->layer.dynamic_texture_count; j++) {
        BOB_DynamicTexture tex = r->layer.dynamic_textures[j];

        // Bind the texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex.texture);
        glUniform1i(glGetUniformLocation(r->shader, "screenTexture"), 0);

        // Build a single quad directly into a temporary buffer
        BOB_Render_Vertex verts[4] = {
            {tex.colour, {tex.dimensions.x, tex.dimensions.y, tex.depth}, {tex.uv.x, tex.uv.y}},
            {tex.colour, {tex.dimensions.x, tex.dimensions.y + tex.dimensions.h, tex.depth}, {tex.uv.x, tex.uv.y+tex.uv.h}},
            {tex.colour, {tex.dimensions.x + tex.dimensions.w, tex.dimensions.y + tex.dimensions.h, tex.depth}, {tex.uv.x + tex.uv.w, tex.uv.y + tex.uv.h}},
            {tex.colour, {tex.dimensions.x + tex.dimensions.w, tex.dimensions.y, tex.depth}, {tex.uv.x + tex.uv.w, tex.uv.y}},
        };
        uint32_t indices[] = { 0, 1, 3, 1, 2, 3 };

        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    if(r->layer.earliest_atlas_used < 0) return;
    for(int j = r->layer.earliest_atlas_used; j < r->num_atlas_batches; j++) {
        if(BOB_GET_ATLAS_BATCH(r, j).index_count > 0) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, BOB_GET_ATLAS_BATCH(r, j).vertex_count * sizeof(BOB_Render_Vertex), BOB_GET_ATLAS_BATCH(r, j).vertex_data); //Copies the data from renderer's triangle data into the vbo
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, BOB_GET_ATLAS_BATCH(r, j).index_count * sizeof(uint32_t), BOB_GET_ATLAS_BATCH(r, j).index_data); //Copies the quad data into the vbo

            //Bind the atlas texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, BOB_GET_ATLAS_BATCH(r, j).a->texture);
            glUniform1i(glGetUniformLocation(r->shader, "screenTexture"), 0);

            glDrawElements(GL_TRIANGLES, BOB_GET_ATLAS_BATCH(r, j).index_count, GL_UNSIGNED_INT, 0); //Make the draw call
        }
    }
}

void BOBi_flush(BOB_Renderer *r, uint32_t atlas, uint32_t num_vertices, uint32_t num_indices) {
    if(BOB_GET_ATLAS_BATCH(r, atlas).index_count + num_indices >= BOB_MAX_INDEX_CAPACITY || 
      BOB_GET_ATLAS_BATCH(r, atlas).vertex_count + num_vertices >= BOB_MAX_VERTEX_CAPACITY) {
        BOB_renderer_end(r);
        BOB_renderer_begin(r);
        return;
    }

    if(BOB_GET_ATLAS_BATCH(r, atlas).index_count + num_indices >= BOB_GET_ATLAS_BATCH(r, atlas).index_size) {
        size_t new_cap = BOB_GET_ATLAS_BATCH(r, atlas).index_size * 2;
        if(new_cap < BOB_GET_ATLAS_BATCH(r, atlas).index_count + num_indices) new_cap = BOB_GET_ATLAS_BATCH(r, atlas).index_count + num_indices;
        uint32_t *temp = calloc(new_cap, sizeof(uint32_t));
        BOB_MEMCPY(temp, BOB_GET_ATLAS_BATCH(r, atlas).index_data, sizeof(uint32_t) * BOB_GET_ATLAS_BATCH(r, atlas).index_size);
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, atlas).index_data);
        BOB_GET_ATLAS_BATCH(r, atlas).index_data = temp;
        BOB_GET_ATLAS_BATCH(r, atlas).index_size = new_cap;
    }
    if(BOB_GET_ATLAS_BATCH(r, atlas).vertex_count + num_vertices >= BOB_GET_ATLAS_BATCH(r, atlas).vertex_size) {
        size_t new_cap = BOB_GET_ATLAS_BATCH(r, atlas).vertex_size * 2;
        if(new_cap < BOB_GET_ATLAS_BATCH(r, atlas).vertex_count + num_vertices) new_cap = BOB_GET_ATLAS_BATCH(r, atlas).vertex_count + num_vertices;
        BOB_Render_Vertex *temp = calloc(new_cap, sizeof(BOB_Render_Vertex));
        BOB_MEMCPY(temp, BOB_GET_ATLAS_BATCH(r, atlas).vertex_data, sizeof(BOB_Render_Vertex) * BOB_GET_ATLAS_BATCH(r, atlas).vertex_size);
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, atlas).vertex_data);
        BOB_GET_ATLAS_BATCH(r, atlas).vertex_data = temp;
        BOB_GET_ATLAS_BATCH(r, atlas).vertex_size = new_cap;
    }
}

uint32_t BOB_add_texture_atlas(BOB_Renderer *r, BOB_TextureAtlas *ta) {
    if(r->num_atlas_batches >= BOB_MAX_ATLAS_CAPACITY) {
        // size_t newCap = r->atlas_batch_capacity * 2;
        // BOB_AtlasRenderBatch *temp = BOB_MALLOC(newCap * sizeof(BOB_AtlasRenderBatch));
        // BOB_MEMCPY(temp, r->layer.atlas_batches, r->num_atlas_batches * sizeof(BOB_AtlasRenderBatch));
        // BOB_FREE(r->layer.atlas_batches);
        // r->layer.atlas_batches = temp;
        // r->atlas_batch_capacity = newCap;
        BOB_PRINT("ERROR: Exceeded Atlas capacity\n");
        return UINT32_MAX;
    }

    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).index_count = 0;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).vertex_count = 0;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).index_size = BOB_INIT_INDEX_CAPACITY;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).vertex_size = BOB_INIT_VERTEX_CAPACITY;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).index_data = NULL;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).vertex_data = NULL;
    BOB_GET_ATLAS_BATCH(r, r->num_atlas_batches).a = ta;

    r->num_atlas_batches++;

    return r->num_atlas_batches - 1;
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

//Draws a texture quad
void BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint32_t atlas, float depth) {
    if(!BOBi_clip_quad(r, &screen_quad)) return;

    //If we have overreached our current rendering limit or we cannot store any more textures, end the current draw call and start a new one
    BOBi_flush(r, atlas, BOB_VERTICIES_PER_QUAD, BOB_INDECIES_PER_QUAD);

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOB_GET_ATLAS_BATCH(r, atlas).vertex_count;

    BOB_Vector3 coords[4] = {
        {screen_quad.x, screen_quad.y, depth},
        {screen_quad.x, screen_quad.y + screen_quad.h, depth},
        {screen_quad.x + screen_quad.w, screen_quad.y + screen_quad.h, depth},
        {screen_quad.x + screen_quad.w , screen_quad.y, depth}
    };

    float width = BOB_GET_ATLAS_BATCH(r, atlas).a->width;
    float height = BOB_GET_ATLAS_BATCH(r, atlas).a->height;

    BOB_Vector2 uv[4] = {
        {tex_sub_rect.x / width, tex_sub_rect.y / height},
        {tex_sub_rect.x / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width , tex_sub_rect.y / height}
    };

    //Update the earliest atlas used
    if(r->layer.earliest_atlas_used < 0 || r->layer.earliest_atlas_used < atlas)
        r->layer.earliest_atlas_used = atlas;

    //Lazy allocation of memory
    if(BOB_GET_ATLAS_BATCH(r, atlas).vertex_data == NULL || BOB_GET_ATLAS_BATCH(r, atlas).index_data == NULL) {
        BOB_GET_ATLAS_BATCH(r, atlas).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOB_GET_ATLAS_BATCH(r, atlas).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    for(int i = 0; i < BOB_VERTICIES_PER_QUAD; i++) {
        BOB_GET_ATLAS_BATCH(r, atlas).vertex_data[BOB_GET_ATLAS_BATCH(r, atlas).vertex_count++] = (BOB_Render_Vertex){colour, coords[i], uv[i]};
    }

    //Need to also add ebo data so we can remove overlapping vertices
    //First triangle
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index;
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index + 3;

    //Second triangle
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index + 2;
    BOB_GET_ATLAS_BATCH(r, atlas).index_data[BOB_GET_ATLAS_BATCH(r, atlas).index_count++] = base_index + 3;
}

void BOB_draw_texture(BOB_Renderer *r, uint32_t texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth) {
    if(!BOBi_clip_quad(r, &dimensions)) return;

    if(r->layer.dynamic_texture_count >= BOB_MAX_TEX_CAPACITY) {
        BOB_renderer_end(r);
        BOB_renderer_begin(r);
    }

    r->layer.dynamic_textures[r->layer.dynamic_texture_count++] = (BOB_DynamicTexture){texture, depth, dimensions, uv_dimensions, colour};
}

void BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer *pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, float depth) {
    if(!BOBi_clip_quad(r, &dimensions)) return;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pb->pbo);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, pb->pixel_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pb->width, pb->height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    BOB_draw_texture(r, pb->pixel_tex, dimensions, uv_dimensions, colour, depth);
}

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
BOB_PixelBuffer BOB_pixelbuffer_init(size_t width, size_t height, uint8_t pixel_size) {
    //TODO: Find some way to check the pixel formats properly
    if(pixel_size != 3 && pixel_size != 4) {
        BOB_PRINT("Invalid pixel size\n");
        return (BOB_PixelBuffer){0};
    }

    BOB_PixelBuffer pb = {0};
    pb.width = width;
    pb.height = height;

    //Setting up the texture for the pixel simulations:
    glGenTextures(1, &pb.pixel_tex); //Only use one texture for the pixels that we just write to. Could switch to two and swap them out (like framebuffers)
    glBindTexture(GL_TEXTURE_2D, pb.pixel_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pb.width, pb.height, 0, (pixel_size == 3) ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, NULL); //Setting it to use rgba colours
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    pb.buf_sz = width * height * pixel_size;

    //Setting up the pbo for the pixel simulations
    glGenBuffers(1, &pb.pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pb.pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pb.buf_sz, NULL, GL_STREAM_DRAW);
    pb.pixel_buf = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if(pb.pixel_buf) {
        BOB_MEMSET(pb.pixel_buf, 0x00, pb.buf_sz); //Setting all of the pixels to be colourless initially
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return pb;
}
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer *pb) {
    glDeleteBuffers(1, &pb->pbo);
    glDeleteTextures(1, &pb->pixel_tex);
}

//Draws the entire frame directly on the screen by copying its entire contents into the renderer's pixel buffer
void BOB_pixelbuffer_updload_data(BOB_PixelBuffer *pb, uint8_t *data) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pb->pbo);

    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    BOB_MEMCPY(ptr, data, pb->buf_sz);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindTexture(GL_TEXTURE_2D, pb->pixel_tex);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pb->width, pb->height, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

//Initialises a texture atlas
//Optionally packs a single white pixel at the start of the texture atlas to render a solid quad
BOB_TextureAtlas BOB_atlas_init(uint32_t width, uint32_t height, uint32_t texture, uint8_t pixel_size) {
    BOB_TextureAtlas a = {0};
    if(pixel_size != 3 && pixel_size != 4) return a;
    a.width = width;
    a.height = height;
    a.pixel_size = pixel_size;
    a.texture = texture;

    return a;
}

BOB_TextureAtlas BOB_atlas_init_blank(uint32_t width, uint32_t height, uint8_t pixel_size) {
    BOB_TextureAtlas a = {0};
    if(pixel_size != 3 && pixel_size != 4) return a;
    a.width = width;
    a.height = height;
    a.pixel_size = pixel_size;

    glGenTextures(1, &a.texture);
    glBindTexture(GL_TEXTURE_2D, a.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, (a.pixel_size == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return a;
}

void BOB_atlas_free(BOB_TextureAtlas *a) {
    glDeleteTextures(1, &a->texture);
}

//Returns the UV rect where the texture was placed
//pixel_size must be either 3 or 4. If it does not match the pixel size of the atlas,
//an empty quad will returned as the pixel formats are different
BOB_Quad BOB_atlas_pack(BOB_TextureAtlas *a, uint8_t* pixels, size_t w, size_t h, uint8_t pixel_size) {
    //If the atlas' pixel format has not been set, set it to the one passed in
    if(a->pixel_size == 0) a->pixel_size = pixel_size;
    else if(a->pixel_size != pixel_size) return (BOB_Quad){0}; //Otherwise there is a mismatch and return an empty quad

    //Move to next row if this texture doesn't fit
    if(a->cursor_x + w > a->width) {
        a->cursor_y += a->row_height;
        a->cursor_x = 0;
        a->row_height = 0;
    }

    //Upload the subregion
    glBindTexture(GL_TEXTURE_2D, a->texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, a->cursor_x, a->cursor_y, w, h, (a->pixel_size == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pixels);

    //Compute normalised UVs
    BOB_Quad uv = {
        (float)a->cursor_x / a->width,
        (float)a->cursor_y / a->height,
        (float) w / a->width,
        (float) h / a->height
    };

    a->cursor_x += w;
    if(h > a->row_height) a->row_height = h;

    return uv;
}

//Draws a mesh of triangles
void BOBi_draw_mesh(BOB_Renderer *r, BOB_Vector3 *vertices, size_t vertex_count, uint32_t *indices, size_t index_count, BOB_Vector4 colour) {
    BOBi_flush(r, 0, vertex_count, index_count);

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOB_GET_ATLAS_BATCH(r, 0).vertex_count;

    //Update the earliest atlas used
    if(r->layer.earliest_atlas_used != 0)
        r->layer.earliest_atlas_used = 0;

    //Lazy allocation of memory
    if(BOB_GET_ATLAS_BATCH(r, 0).vertex_data == NULL || BOB_GET_ATLAS_BATCH(r, 0).index_data == NULL) {
        BOB_GET_ATLAS_BATCH(r, 0).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOB_GET_ATLAS_BATCH(r, 0).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    for(size_t i = 0; i < vertex_count; i++) {
        BOB_GET_ATLAS_BATCH(r, 0).vertex_data[BOB_GET_ATLAS_BATCH(r, 0).vertex_count++] = (BOB_Render_Vertex){colour, vertices[i]};
    }

    for(size_t i = 0; i < index_count; i++) {
        BOB_GET_ATLAS_BATCH(r, 0).index_data[BOB_GET_ATLAS_BATCH(r, 0).index_count++] = base_index + indices[i];
    }
}

void BOB_draw_line(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, float depth) {
    if(!BOBi_clip_line(r, &start_pos, &end_pos)) return;

    BOB_Vector2 delta = {end_pos.x - start_pos.x, end_pos.y - start_pos.y};
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if(length > 0 && thickness > 0) {
        float scale = thickness/(2*length);

        BOB_Vector2 radius = {-scale*delta.y, scale*delta.x};
        BOB_Vector3 strip[4] = {
            {start_pos.x - radius.x, start_pos.y - radius.y, depth},
            {start_pos.x + radius.x, start_pos.y + radius.y, depth},
            {end_pos.x - radius.x, end_pos.y - radius.y, depth},
            {end_pos.x + radius.x, end_pos.y + radius.y, depth},
        };

        BOBi_draw_mesh(r, strip, 4, (uint32_t[6]){0,1,2,1,2,3}, 6, colour);
    }
}

void BOB_draw_quad(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, float depth) {
    if(!BOBi_clip_quad(r, &quad)) return;

    BOB_Vector3 strip[4] = {
        {quad.x, quad.y, depth},
        {quad.x, quad.y+quad.h, depth},
        {quad.x+quad.w, quad.y, depth},
        {quad.x+quad.w, quad.y+quad.h, depth},
    };

    BOBi_draw_mesh(r, strip, 4, (uint32_t[6]){0,1,2,1,2,3}, 6, colour);
}

void BOB_draw_unfilled_quad(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, float depth) {
    BOB_Vector2 tl = {quad.x,          quad.y};
    BOB_Vector2 tr = {quad.x + quad.w, quad.y};
    BOB_Vector2 bl = {quad.x,          quad.y + quad.h};
    BOB_Vector2 br = {quad.x + quad.w, quad.y + quad.h};

    BOB_draw_line(r, tl, tr, thickness, colour, depth);
    BOB_draw_line(r, tr, br, thickness, colour, depth);
    BOB_draw_line(r, br, bl, thickness, colour, depth);
    BOB_draw_line(r, bl, tl, thickness, colour, depth);
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
                BOB_PRINT("Exceeded new polygon point capacity\n");
                break;
            }
            //Only second point is added
            new_points[new_poly_size++] = end;
        }
        else if(!start_inside && end_inside) {
            if(new_poly_size+1 >= BOBi_MAX_POLY_SIZE) {
                BOB_PRINT("Exceeded new polygon point capacity\n");
                break;
            }
            //Point of intersection with edge and second point is added
            new_points[new_poly_size++] = BOBi_get_intersection(start, end, edge, value);
            new_points[new_poly_size++] = end;
        }
        //When only second point is outside
        else if(start_inside && !end_inside) {
            if(new_poly_size >= BOBi_MAX_POLY_SIZE) {
                BOB_PRINT("Exceeded new polygon point capacity\n");
                break;
            }
            //Only point of intersection with edge is added
            new_points[new_poly_size++] = BOBi_get_intersection(start, end, edge, value);
        }
        //When both points are outside, no points are added
    }
    BOB_MEMCPY(poly_points, new_points, new_poly_size * sizeof(BOB_Vector2));
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

void BOB_draw_polygon(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float depth) {
    BOB_Vector2 points[BOBi_MAX_POLY_SIZE];
    BOB_MEMCPY(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points, poly_size);
    if(clipped_size < 3) return;

    uint32_t triangle_indices[(BOBi_MAX_POLY_SIZE - 2) * 3]; //Ear clipping always produces n-2 triangles for a polygon with n vertices
    size_t triangle_count = BOBi_triangulate_ec(points, clipped_size, triangle_indices);

    if(!triangle_count) return;

    //Processing the returned vertex data into a more compact form so we can pass it to the renderer
    uint32_t vertex_map[BOBi_MAX_POLY_SIZE];

    //Filling the map with dummy values
    for(size_t i = 0; i < clipped_size; i++)
        vertex_map[i] = UINT32_MAX;

    BOB_Vector3 vertices[BOBi_MAX_POLY_SIZE]; //Holds the compressed vertex values
    size_t vertex_count = 0;

    //Copying the old verticies into compressed format
    for(size_t i = 0; i < triangle_count*3; i++) {
        uint32_t old = triangle_indices[i];
        if(vertex_map[old] == UINT32_MAX) {
            vertex_map[old] = vertex_count;
            vertices[vertex_count++] = (BOB_Vector3){points[old].x, points[old].y, depth};
        }
        triangle_indices[i] = vertex_map[old];
    }

    BOBi_draw_mesh(r, vertices, vertex_count, triangle_indices, triangle_count * 3, colour);
}

//Draws an unfilled polygon
void BOB_draw_unfilled_polygon(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, float thickness, float depth) {
    BOB_Vector2 points[BOBi_MAX_POLY_SIZE];
    BOB_MEMCPY(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points, poly_size);
    if(clipped_size < 2) return;

    for(size_t i = 0; i < clipped_size; i++) {
        size_t next = (i+1) % clipped_size;
        BOB_draw_line(r, points[i], points[next], thickness, colour, depth);
    }
}


void BOB_draw_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, float depth) {
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
    BOB_MEMCPY(points2, vertices, vertex_count * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points2, vertex_count);
    if(clipped_size < 3) return;

    //Generating the indecies for the triangle ebo
    for(int i = 0; i < clipped_size; i++) {
        points3[i] = (BOB_Vector3){points2[i].x, points2[i].y, depth};
        indices[index_count++] = 0;
        indices[index_count++] = i;
        indices[index_count++] = ((i+1) % clipped_size);
    }

    BOBi_draw_mesh(r, points3, clipped_size, indices, index_count, colour);
}

void BOB_draw_unfilled_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, float thickness, BOB_Vector4 colour, float depth) {
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
        BOB_draw_line(r, vertices[i], vertices[next], thickness, colour, depth);
    }
}

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_start_clip(BOB_Renderer *r, BOB_Quad rect, BOB_Clip_Dir dir) {
    BOBi_Clip_Stack *stack = r->stack;
    if(stack->size >= stack->capacity) {
        size_t newCap = (stack->capacity == 0) ? 4 : stack->capacity * 2;
        BOBi_Clip_Rect* temp = BOB_MALLOC(sizeof(BOBi_Clip_Rect) * newCap);
        BOB_MEMCPY(temp, stack->elems, sizeof(BOBi_Clip_Rect) * stack->capacity);
        BOB_FREE(stack->elems);

        stack->elems = temp;
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

uint8_t BOB_bitmap_font_init(BOB_Bitmap_Font *opts, uint32_t atls, uint32_t tpw, uint32_t tph, uint32_t cpw, uint32_t cph, uint32_t cpx, uint32_t cpy, uint32_t tbpx, uint32_t tbpy, BOB_Bitmap_Layout lyt, BOB_Bitmap_Layout_Desc desc) {
    if(!opts) return 0;

    opts->atlas = atls;
    opts->tex_pixel_width = tpw;
    opts->tex_pixel_height = tph;
    opts->char_pixel_width = cpw;
    opts->char_pixel_height = cph;

    opts->char_padding_x = cpx; //Horizontal padding between chars on the atlas
    opts->char_padding_y = cpy; //Vertical padding between chars on the atlas
    opts->tex_border_padding_x = tbpx;
    opts->tex_border_padding_y = tbpy;

    opts->layout = lyt;
    opts->desc = desc;

    return 1;
}
void BOB_bitmap_font_free(BOB_Bitmap_Font *bf) {
    // glDeleteTextures(1, &bf->tex); //TODO: Replace with some render function to remove an atlas
    // if(bf->layout == BITMAP_CUSTOM) {
    //     free(bf->desc.custom_desc.data);
    //     bf->desc.custom_desc.data = NULL;
    // }
}

uint8_t BOB_draw_char(BOB_Renderer *r, BOB_Bitmap_Font *bf, char c, BOB_Quad dimensions, BOB_Vector4 colour, float depth) {
    BOB_ASSERT(bf->layout < BOB_NUM_BITMAP_LAYOUTS && "Invalid Bitmap layout");

    if(c < 32 || c > 126) {
        BOB_PRINT("Trying to draw a non-ASCII character number: %hhu: %c\n", (uint8_t)c, c);
        return 0;
    }
    int index = -1;

    switch(bf->layout) {
        case BOB_BITMAP_STANDARD:
            index = c - 32;
            if(index < bf->desc.offset_desc.start_offset || index > (96 - bf->desc.offset_desc.end_offset)) {
                BOB_PRINT("Char %c is not in the range specified by the offsets (%zu, %zu)\n", c, bf->desc.offset_desc.start_offset, bf->desc.offset_desc.end_offset);
                return 0;
            }
            index -= bf->desc.offset_desc.start_offset;
        break;
        case BOB_BITMAP_OFFSET:
            //This is fine. There's only going to be a maximum of 96
            //elements in this array. There won't be a noticeable performance drop
            for(int i = 0; i < bf->desc.custom_desc.len; i++) {
                if(bf->desc.custom_desc.data[i] == c){
                    index = i;
                    break;
                }
            }

            if(index == -1) {
                BOB_PRINT("The character \'%c\' was not specified in the bitmap's custom charset\n", c);
                return 0;
            }
        break;
        case BOB_BITMAP_CUSTOM:
            index = c-32;
            if(index < 0 || index > 94) {
                BOB_PRINT("Trying to draw a non-ASCII character\n");
                return 0;
            }
        break;
        default:
        break;
    }
    size_t img_width_chars = (bf->tex_pixel_width + bf->char_padding_x - (bf->tex_border_padding_x * 2)) / (bf->char_pixel_width + bf->char_padding_x);

    uint32_t x_tiles = index % img_width_chars;
    uint32_t y_tiles = index / img_width_chars;

    uint32_t x_pixels = x_tiles * (bf->char_pixel_width + bf->char_padding_x) + bf->tex_border_padding_x;
    uint32_t y_pixels = y_tiles * (bf->char_pixel_height + bf->char_padding_y) + bf->tex_border_padding_y;

    BOB_draw_atlas_quad(r, dimensions, (BOB_Quad){x_pixels, y_pixels, bf->char_pixel_width, bf->char_pixel_height}, colour, bf->atlas, depth);

    return 1;
}

uint8_t BOB_draw_string(BOB_Renderer *r, BOB_Bitmap_Font *bf, const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 start, BOB_Vector2 scale, BOB_Vector4 colour, float depth) {
    float x = start.x;
    float y = start.y;

    for(size_t i = 0; i < str_len; i++) {
        switch (str[i]) {
            case '\n':
                x = start.x;
                y += scale.y + gap.y;
            continue;
            case '\t':
                x += (scale.x + gap.x) * 4;
            continue;
            case ' ':
                x += scale.x + gap.x;
            continue;
            default:
            break;
        }

        if(!BOB_draw_char(r, bf, str[i], (BOB_Quad){x, y, scale.x, scale.y}, colour, depth))
            return 0;

        x += scale.x + gap.x;
    }

    return 1;
}

BOB_Vector2 bitmap_measure_text(const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 scale) {
    float max_w = 0;
    float h = scale.y + gap.y;
    float cur_w = 0;

    for(int i = 0; i < str_len; i++) {
        switch(str[i]) {
            case '\n':
                if(cur_w > max_w) max_w = cur_w;
                cur_w = 0;
                h += scale.y + gap.y;
            continue;
            case '\t':
                cur_w += (scale.x + gap.x) * 4;
            continue;
            default:
            break;
        }

        if(str[i] < 32 || str[i] > 126) {
            BOB_PRINT("Measuring text with non-printable ASCII characters\n");
            return (BOB_Vector2){-1, -1};
        }
        cur_w += scale.x + gap.x;
    }

    if(cur_w > max_w) max_w = cur_w;

    return (BOB_Vector2){max_w, h};
}

