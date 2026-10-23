// GLFW 3.5 on Windows. opengl32 exports OpenGL 1.1, so the 4.1 core
// calls are loaded with glfwGetProcAddress after the context is current.
#define GLFW_INCLUDE_NONE
#define GLFW_DLL
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdio>

// Constants the loaded calls need. They are not declared by glfw3.h.
enum Gl : unsigned int
{
    GL_FALSE = 0,
    GL_TRUE = 1,
    GL_FLOAT = 0x1406,
    GL_TRIANGLES = 0x0004,
    GL_COLOR_BUFFER_BIT = 0x00004000,
    GL_ARRAY_BUFFER = 0x8892,
    GL_STATIC_DRAW = 0x88E4,
    GL_VERTEX_SHADER = 0x8B31,
    GL_FRAGMENT_SHADER = 0x8B30,
    GL_COMPILE_STATUS = 0x8B81,
    GL_LINK_STATUS = 0x8B82
};

typedef unsigned int Gluint;
typedef int Glint;
typedef unsigned int Glenum;
typedef int Glsizei;
typedef std::ptrdiff_t Glintptr;
typedef char Glchar;

#ifndef APIENTRY
#define APIENTRY __stdcall
#endif

typedef Gluint (APIENTRY *PFN_glCreateShader)(Glenum);
typedef void (APIENTRY *PFN_glShaderSource)(Gluint, Glsizei, const Glchar* const*, const Glint*);
typedef void (APIENTRY *PFN_glCompileShader)(Gluint);
typedef void (APIENTRY *PFN_glGetShaderiv)(Gluint, Glenum, Glint*);
typedef void (APIENTRY *PFN_glGetShaderInfoLog)(Gluint, Glsizei, Glsizei*, Glchar*);
typedef void (APIENTRY *PFN_glDeleteShader)(Gluint);
typedef Gluint (APIENTRY *PFN_glCreateProgram)(void);
typedef void (APIENTRY *PFN_glAttachShader)(Gluint, Gluint);
typedef void (APIENTRY *PFN_glLinkProgram)(Gluint);
typedef void (APIENTRY *PFN_glGetProgramiv)(Gluint, Glenum, Glint*);
typedef void (APIENTRY *PFN_glGetProgramInfoLog)(Gluint, Glsizei, Glsizei*, Glchar*);
typedef void (APIENTRY *PFN_glUseProgram)(Gluint);
typedef void (APIENTRY *PFN_glDeleteProgram)(Gluint);
typedef void (APIENTRY *PFN_glGenBuffers)(Glsizei, Gluint*);
typedef void (APIENTRY *PFN_glBindBuffer)(Glenum, Gluint);
typedef void (APIENTRY *PFN_glBufferData)(Glenum, Glintptr, const void*, Glenum);
typedef void (APIENTRY *PFN_glDeleteBuffers)(Glsizei, const Gluint*);
typedef void (APIENTRY *PFN_glGenVertexArrays)(Glsizei, Gluint*);
typedef void (APIENTRY *PFN_glBindVertexArray)(Gluint);
typedef void (APIENTRY *PFN_glDeleteVertexArrays)(Glsizei, const Gluint*);
typedef void (APIENTRY *PFN_glVertexAttribPointer)(Gluint, Glint, Glenum, unsigned char, Glsizei, const void*);
typedef void (APIENTRY *PFN_glEnableVertexAttribArray)(Gluint);
typedef void (APIENTRY *PFN_glDrawArrays)(Glenum, Glint, Glsizei);
typedef void (APIENTRY *PFN_glClear)(unsigned int);
typedef void (APIENTRY *PFN_glViewport)(Glint, Glint, Glsizei, Glsizei);
typedef void (APIENTRY *PFN_glClearColor)(float, float, float, float);

static PFN_glCreateShader p_glCreateShader;
static PFN_glShaderSource p_glShaderSource;
static PFN_glCompileShader p_glCompileShader;
static PFN_glGetShaderiv p_glGetShaderiv;
static PFN_glGetShaderInfoLog p_glGetShaderInfoLog;
static PFN_glDeleteShader p_glDeleteShader;
static PFN_glCreateProgram p_glCreateProgram;
static PFN_glAttachShader p_glAttachShader;
static PFN_glLinkProgram p_glLinkProgram;
static PFN_glGetProgramiv p_glGetProgramiv;
static PFN_glGetProgramInfoLog p_glGetProgramInfoLog;
static PFN_glUseProgram p_glUseProgram;
static PFN_glDeleteProgram p_glDeleteProgram;
static PFN_glGenBuffers p_glGenBuffers;
static PFN_glBindBuffer p_glBindBuffer;
static PFN_glBufferData p_glBufferData;
static PFN_glDeleteBuffers p_glDeleteBuffers;
static PFN_glGenVertexArrays p_glGenVertexArrays;
static PFN_glBindVertexArray p_glBindVertexArray;
static PFN_glDeleteVertexArrays p_glDeleteVertexArrays;
static PFN_glVertexAttribPointer p_glVertexAttribPointer;
static PFN_glEnableVertexAttribArray p_glEnableVertexAttribArray;
static PFN_glDrawArrays p_glDrawArrays;
static PFN_glClear p_glClear;
static PFN_glViewport p_glViewport;
static PFN_glClearColor p_glClearColor;

static bool load_entry(const char* name, void** slot)
{
    *slot = reinterpret_cast<void*>(glfwGetProcAddress(name));
    if (*slot == nullptr)
    {
        std::fprintf(stderr, "missing OpenGL entry point: %s\n", name);
        return false;
    }
    return true;
}

static bool load_gl()
{
    return load_entry("glCreateShader", reinterpret_cast<void**>(&p_glCreateShader))
        && load_entry("glShaderSource", reinterpret_cast<void**>(&p_glShaderSource))
        && load_entry("glCompileShader", reinterpret_cast<void**>(&p_glCompileShader))
        && load_entry("glGetShaderiv", reinterpret_cast<void**>(&p_glGetShaderiv))
        && load_entry("glGetShaderInfoLog", reinterpret_cast<void**>(&p_glGetShaderInfoLog))
        && load_entry("glDeleteShader", reinterpret_cast<void**>(&p_glDeleteShader))
        && load_entry("glCreateProgram", reinterpret_cast<void**>(&p_glCreateProgram))
        && load_entry("glAttachShader", reinterpret_cast<void**>(&p_glAttachShader))
        && load_entry("glLinkProgram", reinterpret_cast<void**>(&p_glLinkProgram))
        && load_entry("glGetProgramiv", reinterpret_cast<void**>(&p_glGetProgramiv))
        && load_entry("glGetProgramInfoLog", reinterpret_cast<void**>(&p_glGetProgramInfoLog))
        && load_entry("glUseProgram", reinterpret_cast<void**>(&p_glUseProgram))
        && load_entry("glDeleteProgram", reinterpret_cast<void**>(&p_glDeleteProgram))
        && load_entry("glGenBuffers", reinterpret_cast<void**>(&p_glGenBuffers))
        && load_entry("glBindBuffer", reinterpret_cast<void**>(&p_glBindBuffer))
        && load_entry("glBufferData", reinterpret_cast<void**>(&p_glBufferData))
        && load_entry("glDeleteBuffers", reinterpret_cast<void**>(&p_glDeleteBuffers))
        && load_entry("glGenVertexArrays", reinterpret_cast<void**>(&p_glGenVertexArrays))
        && load_entry("glBindVertexArray", reinterpret_cast<void**>(&p_glBindVertexArray))
        && load_entry("glDeleteVertexArrays", reinterpret_cast<void**>(&p_glDeleteVertexArrays))
        && load_entry("glVertexAttribPointer", reinterpret_cast<void**>(&p_glVertexAttribPointer))
        && load_entry("glEnableVertexAttribArray", reinterpret_cast<void**>(&p_glEnableVertexAttribArray))
        && load_entry("glDrawArrays", reinterpret_cast<void**>(&p_glDrawArrays))
        && load_entry("glClear", reinterpret_cast<void**>(&p_glClear))
        && load_entry("glViewport", reinterpret_cast<void**>(&p_glViewport))
        && load_entry("glClearColor", reinterpret_cast<void**>(&p_glClearColor));
}

static void error_callback(int, const char* description)
{
    std::fprintf(stderr, "%s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static Gluint compile_shader(Glenum type, const char* source)
{
    const Gluint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &source, nullptr);
    p_glCompileShader(shader);

    Glint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        p_glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        std::fprintf(stderr, "%s\n", log);
        p_glDeleteShader(shader);
        return 0;
    }
    return shader;
}

int main()
{
    glfwSetErrorCallback(error_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(640, 480, "GLFW", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    const int major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    const int minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    const int profile = glfwGetWindowAttrib(window, GLFW_OPENGL_PROFILE);
    if (major < 4 || (major == 4 && minor < 1) || profile != GLFW_OPENGL_CORE_PROFILE)
    {
        std::fprintf(stderr, "OpenGL context is %d.%d profile %d; 4.1 core is required\n",
                     major, minor, profile);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    if (!load_gl())
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwSetKeyCallback(window, key_callback);

    const char* vertex_source =
        "#version 410 core\n"
        "layout (location = 0) in vec3 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    const char* fragment_source =
        "#version 410 core\n"
        "out vec4 colour;\n"
        "void main()\n"
        "{\n"
        "    colour = vec4(0.90, 0.25, 0.20, 1.0);\n"
        "}\n";

    const Gluint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const Gluint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex_shader == 0 || fragment_shader == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const Gluint program = p_glCreateProgram();
    p_glAttachShader(program, vertex_shader);
    p_glAttachShader(program, fragment_shader);
    p_glLinkProgram(program);
    p_glDeleteShader(vertex_shader);
    p_glDeleteShader(fragment_shader);

    Glint linked = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[512];
        p_glGetProgramInfoLog(program, sizeof log, nullptr, log);
        std::fprintf(stderr, "%s\n", log);
        p_glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const float vertices[] = {
        -0.6f, -0.4f, 0.0f,
         0.6f, -0.4f, 0.0f,
         0.0f,  0.6f, 0.0f
    };

    Gluint vertex_array = 0;
    Gluint vertex_buffer = 0;
    p_glGenVertexArrays(1, &vertex_array);
    p_glGenBuffers(1, &vertex_buffer);
    p_glBindVertexArray(vertex_array);
    p_glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    p_glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    p_glEnableVertexAttribArray(0);
    p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    p_glBindVertexArray(0);

    p_glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
    while (!glfwWindowShouldClose(window))
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        p_glViewport(0, 0, width, height);
        p_glClear(GL_COLOR_BUFFER_BIT);
        p_glUseProgram(program);
        p_glBindVertexArray(vertex_array);
        p_glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    p_glDeleteVertexArrays(1, &vertex_array);
    p_glDeleteBuffers(1, &vertex_buffer);
    p_glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
