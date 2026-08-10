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
    * [Creating Objects](#creating-objects)
    * [The Render Loop](#the-render-loop)
    * [Rendering Lines](#rendering-lines)
    * [Rendering Quads](#rendering-quads)
    * [Rendering Polys](#rendering-polys)
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
    * [Texture Atlas](#texture-atlas)
    * [Pixelbuffer](#pixelbuffer)
    * [Font](#font)
    * [Materials](#materials)
    * [Uniforms](#uniforms)
    * [Clipping](#clipping)

## General Concepts
### Creating Objects
### The Render Loop
### Rendering Lines
### Rendering Quads
### Rendering Polys
### Rendering Textures
### Rendering Fonts
### Streaming data to a pixelbuffer
### Adding images to an atlas
### Specifying a Clip Rect
### Loading a BMFont
### Creating a font from a different Bitmap font format
### Creating custom shaders
### Setting custom values to constants
## API
### OpenGL
### Vulkan
### Renderer
### Texture
### Texture Atlas
### Pixelbuffer
### Font
### Materials
### Uniforms
### Clipping
