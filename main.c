#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include <linux/videodev2.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>


bool check_for_modlue() {

    FILE* fp = fopen("/proc/modules", "r");
    if (fp == NULL) {
      return false;
    }

    char* module_name = "v4l2loopback";
    size_t buf_size = strlen(module_name)+1;
    char buffer[buf_size];
    buffer[buf_size-1] = '\0';
    while (fgets(buffer, buf_size, fp)) {
        if (strncmp(buffer, module_name, buf_size-1) == 0) {
            return true;
        }
    }

    fclose(fp);
    return false;
}

char** list_video_devices() {

    DIR* dir;
    struct dirent *entry;
    dir = opendir("/dev");
    if (!dir) {
        return NULL;
    }

    size_t len = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            len++;
        }
    }

    rewinddir(dir);

    char** devices = malloc(sizeof(char*) * (len + 1));
    devices[len] = NULL;

    size_t index = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "video", 5) == 0) {
            devices[index] = malloc(sizeof(char)*(5+strlen(entry->d_name)+1));
            snprintf(devices[index], 5+strlen(entry->d_name)+1, "/dev/%s", entry->d_name);
            index++;
        }
    }

    closedir(dir);

    return devices;
}

void free_cpp(char** ls) {
    size_t index = 0;
    while (ls[index] != NULL) {
        free(ls[index++]);
    }
    free(ls);
}

size_t cpplen(char** ls) {
    size_t index= 0;
    while(ls[index++] != NULL);
    return index;
}

char** list_loopback_devices(char** video_devices) {

    char** loopback_devices = calloc(cpplen(video_devices), sizeof(char*));

    int fd = 0;
    struct v4l2_capability vid_caps = {0};
    size_t wrt_ptr = 0;
    for(size_t i = 0; video_devices[i] != NULL; i++) {
        fd = open(video_devices[i], O_RDWR);
        if (fd >= 0) {
            int device_read_status = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
            if (device_read_status != -1 && strcmp("v4l2 loopback", (char*)vid_caps.driver) == 0) {
                loopback_devices[wrt_ptr++] = strdup(video_devices[i]);
            }
        }
    }
    return loopback_devices;
}

bool is_loopback_device(char* dev) {

    bool is_lbdev = false;

    char** devices = list_video_devices();
    char** loopback_devices = list_loopback_devices(devices);
    for(size_t i = 0; loopback_devices[i] != NULL; i++) {
        if (strcmp(loopback_devices[i], dev) == 0) {
            is_lbdev = true;
            break;
        }
    }

    free_cpp(devices);
    free_cpp(loopback_devices);

    return is_lbdev;
}

void print_format(struct v4l2_format* vid_format) {
    printf( "vid_format->type                 =%d \n"
            "vid_format->fmt.pix.width        =%d \n"
            "vid_format->fmt.pix.height       =%d \n"
            "vid_format->fmt.pix.pixelformat  =%d \n"
            "vid_format->fmt.pix.sizeimage    =%d \n"
            "vid_format->fmt.pix.field        =%d \n"
            "vid_format->fmt.pix.bytesperline =%d \n"
            "vid_format->fmt.pix.colorspace   =%d \n",
            vid_format->type,
            vid_format->fmt.pix.width,
            vid_format->fmt.pix.height,
            vid_format->fmt.pix.pixelformat,
            vid_format->fmt.pix.sizeimage,
            vid_format->fmt.pix.field,
            vid_format->fmt.pix.bytesperline,
            vid_format->fmt.pix.colorspace);
}

int send_image(char* dev, char* image_path) {
    int fd = 0;
    struct v4l2_capability vid_caps = {0};
    struct v4l2_format vid_format = {0};
    size_t framesize = 0;
	size_t linewidth = 0;

    unsigned char* image;
    int image_width;
    int image_height;
    int image_nchannels;


    // load image
    image = stbi_load(image_path, &image_width, &image_height, &image_nchannels, 0);  // 0 means request the number of channels to be written to data->comp this next line is important to make sure that jpgs and pngs are both displayed correctly

    if(image == NULL) {
        printf("Failed to open/load image\n");
        return 1;
    } else {
         printf("Loaded image w: %d, h: %d, c: %d => pixel: %d, bytes: %d\n", image_width, image_height, image_nchannels, image_width * image_height, image_width * image_height*image_nchannels);
    }


    // open loopback device

    fd = open(dev, O_RDWR);

    if (fd < 0) {
        printf("Failed to open loopback device!\n");
        return 1;
    }

    // query device capabilities

    int query_cam = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
    if (query_cam == -1) {
        printf("Failed to query camera!\n");
        return 1;
    }

    // loading current fmt from device (this is only a sanity check)

    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT; // this is important else ioctl will return EINVAL, see:
                                                  // https://github.com/umlaeute/v4l2loopback/blob/main/examples/test.c#L128C1-L128C47
                                                  // https://www.kernel.org/doc/html/v4.12/media/uapi/v4l/vidioc-g-fmt.html
                                                  // for some strange reason this line is not present in the example at https://github.com/umlaeute/v4l2loopback/blob/main/examples/test.c#L128C1-L128C47
    int read_fmt = ioctl(fd, VIDIOC_G_FMT, &vid_format);
    if (read_fmt == -1) {
        printf("Failed to read fmt\n");
        return 1;
    }

    // set fmt

    // framesize is the number of bytes a frame contains = width * height * number_of_channels (RGB / RGBA)
    framesize = image_width * image_height * image_nchannels;

    // linewidth is the number of bytes a line consists of = width * number_of_channels (RGB / RGBA)
    linewidth = image_width * image_nchannels;

    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	vid_format.fmt.pix.width = image_width;
	vid_format.fmt.pix.height = image_height;
    vid_format.fmt.pix.pixelformat = image_nchannels == 4 ? V4L2_PIX_FMT_RGBA32:V4L2_PIX_FMT_RGB24;
	vid_format.fmt.pix.sizeimage = framesize;
	vid_format.fmt.pix.field = V4L2_FIELD_NONE;
	vid_format.fmt.pix.bytesperline = linewidth;
	vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    int put_fmt = ioctl(fd, VIDIOC_S_FMT, &vid_format);
    if (put_fmt == -1) {
        printf("Failed to write fmt\n");
        return 1;
    }

    print_format(&vid_format);

    while (1) {
        size_t x = write(fd, image, framesize);
        if (x != framesize) {
            printf("Failed to write full frame to loopback device");
            return 1;
        }
        usleep(1.0f/30 * 1000000.0f);
    }

    stbi_image_free(image);
    close(fd);
    return 0;
}

// https://wiki.delphigl.com/index.php/glBegin
#define OGLBQ0 -1,-1
#define OGLBQ1 -1,1
#define OGLBQ2 1,1
#define OGLBQ3 1,-1

#define OGLTX0 0,0
#define OGLTX1 0,1
#define OGLTX2 1,1
#define OGLTX3 1,0

int send_texture(char* dev, char* image_path) {

    // The plan
    // 1. Read image (bottom row first)
    // 2. Initialize the transfer buffer
    // 3. Initialize the loopback device
    // 4. Initialize OpenGL
    // 5. Draw the shader(image) to the target_texture (HACK: we flip the image rows here since it is cheaper than on the CPU)
    // 6. Flip the target_texture again and draw it to the window
    // 7. Copy the target_texture back to the CPU
    // 8. Write the image to the loopback device (no more flipping needed)


    unsigned char* image;
    int image_width;
    int image_height;
    int image_nchannels;

    int fd = 0;
    struct v4l2_capability vid_caps = {0};
    struct v4l2_format vid_format = {0};
    size_t framesize = 0;
	size_t linewidth = 0;

    GLFWwindow* gl_ctx;
    GLenum err;
    GLint result;
    char gl_error_buffer[1024] = {0};

    GLuint texture;
    GLuint shader;
    GLuint program;
    GLuint target_render_buffer;
    GLuint target_frame_buffer;
    GLenum target_draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
    GLuint target_texture;

    unsigned char* transfer_buffer;


    // STEP 1.: Read image (bottom row first)
    stbi_set_flip_vertically_on_load(true); // flipping becase opengl seams to expect bottom-row-first: https://stackoverflow.com/a/72120584
    image = stbi_load(image_path, &image_width, &image_height, &image_nchannels, 0);  // 0 means request the number of channels to be written to data->comp this next line is important to make sure that jpgs and pngs are both displayed correctly

    if(image == NULL) {
        printf("Failed to open/load image\n");
        return 1;
    } else {
         printf("Loaded image w: %d, h: %d, c: %d => pixel: %d, bytes: %d\n", image_width, image_height, image_nchannels, image_width * image_height, image_width * image_height * image_nchannels);
    }

    // STEP 2.: Initialize the transfer buffer
    transfer_buffer = malloc(sizeof(char)*image_width * image_height * image_nchannels);

    // STEP 3.: Initialize the loopback device

    // open the device
    fd = open(dev, O_RDWR);

    if (fd < 0) {
        printf("Failed to open loopback device!\n");
        return 1;
    }

    // query device capabilities
    int query_cam = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
    if (query_cam == -1) {
        printf("Failed to query camera!\n");
        return 1;
    }

    // loading current fmt from device (this is only a sanity check)
    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT; // this is important else ioctl will return EINVAL, see:
                                                  // https://github.com/umlaeute/v4l2loopback/blob/main/examples/test.c#L128C1-L128C47
                                                  // https://www.kernel.org/doc/html/v4.12/media/uapi/v4l/vidioc-g-fmt.html
                                                  // for some strange reason this line is not present in the example at https://github.com/umlaeute/v4l2loopback/blob/main/examples/test.c#L128C1-L128C47
    int read_fmt = ioctl(fd, VIDIOC_G_FMT, &vid_format);
    if (read_fmt == -1) {
        printf("Failed to read fmt\n");
        return 1;
    }

    // set fmt
    // framesize is the number of bytes a frame contains = width * height * number_of_channels (RGB / RGBA)
    framesize = image_width * image_height * image_nchannels;

    // linewidth is the number of bytes a line consists of = width * number_of_channels (RGB / RGBA)
    linewidth = image_width * image_nchannels;

    vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	vid_format.fmt.pix.width = image_width;
	vid_format.fmt.pix.height = image_height;
    vid_format.fmt.pix.pixelformat = image_nchannels == 4 ? V4L2_PIX_FMT_RGBA32:V4L2_PIX_FMT_RGB24;
	vid_format.fmt.pix.sizeimage = framesize;
	vid_format.fmt.pix.field = V4L2_FIELD_NONE;
	vid_format.fmt.pix.bytesperline = linewidth;
	vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

    int put_fmt = ioctl(fd, VIDIOC_S_FMT, &vid_format);
    if (put_fmt == -1) {
        printf("Failed to write fmt\n");
        return 1;
    }

    print_format(&vid_format);

    // STEP 4.: Initialize OpenGL

    // create opengl context + window
    glfwInit();
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    gl_ctx = glfwCreateWindow(image_width, image_width, "Debugging Wondow, This should be the same as the cam", NULL, NULL);
    if (!gl_ctx) {
        printf("Failed to create opengl context\n");
        return 1;
    }

    glfwMakeContextCurrent(gl_ctx);
    err = glewInit();
    if (GLEW_OK != err) {
        printf("Failed to init glew\n");
        return 1;
    }

    // load image to texture
    glEnable(GL_TEXTURE_2D);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, image_nchannels == 4 ? GL_RGBA : GL_RGB, image_width, image_height, 0, image_nchannels == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, image);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // create shader
   const char* shader_source = "#version 330\n\
                             out vec4 fragColor;\n\
                             uniform sampler2D tex;\n\
                             uniform float grey_value;\n\
                             layout(origin_upper_left) in vec4 gl_FragCoord; //HACK flip the texture on the GPU\n\
                             void main() {\n\
                                 vec3 fcolor = texture(tex, gl_FragCoord.xy/vec2(1000, 639)).xyz;\n\
                                 vec3 greyScale = vec3(grey_value, grey_value, grey_value);\n\
                                 fragColor.rgb = vec3(dot(fcolor, greyScale));\n\
                                 fragColor.a = 1;\n\
                             }";

    result = GL_FALSE;
    shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        glGetShaderInfoLog(shader, 1024, NULL, gl_error_buffer);
        printf("GL shader compilation failed: %s", gl_error_buffer);
        return 1;
    };

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    result = GL_FALSE;
    gl_error_buffer[0] = '\0';
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE) {
        glGetProgramInfoLog(program, 1024, NULL, gl_error_buffer);
        printf("GL Programm linking failed: %s", gl_error_buffer);
        return 1;
    }

    // create render target

    // generate the render framebuffer
    glGenFramebuffers(1, &target_frame_buffer);
    // bind our render framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, target_frame_buffer);

    // generate target texture
    glGenTextures(1, &target_texture);

    glBindTexture(GL_TEXTURE_2D, target_texture);
    // draw empty image to texture (also sets width and height)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // setup depth buffer
    glGenRenderbuffers(1, &target_render_buffer);
    glBindRenderbuffer(GL_RENDERBUFFER, target_render_buffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, image_width, image_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target_render_buffer);

    // set target texture as colour attachement #0
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target_texture, 0);

    // set draw buffers
    glDrawBuffers(1, target_draw_buffers);

    unsigned int progress = 0;
    while (1) {
        progress++;
        // SETP 5.: Draw the shader(image) to the target_texture
        glBindFramebuffer(GL_FRAMEBUFFER, target_frame_buffer);

        glBindTexture(GL_TEXTURE_2D, texture);

        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "tex"), 0);
        // animate the grey value to see some action an the cam
        glUniform1f(glGetUniformLocation(program, "grey_value"), cos((progress%128)/128.*2*3.1415)+1);

        /*for (size_t i = 0; i < (cos((progress%256)/256.*2*3.1415)+1) * 20; i++){
            printf("-");
        }
        printf("\n");*/


        glBegin(GL_QUADS);
        glTexCoord2f(OGLTX0);
        glVertex2f(OGLBQ0);
        glTexCoord2f(OGLTX1);
        glVertex2f(OGLBQ1);
        glTexCoord2f(OGLTX2);
        glVertex2f(OGLBQ2);
        glTexCoord2f(OGLTX3);
        glVertex2f(OGLBQ3);
        glEnd();

        // STEP 6.: Flip the target_texture again and draw it to the window
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(0, 0, image_width, image_height);
        glfwSetWindowSize(gl_ctx, image_width, image_height);

        glBindTexture(GL_TEXTURE_2D, target_texture);
        glUseProgram(0); // remove all used programs

        // note that the BQn and TXn are not machted any more this is because we fliped the texture in the shader
        glBegin(GL_QUADS);
        glTexCoord2f(OGLTX1);
        glVertex2f(OGLBQ0);
        glTexCoord2f(OGLTX0);
        glVertex2f(OGLBQ1);
        glTexCoord2f(OGLTX3);
        glVertex2f(OGLBQ2);
        glTexCoord2f(OGLTX2);
        glVertex2f(OGLBQ3);
        glEnd();

        // STEP 7.: Copy the target_texture back to the CPU

        // we need to copy from the target_frame_buffer, not the window's frame buffer
        glBindFramebuffer(GL_FRAMEBUFFER, target_frame_buffer);
        glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, transfer_buffer);

        glFlush();
        glfwSwapBuffers(gl_ctx);

        // SETP 8.: Write the image to the loopback device (no more flipping needed)
        size_t x = write(fd, transfer_buffer, framesize);
        if (x != framesize) {
            printf("Failed to write full frame to loopback device");
            return 1;
        }
        usleep(1.0f/30 * 1000000.0f);
    }

    // TODO free everything

    stbi_image_free(image);
    close(fd);
    return 0;
}

void usage(){
    printf(
            "Usage: main <command>\n\n"
            "Commands:\n"
            "   has-module                    check if v4l2loopback module is loaded\n"
            "   list-devices                  list loopback devices\n"
            "   is-loopback <device>          check is a given device is a loopback device\n"
            "   send-img <device> <image>     send a static image to the loopback device\n"
            "   send-texture <device> <image> read an image into an opengl texture, apply a shader and send it to a loopback device\n"
            );
}

int main(int argc, char *argv[]) {
    (void) argc;

    if (argc == 1) {
        usage();
        return 1;
    }

    if (strcmp("has-module", argv[1]) == 0) {
        bool has_mod = check_for_modlue();
        printf("Check: Module loaded: %b\n", has_mod);
        return !has_mod;
    }

    if (strcmp("list-devices", argv[1]) == 0) {
        if (!check_for_modlue()) {
            printf("Missing v4l2loopback module\n");
            return 1;
        }

        char** devices = list_video_devices();
        char** loopback_devices = list_loopback_devices(devices);
        for(size_t i = 0; loopback_devices[i] != NULL; i++) {
            printf("%s\n", loopback_devices[i]);
        }
        free_cpp(devices);
        free_cpp(loopback_devices);
        return 0;
    }


    if (strcmp("is-loopback", argv[1]) == 0) {

        if (!check_for_modlue()) {
            printf("Missing v4l2loopback module\n");
            return 1;
        }

        if (argc != 3) {
            usage();
            return 1;
        }

        if (is_loopback_device(argv[2])) {
            printf("Device '%s' is a v4l2 loopback device\n", argv[2]);
            return 0;
        } else {
            printf("Device '%s' is NOT a v4l2 loopback device\n", argv[2]);
            return 1;
        }
    }

    if (strcmp("send-img", argv[1]) == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        if (!is_loopback_device(argv[2])) {
            printf("Device '%s' is NOT a v4l2 loopback device\n", argv[2]);
            return 1;
        }
        return send_image(argv[2], argv[3]);
    }

    if (strcmp("send-texture", argv[1]) == 0) {
        if (argc != 4) {
            usage();
            return 1;
        }

        if (!is_loopback_device(argv[2])) {
            printf("Device '%s' is NOT a v4l2 loopback device\n", argv[2]);
            return 1;
        }
        return send_texture(argv[2], argv[3]);
    }


    usage();
    return EXIT_FAILURE;
}

