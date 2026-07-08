#include "bob.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>

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
                            "layout (location = 0) in vec2 aPos;\n"
                            "layout (location = 1) in vec4 aColor;\n"
                            "layout (location = 2) in vec2 aTexCoord;\n"
                            "uniform mat4 uProjection;\n"
                            "out vec4 ourColor;\n"
                            "out vec2 TexCoord;\n"
                            "void main() {\n"
                            "    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);\n"
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
    glBufferData(GL_ARRAY_BUFFER, BOB_INIT_VERTEX_CAPACITY * sizeof(BOB_Render_Vertex), NULL, GL_DYNAMIC_DRAW);

    //Getting the ebo
    glGenBuffers(1, &r.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, pos)); //Vertex Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, colour)); //Vertex Colour
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, uv)); //UV
    glEnableVertexAttribArray(2);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Setting the projection matrix
    BOB_ortho(0.0f, r.screen_width, r.screen_height, 0.0f, -1.0f, 1.0f, r.projection);

    r.num_atlas_batches = 0;
    r.atlas_batch_capacity = 2;
    for(int i = 0; i < BOB_MAX_LAYERS; i++) {
        r.layers[i].atlas_batches = calloc(r.atlas_batch_capacity, sizeof(BOB_AtlasRenderBatch));
        r.layers[i].earliest_atlas_used = -1;
        r.layers[i].dynamic_texture_capacity = 8;
        r.layers[i].dynamic_textures = NULL;
        r.layers[i].dynamic_texture_count = 0;
    }

    //Make the special debug atlas as the first one
    BOB_TextureAtlas *ta = BOB_MALLOC(sizeof(BOB_TextureAtlas));
    *ta = BOB_atlas_init_blank(1, 1, 4);
    uint8_t white[4] = {255, 255, 255, 255};
    BOB_atlas_pack(ta, white, 1, 1, 4);

    BOB_add_texture_atlas(&r, ta);

    //Initialise the stack of clip rects
    r.stack = BOB_MALLOC(sizeof(BOB_Clip_Stack));
    r.stack->elems = BOB_MALLOC(sizeof(BOB_Clip_Rect) * INIT_STACK_CAPACITY);
    r.stack->capacity = INIT_STACK_CAPACITY;
    r.stack->size = 0;

    return r;
}

//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r) {
    glDeleteBuffers(1, &r->vbo);
    glDeleteVertexArrays(1, &r->vao);
    glDeleteProgram(r->shader);
    for(int i = 0; i < BOB_MAX_LAYERS; i++) {
        for(int j = 0; j < r->num_atlas_batches; j++) {
            BOB_FREE(BOB_GET_ATLAS_BATCH(r, i, j).vertex_data);
            BOB_FREE(BOB_GET_ATLAS_BATCH(r, i, j).index_data);
            BOB_atlas_free(BOB_GET_ATLAS_BATCH(r, i, j).a);
        }
        if(i == 0) BOB_FREE(BOB_GET_ATLAS_BATCH(r, i, 0).a);
        if(r->layers[i].atlas_batches) BOB_FREE(r->layers[i].atlas_batches);
        if(r->layers[i].dynamic_textures) BOB_FREE(r->layers[i].dynamic_textures);
    }

    BOB_FREE(r->stack->elems);
    r->stack->elems = NULL;
    BOB_FREE(r->stack);
    r->stack = NULL;
}

//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r) {
    for(int i = 0; i < BOB_MAX_LAYERS; i++) {
        for(int j = 0; j < r->num_atlas_batches; j++) {
            BOB_GET_ATLAS_BATCH(r, i, j).index_count = 0;
            BOB_GET_ATLAS_BATCH(r, i, j).vertex_count = 0;
        }
        r->layers[i].dynamic_texture_count = 0;
        r->layers[i].earliest_atlas_used = -1;
    }
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

    for(int i = 0; i < BOB_MAX_LAYERS; i++) {
        for(int j = 0; j < r->layers[i].dynamic_texture_count; j++) {
            BOB_DynamicTexture tex = r->layers[i].dynamic_textures[j];

            // Bind the texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex.texture);
            glUniform1i(glGetUniformLocation(r->shader, "screenTexture"), 0);

            // Build a single quad directly into a temporary buffer
            BOB_Render_Vertex verts[4] = {
                {{tex.dimensions.x, tex.dimensions.y}, tex.colour, {tex.uv.x, tex.uv.y}},
                {{tex.dimensions.x, tex.dimensions.y + tex.dimensions.h}, tex.colour, {tex.uv.x, tex.uv.y+tex.uv.h}},
                {{tex.dimensions.x + tex.dimensions.w, tex.dimensions.y + tex.dimensions.h}, tex.colour, {tex.uv.x + tex.uv.w, tex.uv.y + tex.uv.h}},
                {{tex.dimensions.x + tex.dimensions.w, tex.dimensions.y}, tex.colour, {tex.uv.x + tex.uv.w, tex.uv.y}},
            };
            uint32_t indices[] = { 0, 1, 3, 1, 2, 3 };

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        if(r->layers[i].earliest_atlas_used < 0) continue;
        for(int j = r->layers[i].earliest_atlas_used; j < r->num_atlas_batches; j++) {
            //TODO: Check if not redefining the buffer sizes using glBufferData
            //and then passing in arrays with a larger size will cause a crash,
            //and if so, how to get around it. Irrelevant for now
            if(BOB_GET_ATLAS_BATCH(r, i, j).index_count > 0) {
                glBufferSubData(GL_ARRAY_BUFFER, 0, BOB_GET_ATLAS_BATCH(r, i, j).vertex_count * sizeof(BOB_Render_Vertex), BOB_GET_ATLAS_BATCH(r, i, j).vertex_data); //Copies the data from renderer's triangle data into the vbo
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, BOB_GET_ATLAS_BATCH(r, i, j).index_count * sizeof(uint32_t), BOB_GET_ATLAS_BATCH(r, i, j).index_data); //Copies the quad data into the vbo

                //Bind the atlas texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, BOB_GET_ATLAS_BATCH(r, i, j).a->texture);
                glUniform1i(glGetUniformLocation(r->shader, "screenTexture"), 0);

                glDrawElements(GL_TRIANGLES, BOB_GET_ATLAS_BATCH(r, i, j).index_count, GL_UNSIGNED_INT, 0); //Make the draw call
            }
        }
    }
}

void BOBi_next_atlas_batch(BOB_Renderer *r, uint32_t atlas, uint8_t layer) {
    if(BOB_GET_ATLAS_BATCH(r, atlas, layer).index_count + BOB_INDECIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, atlas, layer).index_size) {
        size_t new_cap = BOB_GET_ATLAS_BATCH(r, atlas, layer).index_size * 2;
        uint32_t *temp = calloc(new_cap, sizeof(uint32_t));
        BOB_MEMCPY(temp, BOB_GET_ATLAS_BATCH(r, atlas, layer).index_data, sizeof(uint32_t) * BOB_GET_ATLAS_BATCH(r, atlas, layer).index_size);
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, atlas, layer).index_data);
        BOB_GET_ATLAS_BATCH(r, atlas, layer).index_data = temp;
        BOB_GET_ATLAS_BATCH(r, atlas, layer).index_size = new_cap;
    }
    if(BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_count + BOB_VERTICIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_size) {
        size_t new_cap = BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_size * 2;
        BOB_Render_Vertex *temp = calloc(new_cap, sizeof(BOB_Render_Vertex));
        BOB_MEMCPY(temp, BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_data, sizeof(BOB_Render_Vertex) * BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_size);
        BOB_FREE(BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_data);
        BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_data = temp;
        BOB_GET_ATLAS_BATCH(r, atlas, layer).vertex_size = new_cap;
    }
}

uint32_t BOB_add_texture_atlas(BOB_Renderer *r, BOB_TextureAtlas *ta) {
    if(r->num_atlas_batches >= r->atlas_batch_capacity) {
        size_t newCap = r->atlas_batch_capacity * 2;
        for(int i = 0; i < BOB_MAX_LAYERS; i++) {
            BOB_AtlasRenderBatch *temp = BOB_MALLOC(newCap * sizeof(BOB_AtlasRenderBatch));
            BOB_MEMCPY(temp, r->layers[i].atlas_batches, r->num_atlas_batches * sizeof(BOB_AtlasRenderBatch));
            BOB_FREE(r->layers[i].atlas_batches);
            r->layers[i].atlas_batches = temp;
            r->atlas_batch_capacity = newCap;
        }
    }

    for(int i = 0; i < BOB_MAX_LAYERS; i++) {
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).index_count = 0;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).vertex_count = 0;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).index_size = BOB_INIT_INDEX_CAPACITY;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).vertex_size = BOB_INIT_VERTEX_CAPACITY;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).index_data = NULL;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).vertex_data = NULL;
        BOB_GET_ATLAS_BATCH(r, i, r->num_atlas_batches).a = ta;
    }

    r->num_atlas_batches++;

    return r->num_atlas_batches - 1;
}

//Helper function to clip a quad
uint8_t BOBi_clip_quad(BOB_Renderer *r, BOB_Quad *quad) {
    if(r->stack->size == 0) return 1; //No need to clip if no clip rects
    BOB_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
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
uint8_t BOBi_line_outcode(BOB_Vector2* point, BOB_Clip_Rect clip) {
    uint8_t code = 0;

    if(point->x < clip.left) code |= 1; //Left
    else if(point->x > clip.right) code |= 2; //Right

    if(point->y < clip.top) code |= 4; //Top
    else if(point->y > clip.bottom) code |= 8; //Bottom

    return code;
}

uint8_t BOBi_clip_line(BOB_Renderer *r, BOB_Vector2 *start, BOB_Vector2* end) {
    if(r->stack->size == 0) return 1; //No need to clip if no clip rects
    BOB_Clip_Rect clip = BOB_peek_clip_rect(r->stack);
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
void BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint32_t atlas, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    if(!BOBi_clip_quad(r, &screen_quad)) return;

    //If we have overreached our current rendering limit or we cannot store any more textures, end the current draw call and start a new one
    if(BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_count + BOB_VERTICIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_size ||
        BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count + BOB_INDECIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, layer, atlas).index_size) {
        BOBi_next_atlas_batch(r, layer, atlas);
    }

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_count;

    BOB_Vector2 coords[4] = {
        {screen_quad.x, screen_quad.y},
        {screen_quad.x, screen_quad.y + screen_quad.h},
        {screen_quad.x + screen_quad.w, screen_quad.y + screen_quad.h},
        {screen_quad.x + screen_quad.w , screen_quad.y}
    };

    float width = BOB_GET_ATLAS_BATCH(r, layer, atlas).a->width;
    float height = BOB_GET_ATLAS_BATCH(r, layer, atlas).a->height;

    BOB_Vector2 uv[4] = {
        {tex_sub_rect.x / width, tex_sub_rect.y / height},
        {tex_sub_rect.x / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width , tex_sub_rect.y / height}
    };

    //Update the earliest atlas used
    if(r->layers[layer].earliest_atlas_used < 0 || r->layers[layer].earliest_atlas_used < atlas)
        r->layers[layer].earliest_atlas_used = atlas;

    //Lazy allocation of memory
    if(BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_data == NULL || BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data == NULL) {
        BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    for(int i = 0; i < BOB_VERTICIES_PER_QUAD; i++) {
        BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).vertex_count++] = (BOB_Render_Vertex){coords[i], colour, uv[i]};
    }

    //Need to also add ebo data so we can remove overlapping vertices
    //First triangle
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index;
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index + 3;

    //Second triangle
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index + 2;
    BOB_GET_ATLAS_BATCH(r, layer, atlas).index_data[BOB_GET_ATLAS_BATCH(r, layer, atlas).index_count++] = base_index + 3;
}

void BOB_draw_texture(BOB_Renderer *r, uint32_t texture, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    if(!BOBi_clip_quad(r, &dimensions)) return;

    //Lazy initialisation
    if(r->layers[layer].dynamic_textures == NULL) {
        r->layers[layer].dynamic_textures = BOB_MALLOC(r->layers[layer].dynamic_texture_capacity * sizeof(BOB_DynamicTexture));
    }

    if(r->layers[layer].dynamic_texture_count >= r->layers[layer].dynamic_texture_capacity) {
        size_t new_cap = r->layers[layer].dynamic_texture_capacity * 2;
        BOB_DynamicTexture *temp = BOB_MALLOC(new_cap * sizeof(BOB_DynamicTexture));
        BOB_MEMCPY(temp, r->layers[layer].dynamic_textures, r->layers[layer].dynamic_texture_capacity * sizeof(BOB_DynamicTexture));
        BOB_FREE(r->layers[layer].dynamic_textures);
        r->layers[layer].dynamic_textures = temp;
        r->layers[layer].dynamic_texture_capacity = new_cap;
    }

    r->layers[layer].dynamic_textures[r->layers[layer].dynamic_texture_count++] = (BOB_DynamicTexture){texture, dimensions, uv_dimensions, colour};
}

void BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer *pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint8_t layer) {
    if(!BOBi_clip_quad(r, &dimensions)) return;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pb->pbo);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, pb->pixel_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pb->width, pb->height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    BOB_draw_texture(r, pb->pixel_tex, dimensions, uv_dimensions, colour, layer);
}

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
BOB_PixelBuffer BOB_pixelbuffer_init(size_t width, size_t height, uint8_t pixel_size) {
    //TODO: Find some way to check the pixel formats properly
    if(pixel_size != 3 && pixel_size != 4) {
        BOB_PRINT(stderr, "Invalid pixel size\n");
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

void BOBi_draw_triangle_strip(BOB_Renderer *r, BOB_Vector2 strip[4], BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    if(BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count + BOB_VERTICIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_size ||
        BOB_GET_ATLAS_BATCH(r, layer, 0).index_count + BOB_INDECIES_PER_QUAD >= BOB_GET_ATLAS_BATCH(r, layer, 0).index_size) {
        BOBi_next_atlas_batch(r, layer, 0);
    }

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count;

    //Update the earliest atlas used
    if(r->layers[layer].earliest_atlas_used != 0)
        r->layers[layer].earliest_atlas_used = 0;

    //Lazy allocation of memory
    if(BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data == NULL || BOB_GET_ATLAS_BATCH(r, layer, 0).index_data == NULL) {
        BOB_GET_ATLAS_BATCH(r, layer, 0).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    for(int i = 0; i < BOB_VERTICIES_PER_QUAD; i++) {
        BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data[BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count++] = (BOB_Render_Vertex){strip[i], colour};
    }

    //Need to also add ebo data so we can remove overlapping vertices
    //First triangle
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index;
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index + 2;

    //Second triangle
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index + 1;
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index + 2;
    BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = base_index + 3;
}

void BOB_draw_line(BOB_Renderer *r, BOB_Vector2 start_pos, BOB_Vector2 end_pos, float thickness, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    if(!BOBi_clip_line(r, &start_pos, &end_pos)) return;

    BOB_Vector2 delta = {end_pos.x - start_pos.x, end_pos.y - start_pos.y};
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if(length > 0 && thickness > 0) {
        float scale = thickness/(2*length);

        BOB_Vector2 radius = {-scale*delta.y, scale*delta.x};
        BOB_Vector2 strip[4] = {
            {start_pos.x - radius.x, start_pos.y - radius.y},
            {start_pos.x + radius.x, start_pos.y + radius.y},
            {end_pos.x - radius.x, end_pos.y - radius.y},
            {end_pos.x + radius.x, end_pos.y + radius.y},
        };

        BOBi_draw_triangle_strip(r, strip, colour, layer);
    }
}

void BOB_draw_quad(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    if(!BOBi_clip_quad(r, &quad)) return;

    BOB_Vector2 strip[4] = {
        {quad.x, quad.y},
        {quad.x, quad.y+quad.h},
        {quad.x+quad.w, quad.y},
        {quad.x+quad.w, quad.y+quad.h},
    };

    BOBi_draw_triangle_strip(r, strip, colour, layer);
}

void BOB_draw_quad_bordered(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 q_col, BOB_Vector4 b_col, float thick, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    BOB_draw_quad(r, quad, q_col, layer);
    BOB_draw_unfilled_quad(r, quad, thick, b_col, layer);
}

void BOB_draw_unfilled_quad(BOB_Renderer *r, BOB_Quad quad, float thickness, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    BOB_Vector2 tl = {quad.x,          quad.y};
    BOB_Vector2 tr = {quad.x + quad.w, quad.y};
    BOB_Vector2 bl = {quad.x,          quad.y + quad.h};
    BOB_Vector2 br = {quad.x + quad.w, quad.y + quad.h};

    BOB_draw_line(r, tl, tr, thickness, colour, layer);
    BOB_draw_line(r, tr, br, thickness, colour, layer);
    BOB_draw_line(r, br, bl, thickness, colour, layer);
    BOB_draw_line(r, bl, tl, thickness, colour, layer);
}

//TODO: Figure out how to do clipping with circles
void BOB_draw_circle(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

    //TODO:Change this so we at least draw some of the triangles this batch and the rest in the next one
    if(BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count + BOB_CIRCLE_LINE_SEGMENTS + 1 >= BOB_INIT_VERTEX_CAPACITY || BOB_GET_ATLAS_BATCH(r, layer, 0).index_count + (BOB_CIRCLE_LINE_SEGMENTS * 3) >= BOB_INIT_INDEX_CAPACITY) {
        BOBi_next_atlas_batch(r, layer, 0);
    }

    uint32_t center_index = BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count;

    //Update the earliest atlas used
    if(r->layers[layer].earliest_atlas_used != 0)
        r->layers[layer].earliest_atlas_used = 0;

    //Lazy allocation of memory
    if(BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data == NULL || BOB_GET_ATLAS_BATCH(r, layer, 0).index_data == NULL) {
        BOB_GET_ATLAS_BATCH(r, layer, 0).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data[BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count++] = (BOB_Render_Vertex){centre, colour};

    float angle_step = 2.0f * M_PI / BOB_CIRCLE_LINE_SEGMENTS;
    uint32_t ring_start = BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count;

    //Generating the vertices for the triangles that make up a circle
    for(int i = 0; i < BOB_CIRCLE_LINE_SEGMENTS; i++) {
        float angle = i * angle_step;
        float x = centre.x + cosf(angle) * radius;
        float y = centre.y - sinf(angle) * radius;

        BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_data[BOB_GET_ATLAS_BATCH(r, layer, 0).vertex_count++] = (BOB_Render_Vertex){(BOB_Vector2){x, y}, colour};
    }

    //Generating the indecies for the triangle ebo
    for(int i = 0; i < BOB_CIRCLE_LINE_SEGMENTS; i++) {
        uint32_t current = ring_start + i;
        uint32_t next = ring_start + ((i+1) % BOB_CIRCLE_LINE_SEGMENTS);

        BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = center_index;
        BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = current;
        BOB_GET_ATLAS_BATCH(r, layer, 0).index_data[BOB_GET_ATLAS_BATCH(r, layer, 0).index_count++] = next;
    }
}

//Updates the current clipping rect by pushing the intersection of the new clipping region
//with the old clipping regions to the front of the stack but maintains the clipping directions
//specified in the original rect
void BOB_push_clip_rect(BOB_Clip_Stack *stack, BOB_Clip_Rect rect) {
    if(stack->size >= stack->capacity) {
        size_t newCap = (stack->capacity == 0) ? 4 : stack->capacity * 2;
        BOB_Clip_Rect* temp = BOB_MALLOC(sizeof(BOB_Clip_Rect) * newCap);
        BOB_MEMCPY(temp, stack->elems, sizeof(BOB_Clip_Rect) * stack->capacity);
        BOB_FREE(stack->elems);

        stack->elems = temp;
        stack->capacity = newCap;
    }

    //Getting the intersection of the old and current rect
    if(stack->size > 0) {
        BOB_Clip_Rect old_inter = stack->elems[stack->size-1];
        //Early return if the previous rect was empty
        if(old_inter.empty) {
            rect.empty = 1;
            stack->elems[stack->size++] = rect;
            return;
        }

        if(rect.clip_horz && old_inter.clip_horz) {
            rect.left = (rect.left > old_inter.left) ? rect.left : old_inter.left;
            rect.right = (rect.right < old_inter.right) ? rect.right : old_inter.right;
        }
        else if(old_inter.clip_horz) {
            rect.left = old_inter.left;
            rect.right = old_inter.right;
        }

        if(rect.clip_vert && old_inter.clip_vert) {
            rect.top = (rect.top > old_inter.top) ? rect.top : old_inter.top;
            rect.bottom = (rect.bottom < old_inter.bottom) ? rect.bottom : old_inter.bottom;
        }
        else if(old_inter.clip_vert) {
            rect.top = old_inter.top;
            rect.bottom = old_inter.bottom;
        }

        //Update the clipping directions
        rect.clip_horz |= old_inter.clip_horz;
        rect.clip_vert |= old_inter.clip_vert;
    }

    //Check if the rect is empty
    rect.empty = (rect.left <= rect.right || rect.top <= rect.bottom || (!rect.clip_horz && !rect.clip_vert)) ? 1 : 0;

    stack->elems[stack->size++] = rect;
}

//Removes the first clipping intersection from the stack and returns its value
BOB_Clip_Rect BOB_pop_clip_rect(BOB_Clip_Stack* stack) {
    BOB_ASSERT(stack->size > 0 && "Popping an empty stack");

    BOB_Clip_Rect rect = stack->elems[stack->size-1];
    stack->size--;
    return rect;
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

uint8_t BOB_draw_char(BOB_Renderer *r, BOB_Bitmap_Font *bf, char c, BOB_Quad dimensions, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");
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

    BOB_draw_atlas_quad(r, dimensions, (BOB_Quad){x_pixels, y_pixels, bf->char_pixel_width, bf->char_pixel_height}, colour, bf->atlas, layer);

    return 1;
}

uint8_t BOB_draw_string(BOB_Renderer *r, BOB_Bitmap_Font *bf, const char *str, size_t str_len, BOB_Vector2 gap, BOB_Vector2 start, BOB_Vector2 scale, BOB_Vector4 colour, uint8_t layer) {
    BOB_ASSERT(layer < BOB_MAX_LAYERS && "Invalid layer index\n");

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

        if(!BOB_draw_char(r, bf, str[i], (BOB_Quad){x, y, scale.x, scale.y}, colour, layer))
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

