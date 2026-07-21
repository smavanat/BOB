#include "bob.h"
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    BOB_RenderBatch batch_table[BOB_MAX_TEX_CAPACITY][BOB_MAX_MATERIAL_CAPACITY];
    BOB_TextureAtlas atlas_table[BOB_MAX_ATLAS_CAPACITY];
    BOB_PixelBuffer pixelbuffer_table[BOB_MAX_PIXELBUFFER_CAPACITY];
    BOB_Texture texture_table[BOB_MAX_TEX_CAPACITY];
    BOB_Material material_table[BOB_MAX_MATERIAL_CAPACITY];
    BOB_Font font_table[BOB_MAX_FONT_CAPACITY];
    size_t num_atlases;
    size_t num_textures;
    size_t num_pixelbuffers;
    size_t num_materials;
    size_t num_fonts;
    uint32_t next_atlas_slot;
    uint32_t next_tex_slot;
    uint32_t next_pixelbuf_slot;
    uint32_t next_mat_slot;
    uint32_t next_font_slot;
    BOB_Texture_Handle default_tex;
} BOBi_Data;

BOBi_Data intrn_data = {0};

//Return the value of the element at the top of the stack without popping it
#define BOB_peek_clip_rect(stack) (((stack)->size > 0) ? (stack)->elems[(stack)->size-1] : (BOBi_Clip_Rect){0})
#define BOBi_GET_RENDER_BATCH(tex, mat) intrn_data.batch_table[(tex)][(mat)]
#define BOBi_MSB 0x80000000

//================================================= INTERNAL HELPER FUNCTIONS ===================================================

void BOBi_update_uniform(BOB_Uniform uniform) {
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
        case BOB_UNIFORM_TEXTURE:
            glUniform1i(uniform.location, (uniform.is_reference) ? *(BOB_Texture_Handle *)uniform.ptr : intrn_data.texture_table[uniform.tex_index].texture);
            break;
        case BOB_UNIFORM_MAT4:
            glUniformMatrix4fv(uniform.location, 1, GL_FALSE, (uniform.is_reference) ? (float *)(*(BOB_Mat4 *)uniform.ptr).m : (float *)uniform.mat4.m);
            break;
    }
}

void BOBi_flush_batch(BOB_Renderer *r, BOB_Texture_Handle tex, BOB_Material_Handle mat) {
    glUseProgram(intrn_data.material_table[mat].shader);
    //Setting the uniforms
    for(size_t i = 0; i < intrn_data.material_table[mat].uniform_count; i++) {
        BOBi_update_uniform(intrn_data.material_table[mat].uniforms[i]);
    }

    //Bind all of the arrays and buffers we will reuse over time
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, BOBi_GET_RENDER_BATCH(tex, mat).vertex_count * sizeof(BOB_Render_Vertex), BOBi_GET_RENDER_BATCH(tex, mat).vertex_data); //Copies the data from renderer's triangle data into the vbo
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, BOBi_GET_RENDER_BATCH(tex, mat).index_count * sizeof(uint32_t), BOBi_GET_RENDER_BATCH(tex, mat).index_data); //Copies the quad data into the vbo

    //Bind the atlas texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, intrn_data.texture_table[tex].texture);

    glDrawElements(GL_TRIANGLES, BOBi_GET_RENDER_BATCH(tex, mat).index_count, GL_UNSIGNED_INT, 0); //Make the draw call

    BOBi_GET_RENDER_BATCH(tex, mat).index_count = 0;
    BOBi_GET_RENDER_BATCH(tex, mat).vertex_count = 0;
}

void BOBi_check_capacity(BOB_Renderer *r, BOB_Texture_Handle tex, BOB_Material_Handle mat, uint32_t num_vertices, uint32_t num_indices) {
    if(BOBi_GET_RENDER_BATCH(tex, mat).index_count + num_indices >= BOB_MAX_INDEX_CAPACITY ||
      BOBi_GET_RENDER_BATCH(tex, mat).vertex_count + num_vertices >= BOB_MAX_VERTEX_CAPACITY) {
        BOBi_flush_batch(r, tex, mat);
        return;
    }

    if(BOBi_GET_RENDER_BATCH(tex, mat).index_count + num_indices >= BOBi_GET_RENDER_BATCH(tex, mat).index_size) {
        size_t new_cap = BOBi_GET_RENDER_BATCH(tex, mat).index_size * 2;
        if(new_cap < BOBi_GET_RENDER_BATCH(tex, mat).index_count + num_indices) new_cap = BOBi_GET_RENDER_BATCH(tex, mat).index_count + num_indices;
        uint32_t *temp = BOB_MALLOC(new_cap * sizeof(uint32_t));
        BOB_MEMCPY(temp, BOBi_GET_RENDER_BATCH(tex, mat).index_data, sizeof(uint32_t) * BOBi_GET_RENDER_BATCH(tex, mat).index_size);
        BOB_FREE(BOBi_GET_RENDER_BATCH(tex, mat).index_data);
        BOBi_GET_RENDER_BATCH(tex, mat).index_data = temp;
        BOBi_GET_RENDER_BATCH(tex, mat).index_size = new_cap;
    }
    if(BOBi_GET_RENDER_BATCH(tex, mat).vertex_count + num_vertices >= BOBi_GET_RENDER_BATCH(tex, mat).vertex_size) {
        size_t new_cap = BOBi_GET_RENDER_BATCH(tex, mat).vertex_size * 2;
        if(new_cap < BOBi_GET_RENDER_BATCH(tex, mat).vertex_count + num_vertices) new_cap = BOBi_GET_RENDER_BATCH(tex, mat).vertex_count + num_vertices;
        BOB_Render_Vertex *temp = BOB_MALLOC(new_cap * sizeof(BOB_Render_Vertex));
        BOB_MEMCPY(temp, BOBi_GET_RENDER_BATCH(tex, mat).vertex_data, sizeof(BOB_Render_Vertex) * BOBi_GET_RENDER_BATCH(tex, mat).vertex_size);
        BOB_FREE(BOBi_GET_RENDER_BATCH(tex, mat).vertex_data);
        BOBi_GET_RENDER_BATCH(tex, mat).vertex_data = temp;
        BOBi_GET_RENDER_BATCH(tex, mat).vertex_size = new_cap;
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

uint32_t BOBi_convert_format(BOB_Format format) {
    switch (format) {
        case BOB_RED: return GL_RED;
        case BOB_RG: return GL_RG;
        case BOB_RGB: return GL_RGB;
        case BOB_RGBA: return GL_RGBA;
    }
}

//Compiles a shader from a source file given the desired shader type
unsigned int BOBi_create_shader(BOB_Shader_Data s) {
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
        BOB_PRINT("ERROR::SHADER::COMPILATION_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            BOB_PRINT("%c", infolog[i]);
        }
        BOB_PRINT("\n");
    }

    return shader;
}

//Draws a mesh of triangles
void BOBi_draw_mesh(BOB_Renderer *r, BOB_Vector3 *vertices, size_t vertex_count, uint32_t *indices, size_t index_count, BOB_Vector4 colour, BOB_Material_Handle mat) {
    //Lazy allocation of memory
    if(!BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).init) {
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).index_size = BOB_INIT_INDEX_CAPACITY;
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).vertex_size = BOB_INIT_VERTEX_CAPACITY;
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).init = 1;
    }

    BOBi_check_capacity(r, intrn_data.default_tex, mat, vertex_count, index_count);

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).vertex_count;

    for(size_t i = 0; i < vertex_count; i++) {
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).vertex_data[BOBi_GET_RENDER_BATCH(0,0).vertex_count++] = (BOB_Render_Vertex){colour, vertices[i]};
    }

    for(size_t i = 0; i < index_count; i++) {
        BOBi_GET_RENDER_BATCH(intrn_data.default_tex, mat).index_data[BOBi_GET_RENDER_BATCH(0,0).index_count++] = base_index + indices[i];
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

void BOBi_texture_free(BOB_Texture_Handle tex) {
    glDeleteTextures(1, &intrn_data.texture_table[tex].texture);
    intrn_data.texture_table[tex] = (BOB_Texture){0}; //Clear the data
}
void BOBi_pixelbuffer_free(BOB_PixelBuffer_Handle pb) {
    glDeleteBuffers(1, &intrn_data.pixelbuffer_table[pb].pbo);
    BOB_texture_free(&intrn_data.pixelbuffer_table[pb].pixel_tex);
    intrn_data.pixelbuffer_table[pb] = (BOB_PixelBuffer){0}; //Clear the data
}
void BOBi_material_free(BOB_Material_Handle mat) {
    glDeleteProgram(intrn_data.material_table[mat].shader);
    BOB_FREE(intrn_data.material_table[mat].uniforms);
    intrn_data.material_table[mat] = (BOB_Material){0}; //Clear the data
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

BOBi_Hashmap BOBi_hashmap_init(size_t init_capacity) {
    BOBi_Hashmap ret = {0};
    ret.capacity = BOBi_next_prime(init_capacity);
    ret.keys = BOB_MALLOC(sizeof(uint64_t) * ret.capacity);
    BOB_MEMSET(ret.keys, 0xFF, sizeof(uint64_t) * ret.capacity);
    ret.values = BOB_MALLOC(sizeof(uint32_t) * ret.capacity);
    BOB_MEMSET(ret.values, 0xFF, sizeof(uint32_t) * ret.capacity);

    return ret;
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
    h->keys = BOB_MALLOC(sizeof(uint64_t) * newCap);
    BOB_MEMSET(h->keys, 0xFF, sizeof(uint64_t) * newCap);
    h->values = BOB_MALLOC(sizeof(uint32_t) * newCap);
    BOB_MEMSET(h->values, 0xFF, sizeof(uint32_t) * newCap);

    //Rehash and reinsert all entries from the old table into the new one
    for(size_t i = 0; i < oldCap; i++) {
        if(oldKeys[i] != BOBi_HASHMAP_DUMMY) {
            BOBi_hashmap_add(h, oldKeys[i], oldVals[i]);
        }
    }
    BOB_FREE(oldKeys);
    BOB_FREE(oldVals);
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
    if(h->keys) BOB_FREE(h->keys);
    h->keys = NULL;
    if(h->values) BOB_FREE(h->values);
    h->values = NULL;
}

void BOBi_font_free(BOB_Font_Handle index) {
    if(intrn_data.font_table[index].glyphs) BOB_FREE(intrn_data.font_table[index].glyphs);
    if(intrn_data.font_table[index].kernings) BOB_FREE(intrn_data.font_table[index].kernings);
    if(intrn_data.font_table[index].glyph_map) {
        BOBi_hashmap_free(intrn_data.font_table[index].glyph_map);
        BOB_FREE(intrn_data.font_table[index].glyph_map);
    }
    if(intrn_data.font_table[index].kerning_map) {
        BOBi_hashmap_free(intrn_data.font_table[index].kerning_map);
        BOB_FREE(intrn_data.font_table[index].kerning_map);
    }
    intrn_data.font_table[index] = (BOB_Font){0}; //Clear the data
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

void BOB_clear_colour(BOB_Vector4 colour) {
    glClearColor(colour.x, colour.y, colour.z, colour.w);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //Need to clear the depth buffer as well
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

// ============================================= BOB STATE MANAGEMENT ============================================================

uint8_t BOB_init(GLADloadproc proc) {
    //Loading GLAD
    if(!gladLoadGLLoader(proc)) {
        printf("Failed to initialise GLAD");
        return 0;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);

    if(!BOB_create_texture(1, 1, (uint8_t[4]){255, 255, 255, 255}, BOB_RGBA, &intrn_data.default_tex)) return 0;
    intrn_data.next_atlas_slot = UINT32_MAX;
    intrn_data.next_tex_slot = UINT32_MAX;
    intrn_data.next_pixelbuf_slot = UINT32_MAX;
    intrn_data.next_mat_slot = UINT32_MAX;
    intrn_data.next_font_slot = UINT32_MAX;

    return 1;
}

void BOB_terminate() {
    for(size_t i = 0; i < BOB_MAX_TEX_CAPACITY; i++) {
        BOBi_texture_free(i);
    }
    for(size_t i = 0; i < BOB_MAX_MATERIAL_CAPACITY; i++) {
        BOBi_material_free(i);
    }
    for(size_t i = 0; i < BOB_MAX_TEX_CAPACITY; i++) {
        for(size_t j = 0; j < BOB_MAX_MATERIAL_CAPACITY; j++) {
            BOB_FREE(BOBi_GET_RENDER_BATCH(i, j).index_data);
            BOB_FREE(BOBi_GET_RENDER_BATCH(i, j).vertex_data);
        }
    }
    for(size_t i = 0; i < BOB_MAX_PIXELBUFFER_CAPACITY; i++) {
        BOBi_pixelbuffer_free(i);
    }
    for(size_t i = 0; i < BOB_MAX_FONT_CAPACITY; i++) {
        BOBi_font_free(i);
    }
}

//========================================================== RENDERER FUNCTIONS ===========================================

//Initialises the pixel renderer
uint8_t BOB_renderer_init(size_t width, size_t height, BOB_Renderer *out) {
    BOB_Renderer r = {0};
    r.screen_height = height;
    r.screen_width = width;

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

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, colour)); //Vertex Colour
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, pos)); //Vertex Position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BOB_Render_Vertex), (void *)offsetof(BOB_Render_Vertex, uv)); //UV
    glEnableVertexAttribArray(2);

    //Setting the projection matrix
    BOB_ortho(0.0f, r.screen_width, r.screen_height, 0.0f, -BOB_MAX_LAYER, 0.0f, &r.projection);
    if(!BOB_create_material((BOB_Shader_Data[2]){(BOB_Shader_Data){vertex_shader, BOB_VERTEX_SHADER},
                            (BOB_Shader_Data){fragment_shader, BOB_FRAGMENT_SHADER}}, 2,
                            (BOB_Uniform[2]){BOB_uniform_mat4("uProjection", r.projection), BOB_uniform_signed_int("screenTexture", 0)}, 2, &r.default_mat)) return 0;

    //Initialise the stack of clip rects
    r.stack = BOB_MALLOC(sizeof(BOBi_Clip_Stack));
    r.stack->elems = BOB_MALLOC(sizeof(BOBi_Clip_Rect) * INIT_STACK_CAPACITY);
    r.stack->capacity = INIT_STACK_CAPACITY;
    r.stack->size = 0;
    *out = r;

    return 1;
}

//Frees a pixel renderer
void BOB_renderer_free(BOB_Renderer *r) {
    glDeleteBuffers(1, &r->vbo);
    glDeleteVertexArrays(1, &r->vao);

    BOB_FREE(r->stack->elems);
    r->stack->elems = NULL;
    BOB_FREE(r->stack);
    r->stack = NULL;
}

//Sets up the variables for renderering to the pbo from the BOB_Renderer
void BOB_renderer_begin(BOB_Renderer *r) {
    for(int i = 0; i < BOB_MAX_TEX_CAPACITY; i++) {
        for(int j = 0; j < BOB_MAX_MATERIAL_CAPACITY; j++) {
            if(intrn_data.batch_table[i][j].init) {
                intrn_data.batch_table[i][j].index_count = 0;
                intrn_data.batch_table[i][j].vertex_count = 0;
            }
        }
    }
}

//Ends rendering to the current pixel frame
void BOB_renderer_end(BOB_Renderer *r) {
    //Bind all of the arrays and buffers we will reuse over time
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);
    for(int tex = 0; tex < BOB_MAX_TEX_CAPACITY; tex++) {
        for(int mat = 0; mat < BOB_MAX_MATERIAL_CAPACITY; mat++) {
            if(intrn_data.batch_table[tex][mat].init && intrn_data.batch_table[tex][mat].index_count > 0 && intrn_data.batch_table[tex][mat].vertex_count > 0) {
                glUseProgram(intrn_data.material_table[mat].shader);
                //Setting the uniforms
                for(size_t i = 0; i < intrn_data.material_table[mat].uniform_count; i++) {
                    BOBi_update_uniform(intrn_data.material_table[mat].uniforms[i]);
                }

                glBufferSubData(GL_ARRAY_BUFFER, 0, BOBi_GET_RENDER_BATCH(tex, mat).vertex_count * sizeof(BOB_Render_Vertex), BOBi_GET_RENDER_BATCH(tex, mat).vertex_data); //Copies the data from renderer's triangle data into the vbo
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, BOBi_GET_RENDER_BATCH(tex, mat).index_count * sizeof(uint32_t), BOBi_GET_RENDER_BATCH(tex, mat).index_data); //Copies the quad data into the vbo

                //Bind the atlas texture
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, intrn_data.texture_table[tex].texture);

                glDrawElements(GL_TRIANGLES, BOBi_GET_RENDER_BATCH(tex, mat).index_count, GL_UNSIGNED_INT, 0); //Make the draw call

                BOBi_GET_RENDER_BATCH(tex, mat).index_count = 0;
                BOBi_GET_RENDER_BATCH(tex, mat).vertex_count = 0;
            }
        }
    }
}

//Updates the dimensions of the screen that the renderer renders to.
//Updates projection matrix
//NOTE: Not 100% sure that this works
void BOB_renderer_update_dimensions(BOB_Renderer *r, uint32_t width, uint32_t height) {
    r->screen_width = width;
    r->screen_height = height;

    //Update projection matrix for renderer
    BOB_ortho(0.0f, width, height, 0.0f, -BOB_MAX_LAYER, 0.0f, &r->projection);
    BOB_set_material_mat4(r->default_mat, 0, r->projection);

    //Update the uv coordinates of the texture the renderer is rendering to
    float quadVertices[] = {
        0.0f, 0.0f,          0.0f, 0.0f,
        0.0f, height,        0.0f, 1.0f,
        width, height,       1.0f, 1.0f,
        width, 0.0f,         1.0f, 0.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVertices), quadVertices);
}

//================================================== TEXTURE FUNCTIONS ================================================

//Creates a new texture on the gpu
uint8_t BOB_create_texture(uint32_t width, uint32_t height, uint8_t *data, BOB_Format format, BOB_Texture_Handle *tex) {
    if(intrn_data.num_textures >= BOB_MAX_TEX_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded Texture Capacity");
        *tex |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_tex_slot == UINT32_MAX) {
        index = intrn_data.num_textures;
    }
    else {
        index = intrn_data.next_tex_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_data.next_tex_slot = index + 1; intrn_data.next_tex_slot < intrn_data.num_textures; intrn_data.next_tex_slot++) {
            if (!intrn_data.texture_table[intrn_data.next_tex_slot].init)
                break;
        }

        if (intrn_data.next_tex_slot >= intrn_data.num_textures)
            intrn_data.next_tex_slot = UINT32_MAX;
    }

    intrn_data.texture_table[index].init = 1; //Setting the value to be initialised
    glGenTextures(1, &intrn_data.texture_table[index].texture);
    glBindTexture(GL_TEXTURE_2D, intrn_data.texture_table[index].texture);
    // set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, BOBi_convert_format(format), width, height, 0, BOBi_convert_format(format), GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    intrn_data.texture_table[index].width = width;
    intrn_data.texture_table[index].height = height;

    intrn_data.num_textures++;
    *tex = index;
    return 1;
}

void BOB_texture_free(BOB_Texture_Handle *tex) {
    if(*(tex) & BOBi_MSB) return; //Do not work with already invalid handles
    *(tex) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle
    uint32_t index = *(tex) & (~BOBi_MSB);
    if(index < intrn_data.next_tex_slot) intrn_data.next_tex_slot = index;

    BOBi_texture_free(index);
}

BOB_Texture *BOB_get_tex_ref(BOB_Texture_Handle tex) {
    if(tex & BOBi_MSB) return NULL; //Do not work with already invalid handles
    uint32_t index = tex & (~BOBi_MSB);

    return &intrn_data.texture_table[index];
}

//====================================== MATERIAL FUNCTIONS ======================================

uint8_t get_uniform(BOB_Material_Handle mat, char *name, BOB_Uniform_Handle *uniform) {
    BOB_Material m = intrn_data.material_table[mat];
    for(size_t i = 0; i < m.uniform_count; i++) {
        if(!strcmp(name, m.uniforms[i].name)){
            *uniform = i;
            return 1;
        }
    }

    *uniform |= BOBi_MSB;
    return 0;
}

uint8_t BOB_create_material(BOB_Shader_Data *data, size_t num_shaders, BOB_Uniform *uniforms, size_t num_uniforms, BOB_Material_Handle *mat) {
    if(intrn_data.num_materials >= BOB_MAX_MATERIAL_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded Material Capacity\n");
        *mat |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_mat_slot == UINT32_MAX) {
        index = intrn_data.num_materials;
    }
    else {
        index = intrn_data.next_mat_slot;

        for (intrn_data.next_mat_slot = index + 1; intrn_data.next_mat_slot < intrn_data.num_materials; intrn_data.next_mat_slot++) {
            if (!intrn_data.material_table[intrn_data.next_mat_slot].init)
                break;
        }

        if (intrn_data.next_mat_slot >= intrn_data.num_materials)
            intrn_data.next_mat_slot = UINT32_MAX;
    }

    intrn_data.material_table[index].init = 1; //Setting the value to be initialised

    uint32_t shader_buf[num_shaders]; //Array to store the ids of the loaded shader sub-programs
    uint32_t s = glCreateProgram();

    //Attaching all of the shaders together
    for(int i = 0; i < num_shaders; i++) {
        shader_buf[i] = BOBi_create_shader(data[i]);
        glAttachShader(s, shader_buf[i]);
    }

    glLinkProgram(s);
    int result;
    char infolog[512];

    //Print errors if any:
    glGetProgramiv(s, GL_LINK_STATUS, &result);
    if(!result) {
        glGetProgramInfoLog(s, 512, NULL, infolog);
        BOB_PRINT("ERROR::SHADER::LINKING_FAILED\n");
        for(int i = 0; i < 512; i++){
            if(infolog[i] == '\0') break;
            BOB_PRINT("%c", infolog[i]);
        }
        BOB_PRINT("\n");
        *mat |= BOBi_MSB;
        return 0;
    }

    //Cleanup
    for(int i = 0; i < num_shaders; i++) {
        glDeleteShader(shader_buf[i]);
    }

    intrn_data.material_table[index] = (BOB_Material){.uniform_count = num_uniforms, .shader = s};

    //Setting the uniforms
    BOB_Uniform *temp = BOB_MALLOC(sizeof(BOB_Uniform) * num_uniforms);
    BOB_MEMCPY(temp, uniforms, num_uniforms * sizeof(BOB_Uniform));
    for(size_t i = 0; i < num_uniforms; i++) {
        temp[i].location = glGetUniformLocation(s, temp[i].name);
    }
    intrn_data.material_table[index].uniforms = temp;

    intrn_data.num_materials++;
    *mat = index;
    return 1;
}

void BOB_material_free(BOB_Material_Handle *mat) {
    if(*(mat) & BOBi_MSB) return; //Do not work with already invalid handles
    *(mat) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle
    uint32_t index = *(mat) & (~BOBi_MSB);
    if(index < intrn_data.next_mat_slot) intrn_data.next_mat_slot = index;

    BOBi_material_free(index);
}

BOB_Material *BOB_get_mat_ref(BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return NULL; //Do not work with already invalid handles
    uint32_t index = mat & (~BOBi_MSB);

    return &intrn_data.material_table[index];
}

uint8_t BOB_set_material_float(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        intrn_data.material_table[mat].uniforms[uniform].f = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        intrn_data.material_table[mat].uniforms[uniform].u32 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        intrn_data.material_table[mat].uniforms[uniform].i32 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector2(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        intrn_data.material_table[mat].uniforms[uniform].vec2 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector3(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        intrn_data.material_table[mat].uniforms[uniform].vec3 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_vector4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        intrn_data.material_table[mat].uniforms[uniform].vec4 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_texture(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_TEXTURE) {
        intrn_data.material_table[mat].uniforms[uniform].tex_index = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_mat4(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        intrn_data.material_table[mat].uniforms[uniform].mat4 = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 0;
    }
    return 1;
}
uint8_t BOB_set_material_float_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, float *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_FLOAT) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_unsigned_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, uint32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_UNSIGNED_INT) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_signed_int_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, int32_t *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_SIGNED_INT) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector2_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector2 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC2) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector3_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector3 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC3) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_vector4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Vector4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_VEC4) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_texture_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Texture_Handle *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_TEXTURE) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}
uint8_t BOB_set_material_mat4_ref(BOB_Material_Handle mat, BOB_Uniform_Handle uniform, BOB_Mat4 *value) {
    if(mat & BOBi_MSB || uniform & BOBi_MSB) return 0; //Do not work with already invalid handles
    if(intrn_data.material_table[mat].uniforms[uniform].type == BOB_UNIFORM_MAT4) {
        intrn_data.material_table[mat].uniforms[uniform].ptr = value;
        intrn_data.material_table[mat].uniforms[uniform].is_reference = 1;
    }
    return 1;
}

//================================================== TEXTURE ATLAS FUNCTIONS ========================================

//Initialises a texture atlas
uint8_t BOB_atlas_init(uint32_t width, uint32_t height, BOB_Format format, BOB_Atlas_Handle *a) {
    if(intrn_data.num_atlases >= BOB_MAX_ATLAS_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded Atlas Capacity");
        *a |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_atlas_slot == UINT32_MAX) {
        index = intrn_data.num_atlases;
    }
    else {
        index = intrn_data.next_atlas_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_data.next_atlas_slot = index + 1; intrn_data.next_atlas_slot < intrn_data.num_atlases; intrn_data.next_atlas_slot++) {
            if (!intrn_data.atlas_table[intrn_data.next_atlas_slot].init)
                break;
        }

        if (intrn_data.next_atlas_slot >= intrn_data.num_atlases)
            intrn_data.next_atlas_slot = UINT32_MAX;
    }

    intrn_data.atlas_table[index].init = 1; //Setting the value to be initialised
    intrn_data.atlas_table[index].format = format;
    if(!BOB_create_texture(width, height, NULL, format, &intrn_data.atlas_table[index].texture)) {
        intrn_data.atlas_table[index] = (BOB_TextureAtlas){0};
        *a |= BOBi_MSB;
        return 0;
    }

    intrn_data.num_atlases++;
    *a = index;
    return 1;
}

void BOB_atlas_free(BOB_Atlas_Handle *a) {
    if(*a & BOBi_MSB) return; //Do not work with already invalid handles
    *(a) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle
    uint32_t index = *(a) & (~BOBi_MSB);
    if(index < intrn_data.next_atlas_slot) intrn_data.next_atlas_slot = index;

    BOB_texture_free(&intrn_data.atlas_table[index].texture);
    intrn_data.atlas_table[index] = (BOB_TextureAtlas){0}; //Clear the data
}

BOB_TextureAtlas *BOB_get_atlas_ref(BOB_Atlas_Handle a) {
    if(a & BOBi_MSB) return NULL; //Do not work with already invalid handles
    uint32_t index = a & (~BOBi_MSB);

    return &intrn_data.atlas_table[index];
}

//Returns the UV rect where the texture was placed
//pixel_size must be either 3 or 4. If it does not match the pixel size of the atlas,
//an empty quad will returned as the pixel formats are different
uint8_t BOB_atlas_pack(BOB_Atlas_Handle a, uint8_t* pixels, size_t w, size_t h, BOB_Quad *out_quad) {
    if((a & BOBi_MSB) || (intrn_data.atlas_table[a].texture & BOBi_MSB)) return 0; //Do not work with already invalid handles
    BOB_Texture tex = intrn_data.texture_table[intrn_data.atlas_table[a].texture];
    if(intrn_data.atlas_table[a].cursor_y + h > tex.height) return 0; //Early exit if we can't fit the texture in

    //Move to next row if this texture doesn't fit
    if(intrn_data.atlas_table[a].cursor_x + w > tex.width) {
       intrn_data.atlas_table[a].cursor_y += intrn_data.atlas_table[a].row_height;
       intrn_data.atlas_table[a].cursor_x = 0;
       intrn_data.atlas_table[a].row_height = 0;
    }

    uint32_t tex_index = tex.texture;
    GLenum gl_format = BOBi_convert_format(intrn_data.atlas_table[a].format);

    //Upload the subregion
    glBindTexture(GL_TEXTURE_2D, tex_index);
    glTexSubImage2D(GL_TEXTURE_2D, 0, intrn_data.atlas_table[a].cursor_x, intrn_data.atlas_table[a].cursor_y, w, h, gl_format, GL_UNSIGNED_BYTE, pixels);

    //Compute normalised UVs
    BOB_Quad uv = {
        (float)intrn_data.atlas_table[a].cursor_x / tex.width,
        (float)intrn_data.atlas_table[a].cursor_y / tex.height,
        (float) w / tex.width,
        (float) h / tex.height
    };

    intrn_data.atlas_table[a].cursor_x += w;
    if(h > intrn_data.atlas_table[a].row_height) intrn_data.atlas_table[a].row_height = h;

    *out_quad = uv;
    return 1;
}

//======================================================= PIXELBUFFER FUNCTIONS ==============================================

//Creates a pixel buffer to hold the pixels representing
//a texture of size width * height
//Pixel size should be either 3 or 4 (rgb/rgba)
uint8_t BOB_pixelbuffer_init(size_t width, size_t height, BOB_Format format, BOB_PixelBuffer_Handle *pb) {
    if(intrn_data.num_pixelbuffers >= BOB_MAX_PIXELBUFFER_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded PixelBuffer Capacity");
        *pb |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_pixelbuf_slot == UINT32_MAX) {
        index = intrn_data.num_pixelbuffers;
    }
    else {
        index = intrn_data.next_pixelbuf_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_data.next_pixelbuf_slot = index + 1; intrn_data.next_pixelbuf_slot < intrn_data.num_pixelbuffers; intrn_data.next_pixelbuf_slot++) {
            if (!intrn_data.pixelbuffer_table[intrn_data.next_pixelbuf_slot].init)
                break;
        }

        if (intrn_data.next_pixelbuf_slot >= intrn_data.num_pixelbuffers)
            intrn_data.next_pixelbuf_slot = UINT32_MAX;
    }

    intrn_data.pixelbuffer_table[index].init = 1; //Setting the value to be initialised

    //Setting up the texture for the pixel simulations:
    if(!BOB_create_texture(width, height, NULL, format, &intrn_data.pixelbuffer_table[index].pixel_tex)) {
        intrn_data.pixelbuffer_table[index] = (BOB_PixelBuffer){0};
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

    intrn_data.pixelbuffer_table[index].buf_sz = width * height * pixel_size;

    //Setting up the pbo for the pixel simulations
    glGenBuffers(1, &intrn_data.pixelbuffer_table[index].pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, intrn_data.pixelbuffer_table[index].pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, intrn_data.pixelbuffer_table[index].buf_sz, NULL, GL_STREAM_DRAW);
    uint8_t *ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if(ptr) {
        BOB_MEMSET(ptr, 0x00, intrn_data.pixelbuffer_table[index].buf_sz); //Setting all of the pixels to be colourless initially
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    intrn_data.num_pixelbuffers++;
    *pb = index;
    return 1;
}
//Frees the data used by a pixel buffer
void BOB_pixelbuffer_free(BOB_PixelBuffer_Handle *pb) {
    if(*(pb) & BOBi_MSB) return; //Do not work with already invalid handles
    *(pb) |= BOBi_MSB; //Setting the MSB to indicate this is an invalid handle
    uint32_t index = *(pb) & (~BOBi_MSB);
    if(index < intrn_data.next_pixelbuf_slot) intrn_data.next_pixelbuf_slot = index;

    BOBi_pixelbuffer_free(index);
}

BOB_PixelBuffer *BOB_get_pixelbuf_ref(BOB_PixelBuffer_Handle pb) {
    if(pb & BOBi_MSB) return NULL; //Do not work with already invalid handles
    uint32_t index = pb & (~BOBi_MSB);

    return &intrn_data.pixelbuffer_table[index];
}

//Draws the entire frame directly on the screen by copying its entire contents into the renderer's pixel buffer
uint8_t BOB_pixelbuffer_updload_data(BOB_PixelBuffer_Handle pb, uint8_t *data) {
    if(pb & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Texture tex = intrn_data.texture_table[intrn_data.pixelbuffer_table[pb].pixel_tex];

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, intrn_data.pixelbuffer_table[pb].pbo);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    BOB_MEMCPY(ptr, data, intrn_data.pixelbuffer_table[pb].buf_sz);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindTexture(GL_TEXTURE_2D, tex.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex.width, tex.height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return 1;
}

//Gets the pixel data from a PixelBuffer
uint8_t BOB_pixelbuffer_get_data(BOB_PixelBuffer_Handle pb, uint8_t *dest) {
    if(pb & BOBi_MSB) return 0; //Do not work with already invalid handles
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, intrn_data.pixelbuffer_table[pb].pbo);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_READ_ONLY);
    BOB_MEMCPY(dest, ptr, intrn_data.pixelbuffer_table[pb].buf_sz);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return 1;
}

//============================================================= DRAWING FUNCTIONS ===========================================

uint8_t BOB_draw_texture(BOB_Renderer *r, BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_texture_mat(r, texture, screen_quad, tex_sub_rect, colour, layer, rotation, r->default_mat);
}

//Draws an atlas quad
uint8_t BOB_draw_atlas_quad(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation) {
    return BOB_draw_atlas_quad_mat(r, screen_quad, tex_sub_rect, colour, intrn_data.atlas_table[atlas].texture, layer, rotation, r->default_mat);
}

uint8_t BOB_draw_pixel_buffer(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation) {
    return BOB_draw_pixel_buffer_mat(r, pb, dimensions, uv_dimensions, colour, layer, rotation, r->default_mat);
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
//TODO: Create a new function that this and BOBi_draw_mesh call that handles all of the batching by itself
uint8_t BOB_draw_texture_mat(BOB_Renderer *r, BOB_Texture_Handle texture, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if((texture & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    if(!BOBi_clip_quad(r, &screen_quad)) return 1; //Early exit

    //Lazy allocation of memory
    if(!BOBi_GET_RENDER_BATCH(texture, mat).init) {
        BOBi_GET_RENDER_BATCH(texture, mat).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOBi_GET_RENDER_BATCH(texture, mat).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
        BOBi_GET_RENDER_BATCH(texture, mat).index_size = BOB_INIT_INDEX_CAPACITY;
        BOBi_GET_RENDER_BATCH(texture, mat).vertex_size = BOB_INIT_VERTEX_CAPACITY;
        BOBi_GET_RENDER_BATCH(texture, mat).init = 1;
    }

    //If we have overreached our current rendering limit or we cannot store any more textures, end the current draw call and start a new one
    BOBi_check_capacity(r, texture, mat, BOB_VERTICIES_PER_QUAD, BOB_INDECIES_PER_QUAD);

    //Update the vertex count and vertex data stored in the renderer
    uint32_t base_index = BOBi_GET_RENDER_BATCH(texture, mat).vertex_count;

    BOB_Vector2 rotated_coords[4];
    BOBi_rotate_quad(screen_quad, rotated_coords, rotation);

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    BOB_Vector3 coords[4] = {
        {rotated_coords[0].x, rotated_coords[0].y, layer},
        {rotated_coords[1].x, rotated_coords[1].y, layer},
        {rotated_coords[2].x, rotated_coords[2].y, layer},
        {rotated_coords[3].x, rotated_coords[3].y, layer}
    };

    float width = intrn_data.texture_table[texture].width;
    float height = intrn_data.texture_table[texture].height;

    BOB_Vector2 uv[4] = {
        {tex_sub_rect.x / width, tex_sub_rect.y / height},
        {tex_sub_rect.x / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width, (tex_sub_rect.y + tex_sub_rect.h) / height},
        {(tex_sub_rect.x + tex_sub_rect.w) / width , tex_sub_rect.y / height}
    };

    //Lazy allocation of memory
    if(BOBi_GET_RENDER_BATCH(texture, mat).vertex_data == NULL || BOBi_GET_RENDER_BATCH(texture, mat).index_data == NULL) {
        BOBi_GET_RENDER_BATCH(texture, mat).index_data = BOB_MALLOC(sizeof(uint32_t) * BOB_INIT_INDEX_CAPACITY);
        BOBi_GET_RENDER_BATCH(texture, mat).vertex_data = BOB_MALLOC(sizeof(BOB_Render_Vertex) * BOB_INIT_VERTEX_CAPACITY);
    }

    for(int i = 0; i < BOB_VERTICIES_PER_QUAD; i++) {
        BOBi_GET_RENDER_BATCH(texture, mat).vertex_data[BOBi_GET_RENDER_BATCH(texture, mat).vertex_count++] = (BOB_Render_Vertex){colour, coords[i], uv[i]};
    }

    //Need to also add ebo data so we can remove overlapping vertices
    //First triangle
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index;
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index + 1;
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index + 3;

    //Second triangle
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index + 1;
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index + 2;
    BOBi_GET_RENDER_BATCH(texture, mat).index_data[BOBi_GET_RENDER_BATCH(texture, mat).index_count++] = base_index + 3;

    return 1;
}

//Draws a quad with a specified material
uint8_t BOB_draw_atlas_quad_mat(BOB_Renderer *r, BOB_Quad screen_quad, BOB_Quad tex_sub_rect, BOB_Vector4 colour, BOB_Atlas_Handle atlas, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    return BOB_draw_texture_mat(r, intrn_data.atlas_table[atlas].texture, screen_quad, tex_sub_rect, colour, layer, rotation, mat);
}

//Draws a pixel buffer with a specified material
uint8_t BOB_draw_pixel_buffer_mat(BOB_Renderer *r, BOB_PixelBuffer_Handle pb, BOB_Quad dimensions, BOB_Quad uv_dimensions, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if((pb & BOBi_MSB) || (mat & BOBi_MSB)) return 0; //Do not work with already invalid handles
    if(!BOBi_clip_quad(r, &dimensions)) return 1; //Early exit
    BOB_Texture tex = intrn_data.texture_table[intrn_data.pixelbuffer_table[pb].pixel_tex];

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, intrn_data.pixelbuffer_table[pb].pbo);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, tex.texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex.width, tex.height, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return BOB_draw_texture_mat(r, intrn_data.pixelbuffer_table[pb].pixel_tex, dimensions, uv_dimensions, colour, layer, rotation, mat);
}

//Draws a filled circle with a specified material
uint8_t BOB_draw_circle_mat(BOB_Renderer *r, BOB_Vector2 centre, float radius, BOB_Vector4 colour, uint16_t layer, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

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
    if(clipped_size < 3) return 1; //Early exit

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    //Generating the indecies for the triangle ebo
    for(int i = 0; i < clipped_size; i++) {
        points3[i] = (BOB_Vector3){points2[i].x, points2[i].y, layer};
        indices[index_count++] = 0;
        indices[index_count++] = i;
        indices[index_count++] = ((i+1) % clipped_size);
    }

    BOBi_draw_mesh(r, points3, clipped_size, indices, index_count, colour, mat);
    return 1;
}

//Draws a filled quad with a specified material
uint8_t BOB_draw_quad_mat(BOB_Renderer *r, BOB_Quad quad, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles
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

    BOBi_draw_mesh(r, strip, 4, (uint32_t[6]){0,1,3,1,2,3}, 6, colour, mat);
    return 1;
}

//Draws a filled triangle with a specified material
uint8_t BOB_draw_polygon_mat(BOB_Renderer *r, BOB_Vector2* poly_points, size_t poly_size, BOB_Vector4 colour, uint16_t layer, float rotation, BOB_Material_Handle mat) {
    if(mat & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOB_Vector2 points[BOBi_MAX_POLY_SIZE];
    BOB_MEMCPY(points, poly_points, poly_size * sizeof(BOB_Vector2));

    size_t clipped_size = BOBi_clip_polygon(r, points, poly_size);
    if(clipped_size < 3) return 1; //Early exit

    uint32_t triangle_indices[(BOBi_MAX_POLY_SIZE - 2) * 3]; //Ear clipping always produces n-2 triangles for a polygon with n vertices
    size_t triangle_count = BOBi_triangulate_ec(points, clipped_size, triangle_indices);

    if(!triangle_count) return 1; //Early exit

    //Processing the returned vertex data into a more compact form so we can pass it to the renderer
    uint32_t vertex_map[BOBi_MAX_POLY_SIZE];

    //Filling the map with dummy values
    for(size_t i = 0; i < clipped_size; i++)
        vertex_map[i] = UINT32_MAX;

    BOB_Vector3 vertices[BOBi_MAX_POLY_SIZE]; //Holds the compressed vertex values
    size_t vertex_count = 0;

    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER -1; //Normalise it to be within the required range

    //Copying the old verticies into compressed format
    for(size_t i = 0; i < triangle_count*3; i++) {
        uint32_t old = triangle_indices[i];
        if(vertex_map[old] == UINT32_MAX) {
            vertex_map[old] = vertex_count;
            vertices[vertex_count++] = (BOB_Vector3){points[old].x, points[old].y, layer};
        }
        triangle_indices[i] = vertex_map[old];
    }

    BOBi_draw_mesh(r, vertices, vertex_count, triangle_indices, triangle_count * 3, colour, mat);
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
    BOB_MEMCPY(points, poly_points, poly_size * sizeof(BOB_Vector2));

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

    if(!BOBi_clip_line(r, &start_pos, &end_pos)) return 1; //Early exit

    BOB_Vector2 delta = {end_pos.x - start_pos.x, end_pos.y - start_pos.y};
    float length = sqrtf(delta.x*delta.x + delta.y*delta.y);
    if(layer > BOB_MAX_LAYER) layer = BOB_MAX_LAYER-1; //Normalise it to be within the required range

    if(length > 0 && thickness > 0) {
        float scale = thickness/(2*length);

        BOB_Vector2 radius = {-scale*delta.y, scale*delta.x};
        BOB_Vector3 strip[4] = {
            {start_pos.x - radius.x, start_pos.y - radius.y, layer},
            {start_pos.x + radius.x, start_pos.y + radius.y, layer},
            {end_pos.x - radius.x, end_pos.y - radius.y, layer},
            {end_pos.x + radius.x, end_pos.y + radius.y, layer},
        };

        BOBi_draw_mesh(r, strip, 4, (uint32_t[6]){0,1,2,1,2,3}, 6, colour, mat);
    }

    return 1;
}

//=================================== CLIPPING FUNCTIONS =====================================

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

//===================================== BITMAP FONT RENDERING =============================================

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
    *buf = BOB_MALLOC(fsz + (int)add_null);
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
} BOBi_Parse_Error_Data;

BOBi_Parse_Error_Data error_data = {0};

void BOBi_append_glyph(BOB_Font *font, BOB_Glyph g) {
    if(font->glyphs == NULL) font->glyphs = BOB_MALLOC(sizeof(BOB_Glyph) * font->glyph_capacity);
    if(font->glyph_count >= font->glyph_capacity) {
        size_t new_cap = (font->glyph_capacity > 0) ? font->glyph_capacity * 2 : 16;
        BOB_Glyph *temp = BOB_MALLOC(sizeof(BOB_Glyph) * new_cap);
        BOB_MEMCPY(temp, font->glyphs, sizeof(BOB_Glyph) * font->glyph_capacity);
        free(font->glyphs);
        font->glyphs = temp;
        font->glyph_capacity = new_cap;
    }

    BOBi_hashmap_add(font->glyph_map, g.codepoint, font->glyph_count);
    font->glyphs[font->glyph_count++] = g;
}
void BOBi_append_kerning(BOB_Font *font, BOB_Kerning k) {
    if(font->kernings == NULL) font->kernings = BOB_MALLOC(sizeof(BOB_Kerning) * font->kerning_capacity);
    if(font->kerning_count >= font->kerning_capacity) {
        size_t new_cap = (font->kerning_capacity > 0) ? font->kerning_capacity * 2 : 16;
        BOB_Kerning *temp = BOB_MALLOC(sizeof(BOB_Kerning) * new_cap);
        BOB_MEMCPY(temp, font->kernings, sizeof(BOB_Kerning) * font->kerning_capacity);
        free(font->kernings);
        font->kernings = temp;
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
            *font->glyph_map = BOBi_hashmap_init(font->glyph_capacity);
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
            *font->kerning_map = BOBi_hashmap_init(font->kerning_capacity);
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
    BOB_MEMCPY(&block, data, sizeof(BOBi_BMF_Common_Block));
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
    *font->glyph_map = BOBi_hashmap_init(font->glyph_capacity);

    for(size_t i = 0; i < num_chars; i++) {
        BOBi_BMF_Chars_Block block;
        BOB_MEMCPY(&block, data, sizeof(BOBi_BMF_Chars_Block));
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
    *font->kerning_map = BOBi_hashmap_init(font->kerning_capacity);

    for(size_t i = 0; i < num_kernings; i++) {
        BOBi_BMF_Kernings_Block block;
        BOB_MEMCPY(&block, data, sizeof(BOBi_BMF_Kernings_Block));
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
        BOB_MEMCPY(&block_sz, ptr, sizeof(block_sz));
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

int8_t BOB_load_bmf_font(const char *font_path, BOB_Font_Handle *font, BOB_BMF_Format format) {
    if(intrn_data.num_fonts >= BOB_MAX_FONT_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_font_slot == UINT32_MAX) {
        index = intrn_data.num_fonts;
    }
    else {
        index = intrn_data.next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_data.next_font_slot = index + 1; intrn_data.next_font_slot < intrn_data.num_fonts; intrn_data.next_font_slot++) {
            if (!intrn_data.font_table[intrn_data.next_font_slot].init)
                break;
        }

        if (intrn_data.next_font_slot >= intrn_data.num_fonts)
            intrn_data.next_font_slot = UINT32_MAX;
    }

    intrn_data.font_table[index].init = 1; //Setting the value to be initialised

    uint8_t *buf;
    int size = BOBi_read_to_end(font_path, &buf, 1);
    if(size < 0) {
        *font |= BOBi_MSB;
        return 0;
    }

    uint8_t res = (format == BOB_BMF_TEXT) ? BOBi_parse_text(&intrn_data.font_table[index], buf, size) : BOBi_parse_binary(&intrn_data.font_table[index], buf, size);
    free(buf);
    if(!res) {
        *font |= BOBi_MSB;
        intrn_data.font_table[index] = (BOB_Font){0}; //Clear all of the initially assigned font data
        return 0;
    }

    intrn_data.num_fonts++;
    *font = index;
    return 1;
}

uint8_t BOB_create_custom_font(BOB_Font_Handle *font, size_t num_glyphs, size_t num_kernings, size_t line_height, size_t base) {
    if(intrn_data.num_fonts >= BOB_MAX_FONT_CAPACITY) {
        BOB_PRINT("ERROR: Exceeded Font Capacity");
        *font |= BOBi_MSB;
        return 0;
    }

    uint32_t index;
    if (intrn_data.next_font_slot == UINT32_MAX) {
        index = intrn_data.num_fonts;
    }
    else {
        index = intrn_data.next_font_slot;

        //Linearly searching to find the next empty slot. Yes I know that this is slow
        for (intrn_data.next_font_slot = index + 1; intrn_data.next_font_slot < intrn_data.num_fonts; intrn_data.next_font_slot++) {
            if (!intrn_data.font_table[intrn_data.next_font_slot].init)
                break;
        }

        if (intrn_data.next_font_slot >= intrn_data.num_fonts)
            intrn_data.next_font_slot = UINT32_MAX;
    }

    intrn_data.font_table[index].init = 1; //Setting the value to be initialised
    intrn_data.font_table[index].base = base;
    intrn_data.font_table[index].line_height = line_height;
    intrn_data.font_table[index].glyph_capacity = num_glyphs;
    intrn_data.font_table[index].glyph_count = 0;
    intrn_data.font_table[index].kerning_capacity = num_kernings;
    intrn_data.font_table[index].kerning_count = 0;
    intrn_data.font_table[index].page_count = 0;
    if(num_glyphs) {
        intrn_data.font_table[index].glyphs = malloc(sizeof(BOB_Glyph) * num_glyphs);
        intrn_data.font_table[index].glyph_map = malloc(sizeof(BOBi_Hashmap));
        *intrn_data.font_table[index].glyph_map = BOBi_hashmap_init(intrn_data.font_table[index].glyph_capacity);
    }
    if(num_kernings) {
        intrn_data.font_table[index].kernings = malloc(sizeof(BOB_Kerning) * num_kernings);
        intrn_data.font_table[index].kerning_map = malloc(sizeof(BOBi_Hashmap));
        *intrn_data.font_table[index].kerning_map = BOBi_hashmap_init(intrn_data.font_table[index].kerning_capacity);
    }

    *font = index;

    return 1;
}

uint8_t BOB_add_font_page(BOB_Font_Handle font, uint32_t page_width, uint32_t page_height, uint8_t *page_data, BOB_Format page_format) {
    if(font & BOBi_MSB) return 0; //Do not do anything with invalid handles

     return BOB_create_texture(page_width, page_height, page_data, page_format, &intrn_data.font_table[font].pages[intrn_data.font_table[font].page_count++]);
}

uint8_t BOB_draw_codepoint(BOB_Renderer *r, BOB_Font_Handle font, uint32_t codepoint, BOB_Vector2 *pos, BOB_Vector4 colour, uint16_t layer) {
    BOB_Font f = intrn_data.font_table[font];
    uint32_t index = BOBi_hashmap_get(f.glyph_map, codepoint);
    if(index == UINT32_MAX) return 0; //Codepoint doesn't exist

    BOB_Glyph g = f.glyphs[index];
    BOB_draw_texture(r, f.pages[g.page], (BOB_Quad){pos->x + g.x_offset, pos->y + g.y_offset, g.sub_rect.w, g.sub_rect.h}, g.sub_rect, colour, layer, 0.0f);
    pos->x += g.x_advance;
    return 1;
}

uint8_t BOB_draw_char_string(BOB_Renderer *r, BOB_Font_Handle font, char *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    BOB_Font f = intrn_data.font_table[font];
    float start_x = start->x;
    for(size_t i = 0; i < str_len; i++) {
        switch (str[i]) {
            case '\n':
                start->x = start_x;
                start->y += f.line_height;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                start->x += f.glyphs[index].x_advance * 4;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && i > 0 && str[i-1] != '\n' && str[i] != '\n') {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)str[i-1] << 32) | (uint32_t)str[i]);
            if(index != UINT32_MAX) start->x += f.kernings[index].amount;
        }

        if(!BOB_draw_codepoint(r, font, str[i], start, colour, layer)) return 0;

    }

    return 1;
}


uint8_t BOB_draw_codepoint_string(BOB_Renderer *r, BOB_Font_Handle font, uint32_t *str, size_t str_len, BOB_Vector2 *start, BOB_Vector4 colour, uint16_t layer) {
    BOB_Font f = intrn_data.font_table[font];
    float start_x = start->x;
    for(size_t i = 0; i < str_len; i++) {
        switch (str[i]) {
            case '\n':
                start->x = start_x;
                start->y += f.line_height;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                start->x += f.glyphs[index].x_advance * 4;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && i > 0 && str[i-1] != '\n' && str[i] != '\n') {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)str[i-1] << 32) | (uint32_t)str[i]);
            if(index != UINT32_MAX) start->x += f.kernings[index].amount;
        }

        if(!BOB_draw_codepoint(r, font, str[i], start, colour, layer)) return 0;

    }

    return 1;
}

uint8_t BOB_append_glyph(BOB_Font_Handle font, BOB_Glyph glyph) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_append_glyph(&intrn_data.font_table[font], glyph);
    return 1;
}
uint8_t BOB_append_kerning(BOB_Font_Handle font, BOB_Kerning kerning) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles

    BOBi_append_kerning(&intrn_data.font_table[font], kerning);
    return 1;
}

uint8_t BOB_measure_char_string(const char *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Font f = intrn_data.font_table[font];
    float max_w = 0;
    float h = f.line_height;
    float cur_w = 0;

    for(size_t i = 0; i < str_len; i++) {
        switch(str[i]) {
            case '\n':
                if(cur_w > max_w) max_w = cur_w;
                cur_w = 0;
                h += f.line_height;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                cur_w +=  f.glyphs[index].x_advance * 4;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && i > 0 && str[i-1] != '\n' && str[i] != '\n') {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)str[i-1] << 32) | (uint32_t)str[i]);
            if(index != UINT32_MAX) cur_w += f.kernings[index].amount;
        }
        uint32_t index = BOBi_hashmap_get(f.glyph_map, str[i]);
        if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
        cur_w += f.glyphs[index].x_advance;
    }

    if(cur_w > max_w) max_w = cur_w;

    *out = (BOB_Vector2){max_w, h};

    return 1;
}

uint8_t BOB_measure_codepoint_string(const uint32_t *str, size_t str_len, BOB_Font_Handle font, BOB_Vector2 *out) {
    if((font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Font f = intrn_data.font_table[font];
    float max_w = 0;
    float h = f.line_height;
    float cur_w = 0;

    for(size_t i = 0; i < str_len; i++) {
        switch(str[i]) {
            case '\n':
                if(cur_w > max_w) max_w = cur_w;
                cur_w = 0;
                h += f.line_height;
            continue;
            case '\t': {
                uint32_t index = BOBi_hashmap_get(f.glyph_map, 32);
                if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
                cur_w +=  f.glyphs[index].x_advance * 4;
            }
            continue;
            default:
            break;
        }

        if(f.kerning_capacity > 0 && i > 0 && str[i-1] != '\n' && str[i] != '\n') {
            uint32_t index = BOBi_hashmap_get(f.kerning_map, ((uint64_t)str[i-1] << 32) | (uint32_t)str[i]);
            if(index != UINT32_MAX) cur_w += f.kernings[index].amount;
        }
        uint32_t index = BOBi_hashmap_get(f.glyph_map, str[i]);
        if(index == UINT32_MAX) return 0; //Codepoint doesn't exist
        cur_w += f.glyphs[index].x_advance;
    }

    if(cur_w > max_w) max_w = cur_w;

    *out = (BOB_Vector2){max_w, h};

    return 1;
}

void BOB_print_parsing_error(void) {
    printf("Error Line: %u\nError Column: %u\nError Char: %c\n", error_data.error_line, error_data.error_col, error_data.error_char);
}

uint8_t BOB_font_free(BOB_Font_Handle *font) {
    if((*font) & BOBi_MSB) return 0; //Do not work with already invalid handles
    BOB_Font f = intrn_data.font_table[*font];

    for(size_t i = 0; i < f.page_count; i++) {
        BOB_texture_free(&f.pages[i]);
    }

    BOBi_font_free(*font);

    *font |= BOBi_MSB;
    return 1;
}
