#pragma once

#include "dirutils.cc"
#include "op.cc"
#include "strslice.cc"
#include "utils.cc"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdio.h>
#include <string.h> // for memset
#include <sys/types.h>
#include <utility> // for std::move

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar *message, const void *userParam) {
    fprintf(stderr,
            "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type,
            severity, message);
}

char const QuadVertShaderText[] = R"(
#version 330 core

in vec2 n_pos;
in vec2 n_tex_p;

out vec2 tex_p;

void main() {
    tex_p = n_tex_p;
    gl_Position = vec4(n_pos, 0.0, 1.0);
}
)";

char const QuadFragShaderText[] = R"(
#version 330 core

in vec2 tex_p;

uniform sampler2D tex;

out vec4 color;

void main() {
    color = texture(tex, tex_p);
}
)";

struct Shader {
    GLuint shader;

    Shader() : shader(0) {}
    Shader(GLenum type) : shader(glCreateShader(type)) {}
    Shader(Shader &&other) : shader(other.shader) { other.shader = 0; }
    Shader(Shader const &other) = delete;

    ~Shader() {
        if (this->shader) {
            printf("Deleting a shader\n");
        }

        glDeleteShader(this->shader);
        this->shader = 0;
    }

    Shader &operator=(Shader &&other) {
        Shader tmp{std::move(*this)};

        this->shader = other.shader;
        other.shader = 0;

        return *this;
    }

    auto source(StrSlice source_slice) -> void {
        GLint source_len = source_slice.len;
        glShaderSource(this->shader, 1, &source_slice.ptr, &source_len);
    }

    auto compile() -> bool {
        glCompileShader(this->shader);

        GLint status = GL_FALSE;
        glGetShaderiv(this->shader, GL_COMPILE_STATUS, &status);

        if (status != GL_TRUE) {
            GLint len = 0;
            glGetShaderiv(this->shader, GL_INFO_LOG_LENGTH, &len);

            char *log = new char[len];
            glGetShaderInfoLog(this->shader, len, nullptr, log);
            fprintf(stderr, "Shader Log: %.*s\n", len, log);

            delete[] log;

            return false;
        }
        return true;
    }

    static auto fromSourceOp(GLenum type, StrSlice source_slice) -> Op<Shader> {
        Shader s{type};

        s.source(source_slice);
        if (!s.compile()) {
            return Op<Shader>::empty();
        }

        return s;
    }

    static auto fromSource(GLenum type, StrSlice source_slice) -> Shader {
        Op<Shader> op_shader = Shader::fromSourceOp(type, source_slice);

        Shader result{std::move(op_shader.get())};
        return result;
    }
};

struct Program {
    GLuint program;

    Program() : program(glCreateProgram()) {}
    Program(Program const &other) = delete;
    Program(Program &&other) : program(other.program) { other.program = 0; }

    ~Program() {
        if (this->program) {
            printf("Deleting a program\n");
        }

        glDeleteProgram(program);
        this->program = 0;
    }

    Program &operator=(Program &&other) {
        Program tmp{std::move(*this)};

        this->program = other.program;
        other.program = 0;

        return *this;
    }

    auto attachShader(Shader &shader) -> void {
        glAttachShader(this->program, shader.shader);
    }

    auto link() -> bool {
        glLinkProgram(this->program);

        GLint status = GL_FALSE;
        glGetProgramiv(this->program, GL_LINK_STATUS, &status);

        if (status != GL_TRUE) {
            GLint len = 0;
            glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &len);

            char *log = new char[len];
            glGetProgramInfoLog(this->program, len, nullptr, log);
            fprintf(stderr, "Program Log: %.*s\n", len, log);

            delete[] log;

            return false;
        }
        return true;
    }

    auto useProgram() -> void { glUseProgram(this->program); }
};

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct Texture {
    GLuint texture;

    Texture() : texture(0) {
        glGenTextures(1, &this->texture);

        this->useTex();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    Texture(Texture const &other) = delete;
    Texture(Texture &&other) : texture(other.texture) { other.texture = 0; }

    ~Texture() {
        if (texture) {
            printf("Deleting a texture\n");
        }

        glDeleteTextures(1, &this->texture);
    }

    Texture &operator=(Texture &&other) {
        Texture tmp{std::move(*this)};

        this->texture = other.texture;
        other.texture = 0;

        return *this;
    }

    auto useTex() -> void { glBindTexture(GL_TEXTURE_2D, this->texture); }

    auto solidColor2D(Color color, Dims dims) -> void {
        glBindTexture(GL_TEXTURE_2D, this->texture);

        size_t data_len = dims.width * dims.height * 3;
        unsigned char *data = new unsigned char[data_len];

        for (size_t i = 0; i < data_len; i += 3) {
            data[i + 0] = color.r;
            data[i + 1] = color.g;
            data[i + 2] = color.b;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dims.width, dims.height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, data);

        delete[] data;
    }
};

inline auto toGlLoc(size_t val, size_t range) -> GLfloat {
    double pct = static_cast<double>(val) / static_cast<double>(range);
    double gl = pct * 2.0 - 1.0;

    return static_cast<GLfloat>(gl);
}

struct QuadProgram {
    Program program;
    GLint n_pos;
    GLint n_tex_p;
    GLint tex;

    GLuint vao;
    GLuint vbo;

    Texture texture;

    QuadProgram(Program &&program, GLint n_pos, GLint n_tex_p, GLint tex,
                Texture &&texture)
        : program(std::move(program)), n_pos(n_pos), n_tex_p(n_tex_p), tex(tex),
          texture(std::move(texture)) {
        glGenVertexArrays(1, &this->vao);
        glGenBuffers(1, &this->vbo);

        this->program.useProgram();
        glBindVertexArray(this->vao);
        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glEnableVertexAttribArray(this->n_pos);
        glEnableVertexAttribArray(this->n_tex_p);

        glVertexAttribPointer(this->n_pos, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(0 * sizeof(GLfloat)));
        glVertexAttribPointer(this->n_tex_p, 2, GL_FLOAT, GL_FALSE,
                              4 * sizeof(GLfloat),
                              (void const *)(2 * sizeof(GLfloat)));
    }
    QuadProgram(QuadProgram const &other) = delete;
    QuadProgram(QuadProgram &&other)
        : program(std::move(other.program)), n_pos(other.n_pos),
          n_tex_p(other.n_tex_p), tex(other.tex), vao(other.vao),
          vbo(other.vbo), texture(std::move(other.texture)) {
        other.vao = 0;
        other.vbo = 0;
    }

    ~QuadProgram() {
        glDeleteVertexArrays(1, &this->vao);
        glDeleteBuffers(1, &this->vbo);
    }

    QuadProgram &operator=(QuadProgram &&other) {
        QuadProgram tmp{std::move(*this)};

        this->program = std::move(other.program);
        this->n_pos = other.n_pos;
        this->n_tex_p = other.n_tex_p;
        this->tex = other.tex;
        this->vao = other.vao;
        this->vbo = other.vbo;
        this->texture = std::move(other.texture);

        other.vao = 0;
        other.vbo = 0;

        return *this;
    }

    auto renderAt(Location loc, Dims dims, Dims window_dims) {
        this->program.useProgram();
        this->setPosition(loc, dims, window_dims);

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(this->tex, 0); // GL_TEXTURE0
        this->texture.useTex();    // bind texture to GL_TEXTURE0

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    auto setPosition(Location loc, Dims dims, Dims window_dims) -> void {
        glBindVertexArray(this->vao);

        Location nw = loc;
        Location ne = Location{loc.row, loc.col + dims.width};
        Location sw = Location{loc.row + dims.height, loc.col};
        Location se = Location{loc.row + dims.height, loc.col + dims.width};

        GLfloat nw_r = toGlLoc(nw.row, window_dims.width);
        GLfloat nw_c = -toGlLoc(nw.col, window_dims.height);
        GLfloat ne_r = toGlLoc(ne.row, window_dims.width);
        GLfloat ne_c = -toGlLoc(ne.col, window_dims.height);
        GLfloat sw_r = toGlLoc(sw.row, window_dims.width);
        GLfloat sw_c = -toGlLoc(sw.col, window_dims.height);
        GLfloat se_r = toGlLoc(se.row, window_dims.width);
        GLfloat se_c = -toGlLoc(se.col, window_dims.height);

        GLfloat data[] = {
            nw_r, nw_c, 0.0f, 0.0f, //
            sw_r, sw_c, 1.0f, 0.0f, //
            ne_r, ne_c, 0.0f, 1.0f, //
            se_r, se_c, 1.0f, 1.0f, //
        };

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    }

    auto swapTexture(Texture &&other) -> Texture {
        Texture tmp{std::move(this->texture)};
        this->texture = std::move(other);
        return tmp;
    }
};

struct GLFW;

template <typename T> struct Window {
    GLFW &ref; // ensure we have initialized
    GLFWwindow *window;
    T *ctx;

    Shader QuadFragShader;
    Shader QuadVertShader;

    Window(GLFW &ref, int width, int height, char const *title, T *ctx)
        : ref(ref),
          window(glfwCreateWindow(width, height, title, nullptr, nullptr)),
          ctx(ctx) {
        glfwSetWindowUserPointer(this->window, this->ctx);
        glfwMakeContextCurrent(this->window);

        // During init, enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(MessageCallback, 0);

        this->QuadVertShader =
            Shader::fromSource(GL_VERTEX_SHADER, STR_SLICE(QuadVertShaderText));

        this->QuadFragShader = Shader::fromSource(
            GL_FRAGMENT_SHADER, STR_SLICE(QuadFragShaderText));
    }

    ~Window() { glfwDestroyWindow(this->window); }

    auto shouldClose() -> bool { return glfwWindowShouldClose(this->window); }

    auto close() -> void { glfwSetWindowShouldClose(this->window, 1); }

    auto setPos(int x, int y) -> void { glfwSetWindowPos(this->window, x, y); }

    auto show() -> void { glfwShowWindow(this->window); }

    auto pollAndSwap() -> void {
        glfwPollEvents();
        glfwSwapBuffers(this->window);
    }

    auto getContext() -> T * {
        void *user_ptr = glfwGetWindowUserPointer(this->window);
        assert(user_ptr != nullptr && "Invalid user pointer");
        return static_cast<T *>(user_ptr);
    }

    auto getKey(int key) -> int { return glfwGetKey(this->window, key); }

    auto isKeyPressed(int key) -> bool {
        return this->getKey(key) == GLFW_PRESS;
    }

    auto isKeyRelease(int key) -> bool {
        return this->getKey(key) == GLFW_RELEASE;
    }

    auto getDims() -> Dims {
        int width, height;
        glfwGetWindowSize(this->window, &width, &height);

        assert(width >= 0 && "Invalid width");
        assert(height >= 0 && "Invalid height");

        return Dims{static_cast<size_t>(width), static_cast<size_t>(height)};
    }

    auto setFramebufferSizeCallback(GLFWcursorposfun callback) -> void {
        glfwSetFramebufferSizeCallback(this->window, callback);
    }

    auto setCursorPosCallback(GLFWcursorposfun callback) -> void {
        glfwSetCursorPosCallback(this->window, callback);
    }

    auto setMouseButtonCallback(GLFWmousebuttonfun callback) -> void {
        glfwSetMouseButtonCallback(this->window, callback);
    }

    auto quadProgramOp() -> Op<QuadProgram> {
        Program p{};

        p.attachShader(this->QuadVertShader);
        p.attachShader(this->QuadFragShader);
        if (!p.link()) {
            return Op<QuadProgram>::empty();
        }

        glUseProgram(p.program);
        GLint n_pos = glGetAttribLocation(p.program, "n_pos");
        GLint n_tex_p = glGetAttribLocation(p.program, "n_tex_p");
        GLint tex = glGetUniformLocation(p.program, "tex");

        return QuadProgram{std::move(p), n_pos, n_tex_p, tex, Texture{}};
    }

    auto quadProgram() -> QuadProgram {
        Op<QuadProgram> op_program = Window::quadProgramOp();

        QuadProgram result{std::move(op_program.get())};
        return result;
    }

    auto renderQuad(QuadProgram &p, Location loc, Dims dims) -> void {
        Dims window_dims = this->getDims();
        p.renderAt(loc, dims, window_dims);
    }
};

struct GLFW {
    GLFW(GLFWerrorfun callback) {
        glfwSetErrorCallback(callback);

        int res = glfwInit();
        if (res < 0) {
            fprintf(stderr, "Failed to init glfw");
            EXIT(1);
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    ~GLFW() { glfwTerminate(); }

    auto setErrorCallback(GLFWerrorfun callback) -> void {
        glfwSetErrorCallback(callback);
    }

    template <typename T>
    auto makeWindow(int width, int height, char const *title, T *ctx)
        -> Window<T> {
        return Window<T>{*this, width, height, title, ctx};
    }
};
