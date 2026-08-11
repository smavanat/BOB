# BOB: A batched 2D Renderer for OpenGL and Vulkan

### Features  
 - OpenGL and Vulkan backends  
 - Batched rendering to ensure fewer draw calls per frame to improve performance  
 - Static memory usage means no memory allocations during rendering  
 - Bitmap font rendering API alongside a built-in BMFont parser  
 - Support for rendering primitives: lines, quads, polys  
 - Rect clipping  
 - Texture atlas generation  
 - Pixelbuffers so you can stream pixel data directly to a texture on the GPU  
 - Assign layers to draw calls to determine draw order  
 - Built-in default shaders so you can easily get started

**N.B.** This library is still in early development, and there are still plenty of features to be added, optimisations to be made, and bugs to be fixed (as can be seen by the large number of TODOs scattered throughout the code).

## Getting Started  
If using the OpenGL backend, define BOB_INCLUDE_GLAD and BOB_IMPLEMENTATION in one file before adding bob.h and add [glad.h](https://gen.glad.sh/) to your include path  
If using the Vulkan backend, define BOB_INCLUDE_VULKAN and BOB_IMPLEMENTATION in one file before adding bob.h and add the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (at least version 1.3) to your include path  

```C
#define BOB_IMPLEMENTATION
#define BOB_INCLUDE_VULKAN //NOTE: You can include both the OpenGL and Vulkan backends at the same time, but GLFW only supports having one active at a time
#include "../bob.h"
#include <GLFW/glfw3.h> //Using GLFW as our windowing library of choice, but any libary that supports OpenGL and Vulkan will do
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" //So that we can load the font texture
#include <sys/time.h>

GLFWwindow *window;
BOB_Renderer_Handle r;

//GLFW callback for when the window is resized
//Updates the width and height in the renderer so that the projection matrix
//(and the objects being rendered) can be adjusted to fit the new screen size
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    //Update viewport dimensions
    glViewport(0, 0, width, height);
    glfwGetFramebufferSize(window, &width, &height);
    BOB_renderer_update_dimensions(r, width, height, width, height);
}

//Callback function used by the Vulkan backend to create the swapchain surface
#ifdef BOB_INCLUDE_VULKAN
uint8_t create_window_surface(VkInstance instance, VkSurfaceKHR *surface) {
    if(glfwCreateWindowSurface(instance, window, NULL, surface)){
        printf("Failed to create window surface\n");
        return 0;
    }
    return 1;
}
#endif

//Function that initialises GLFW and your given backend
int init(GLFWwindow **window) {
    //Initialising GLFW:
    glfwInit();

    //OpenGL Backend
    #ifdef BOB_INCLUDE_GLAD
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24); //Necessary for enabling the depth buffer

    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    *window = glfwCreateWindow(800, 600, "BOB", NULL, NULL);

    if(*window == NULL) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return 0;
    }

    //Setting callback functions
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);

    //Loading Glad and initialising BOB
    if(!BOB_init((GLADloadproc)glfwGetProcAddress, 1)) {
        glfwTerminate();
        return 0;
    }

    //Creating the renderer we are going to use
    if(!BOB_create_opengl_renderer(BOB_MAX_ATLAS_CAPACITY, BOB_MAX_PIXELBUFFER_CAPACITY, BOB_MAX_TEX_CAPACITY, BOB_MAX_MATERIAL_CAPACITY, BOB_MAX_FONT_CAPACITY,
                                   800, 600, BOB_MAX_VERTEX_CAPACITY, BOB_MAX_INDEX_CAPACITY, BOB_MAX_DRAW_CALL_CAPACITY, &r)) {
        glfwTerminate();
        return 0;
    }
    #endif

    //Vulkan backend
    #ifdef BOB_INCLUDE_VULKAN
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Set to NO_API so that it uses Vulkan instead of OpenGL
    *window = glfwCreateWindow(800, 600, "BOB", NULL, NULL);

    if(*window == NULL) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return 0;
    }

    //Setting callback functions
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);

    //Get the required vulkan extensions from GLFW
    uint32_t glfw_extension_count = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    if(glfwExtensions == NULL) {
        printf("Failed to get required GLFW extensions\n");
        return 0;
    }

    //Creating a Vulkan instance and initialising BOB
    if(!BOB_init(glfwExtensions, glfw_extension_count, 1)) {
        glfwTerminate();
        return 0;
    }

    //Creating the Vulkan renderer
    int width, height;
    glfwGetFramebufferSize(*window, &width, &height);
    if(!BOB_create_vulkan_renderer(BOB_MAX_ATLAS_CAPACITY, BOB_MAX_PIXELBUFFER_CAPACITY, BOB_MAX_TEX_CAPACITY, BOB_MAX_MATERIAL_CAPACITY, BOB_MAX_FONT_CAPACITY,
                                  800, 600, width, height, BOB_MAX_VERTEX_CAPACITY, BOB_MAX_INDEX_CAPACITY, BOB_MAX_DRAW_CALL_CAPACITY, &create_window_surface, &r)) {
        glfwTerminate();
        return 0;
    }
    #endif

    return 1;

    return 1;
}

//Loads our font texture
uint32_t load_tex(char *path, BOB_Font_Handle font) {
    int width, height, nrChannels;
    unsigned char *data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        BOB_Format format;
        //Converting the number of channels into the required format
        switch (nrChannels) {
            case 1: format = BOB_RED; break;
            case 2: format = BOB_RG; break;
            case 3: format = BOB_RGB; break;
            case 4: format = BOB_RGBA; break;
        }

        // *tex = BOB_create_texture(width, height, data, format);
        BOB_add_font_page(font, width, height, data, format);
    }
    else {
        fprintf(stderr, "Failed to load texture at path %s\n", path);
        return 0;
    }
    stbi_image_free(data);

    return 1;
}


int main(void) {
    if(init(&window)) {
        //Loading our font data
        BOB_Font_Handle font;
        int8_t res = BOB_load_bmf_font(r, "../test.fnt", &font, BOB_BMF_TEXT);
        if(res <= 0) BOB_print_parsing_error();
        load_tex("../test.png", font); //Loading the font image

        while(!glfwWindowShouldClose(window)) {

            //Clear the screen and begin rendering
            BOB_renderer_begin(r, (float[4]){0.0f, 0.0f, 0.0f, 0.0f});
                //Draw some primitives
                BOB_draw_quad(r, (BOB_Quad){80, 80, 200, 200}, (BOB_Vector4){1.0, 0.0, 0.0, 1.0}, 100.0, 0.0);
                BOB_draw_circle(r, (BOB_Vector2){200, 200}, 30, (BOB_Vector4){0.0, 1.0, 0.0, 1.0}, 101.0);
                BOB_draw_line(r, (BOB_Vector2){200, 200}, (BOB_Vector2){400, 200}, 2.0, (BOB_Vector4){0.0, 0.0, 1.0, 1.0}, 100.0);
                BOB_draw_polygon(r, (BOB_Vector2[3]){(BOB_Vector2){60.0f, 100.0f}, (BOB_Vector2){80.0f, 120.0f}, (BOB_Vector2){100.0f, 120.0f}}, 3, (BOB_Vector4){0.0f, 1.0f, 0.0f, 1.0f}, 100.0, 0.0);

                //Print Hello, World on the screen
                BOB_Vector2 pos = (BOB_Vector2){100, 100};
                if(!BOB_draw_char_string(font, "Hello,\nWorld", 12, &pos, (BOB_Vector4){0.0, 1.0, 0.0, 1.0}, 1000)) printf("Invalid Codepoint\n");
            BOB_renderer_end(r); //Submit our draw calls to the GPU

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    BOB_terminate();
    glfwTerminate();
}
```

The above code will output something like the following:  

**TODO** Add image here  
## Summary

* [General Concepts](#general-concepts)
    * [Initialising BOB](#initialising-bob)
    * [Creating Objects](#creating-objects)
    * [The Render Loop](#the-render-loop)
    * [Rendering Primitives](#rendering-primitives)
    * [Rendering Textures](#rendering-textures)
    * [Rendering Fonts](#rendering-fonts)
    * [Streaming data to a pixelbuffer](#streaming-data-to-a-pixelbuffer)
    * [Adding images to an atlas](#adding-images-to-an-atlas)
    * [Specifying a Clip Rect](#specifying-a-clip-rect)
    * [Loading a BMFont](#loading-a-bmfont)
    * [Creating a font from a different Bitmap font format](#creating-a-font-from-a-different-bitmap-font-format)
    * [Creating custom shaders](#creating-custom-shaders)
    * [Setting custom values to constants](#setting-custom-values-to-constants)
 * [API](#api)
    * [OpenGL](#opengl)
    * [Vulkan](#vulkan)
    * [Renderer](#renderer)
    * [Texture](#texture)
    * [Atlas](#atlas)
    * [Pixelbuffer](#pixelbuffer)
    * [Font](#font)
    * [Material](#material)
    * [Shader Data](#shader-data)
    * [Uniform](#uniform)
    * [Clipping](#clipping)

## General Concepts
### Initialising BOB
BOB must be initialised by call BOB_init, specifying the GLADloadproc if using the OpenGL backend or the required Vulkan extensions if using the Vulkan backend. You must also specify the number of renderers your program expects to use so that BOB can accurately allocate memory.
### Creating Objects
The first thing to do after initialising BOB is to create a BOB_Renderer. You should have one BOB_Renderer per application window. The BOB_Render handles drawing operations as well as managing the objects created with that renderer. As with most BOB objects, BOB_Renderers are accessed using handles, both to remove the burdern of object management away from the end user and into the library and to prevent undue modification of object data. Objects created using one BOB_Renderer cannot be used by another, the program will throw an error.
### The Render Loop
Draw calls should be made between the BOB_renderer_begin() and BOB_renderer_end() functions
```C
BOB_renderer_begin(r, (float[4]){0.0f, 0.0f, 0.0f, 0.0f});
    BOB_draw_quad(r, (BOB_Quad){80, 80, 200, 200}, (BOB_Vector4){1.0, 0.0, 0.0, 1.0}, 100.0, 0.0);
    BOB_draw_circle(r, (BOB_Vector2){200, 200}, 30, (BOB_Vector4){0.0, 1.0, 0.0, 1.0}, 101.0);
    BOB_draw_texture(texture, (BOB_Quad){300, 300, 50, 50}, (BOB_Quad){0, 0, 1, 1}, (BOB_Vector4){1.0, 1.0, 1.0, 1.0}, 200, 0.0);
    ...
BOB_renderer_end();
```
The second argument of BOB_renderer_begin() is the colour the screen should be cleared to. If you don't want the screen's colour to be cleared, set it to NULL.  
When drawing primitives the renderer must be specified, but when drawing from an object this is not necessary as its handle contains data on the BOB_Renderer that owns it.  
This loop does not guarantee that only one draw call will be made in a frame, as the renderer could be flushed several times between BOB_renderer_begin() and BOB_renderer_end(), BOB_renderer_end() just indicates that it is time to submit the current swapchain image to be presented to the screen.  
Almost all draw functions in BOB will as for a colour and a layer as an input. The colour specifies the tint you want the final image to have, if you don't want a tint, set it to ```(BOB_Vector4){1.0f, 1.0f, 1.0f, 1.0f}``` (white). The layer determines which elements will be drawn on top and which on the bottom, with larger values being drawn at the top and lower ones at the bottom. You can currently specify layer values between 0 and BOB_MAX_LAYER (1024). If you need more layers, see [Setting custom values to constants](#setting-custom-values-to-constants) on how to change this.
**N.B.** Due to how BOB handles batching, BOB only guarantees that draw calls that share the same material and texture will be drawn in the order they were made, otherwise there is no guarantee that draw calls to the same layer using different materials/textures will be drawn in the order they were made. If you need a specific order between your draw calls, use layers.
### Rendering Primitives
Primitives (lines, quads and polys) can be renderered from a specific BOB_Renderer. When created, each BOB_Renderer creates a small one pixel white texture which it uses as a base to draw primitives from, along side a default material is used to render textures and primitives when no other material is specified. All primitives (except lines) can be rendered either filled or unfilled. When rendering a filled polygon, there is a limit on the number of verticies you can use as specified by BOB_MAX_POLY_SIZE (see the section on [setting custom values](#setting-custom-values-to-constants) on how to change this value) which is currently set to 256. This is due to BOB statically allocating memory to be used when triangulating the polygon so it can be renderered. Unfilled polygons do not need to be triangulated and as such do not have this constraint.  
### Rendering Textures
When rendering a texture from an image, atlas, or pixelbuffer you can specify the screen dimensions of the texture as well as the subregion of the texture you would like to use. The subregion must be specified in texture coordinates, not normalised uv coordinates, it is normalised inside the function.
### Rendering Fonts
When rendering a string from a font you should use BOB_draw_char_string() (for ASCII strings) or BOB_draw_codepoint_string() (for Unicode strings). BOB also provides BOB_draw_codepoint() which can be used to render a single (ASCII or Unicode) character. All of these functions require a pointer to a BOB_Vector2, which should contain the start position you want BOB to start drawing from. As BOB draws the string, it will update the values stored in this pointer so that it eventually shows the final position after rendering. BOB also provides the utility functions BOB_measure_char_string() and BOB_measure_codepoint_string() that return the dimensions of the regions the rendered string will occupy.
### Streaming data to a pixelbuffer
BOB_Pixelbuffers represent textures that have their pixel data continuously updated, and as such they function as textures for rendering, but have their own functions for streaming data to and from themselves. This would look something like the following:
```C
BOB_bind_pixelbuffer_memory(pb); //Bind memory
    /* Pixelbuffer Operations */
    ...

    BOB_pixelbuffer_upload(pb); //Stream the final data to the pixelbuffer's texture
BOB_unbind_pixelbuffer_memory(pb);
```
Currently only one pixelbuffer's memory can be bound per renderer (this may change in the future). Currently there are three pixelbuffer data manipulation functions: BOB_pixelbuffer_get_data(), BOB_pixelbuffer_send_data(), BOB_pixelbuffer_upload_data();
### Adding images to an atlas
BOB allows you to build your own texture atlases for efficient batched rendering. This can be done by creating a blank atlas texture using BOB_atlas_init() and then packing it using BOB_atlas_pack(), which returns the sub_rect on the atlas where the new texture data was added.
### Specifying a Clip Rect
BOB supports rect clipping (scissoring). You can begin a new scissor by calling BOB_start_clip() and passing in your clipping region as well as the direction you want the clipping to occur in, which can be ```BOB_CLIP_HORZ``` (horizontal) ```BOB_CLIP_VERT``` (vertical), or ```BOB_CLIP_BOTH``` (both), and end the clipping by calling BOB_end_clip(). Note that clipping applies to all draw calls made between BOB_start_clip() and BOB_end_clip().
### Loading a BMFont
BOB natively supports loading a BMFont using either a text or binary .fnt file (but not xml). Simply call BOB_load_bmf_font with your filepath and specify the .fnt format you are using, BOB_BMF_BINARY for binary .fnt files, and BOB_BMF_TEXT for text .fnt files. Font pages need to be added one by one using BOB_add_font_page(), and should be added in the order of their ids (0 first, then 1, etc).
### Creating a font from a different Bitmap font format
If you would like to create a BOB_Font from a different bitmap format, you'll have to parse the data yourself, but you can easily add it to BOB by first calling BOB_create_custom_font with the number of glyphs and kernings your font contains, and then call BOB_font_append_glyph() or BOB_font_append_kerning() for each glyph/kerning in your font's data.
### Creating custom shaders
**N.B.** This feature is currently incomplete and very experimental. It is recommended that you currently use the default shaders instead of defining your own. If you do want to write your own shaders, uniforms should be bound to binding 0, and you can only have one texture, which should be bound to (set = 1, binding = 0)  
BOB allows you to define your own custom shaders and pass them in when creating a material. To do this, you must first create a BOB_Shader_Data object containing the shader code (a string containing the shader text if using the OpenGL backend, or an array of SPIR-V bytecode if using the Vulkan backend), the size of the shader code, the name of the shader's entrypoint, and the type of shader (**NOTE:** If using the Vulkan backend, currently only Vertex and Fragment shaders are supported). This should then be passed to the BOB_create_material function alongside any default uniform values you want to use.
### Setting custom values to constants
BOB uses a number of constants for its internal operations. You can set your own values for them by calling ```#define {$CONSTANT_NAME}``` before including bob.h in one of your files. Below is a list of all the definable constants and their default values:
 - BOB_CIRCLE_LINE_SEGMENTS: 64
 - BOB_MAX_VERTEX_CAPACITY: 1048576
 - BOB_MAX_INDEX_CAPACITY: 2097152
 - BOB_MAX_TEX_CAPACITY: 32
 - BOB_MAX_ATLAS_CAPACITY: 8
 - BOB_MAX_PIXELBUFFER_CAPACITY: 8
 - BOB_MAX_MATERIAL_CAPACITY: 16
 - BOB_MAX_FONT_CAPACITY: 32
 - BOB_MAX_DRAW_CALL_CAPACITY: BOB_MAX_VERTEX_CAPACITY / 3
 - INIT_STACK_CAPACITY: 64
 - BOB_MAX_LAYER: 1024
 - BOB_MAX_SHADERS: 32
 - BOB_MAX_UNIFORMS: 64
 - BOB_MAX_POLY_SIZE: 256

## API
### OpenGL
### Vulkan
### Renderer
### Texture
### Atlas
### Pixelbuffer
### Font
### Material
### Shader Data
### Uniform
### Clipping
