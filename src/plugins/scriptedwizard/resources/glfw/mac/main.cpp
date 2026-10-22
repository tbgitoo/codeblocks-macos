#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include <cstdio>

static void error_callback(int, const char* description)
{
    std::fprintf(stderr, "%s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static GLuint compile_shader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof log, NULL, log);
        std::fprintf(stderr, "%s\n", log);
        glDeleteShader(shader);
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

    GLFWwindow* window = glfwCreateWindow(640, 480, "GLFW", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
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

    const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex_shader == 0 || fragment_shader == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof log, NULL, log);
        std::fprintf(stderr, "%s\n", log);
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    const float vertices[] = {
        -0.6f, -0.4f, 0.0f,
         0.6f, -0.4f, 0.0f,
         0.0f,  0.6f, 0.0f
    };

    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glBindVertexArray(0);

    glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
    while (!glfwWindowShouldClose(window))
    {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glBindVertexArray(vertex_array);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vertex_array);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
