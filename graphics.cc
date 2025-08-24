#pragma once

#include "arena.cc"
#include "dirutils.cc"
#include "fileutils.cc"
#include "op.cc"
#include "strslice.cc"
#include "utils.cc"

#define STB_TRUETYPE_IMPLEMENTATION
#include "extern/stb_truetype.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdio.h>
#include <string.h> // for memset
#include <sys/types.h>
#include <utility>

#if __APPLE__
// this does not seem to exist on my mac
#define glDebugMessageCallback(...)
#endif // __APPLE__

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

char const FontFragShaderText[] = R"(
#version 330 core

in vec2 tex_p;

uniform vec3 font_color;
uniform sampler2D tex;

out vec4 color;

void main() {
    float alpha = texture(tex, tex_p).r;
    color = vec4(font_color, alpha);
}
)";

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
};

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

    static auto grayscale(unsigned char g) -> Color { return Color{g, g, g}; }
};

struct Texture2D {
    GLuint texture;

    Texture2D() : texture(0) {
        glGenTextures(1, &this->texture);

        this->useTex();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    Texture2D(Texture2D const &other) = delete;
    Texture2D(Texture2D &&other) : texture(other.texture) { other.texture = 0; }

    ~Texture2D() {
        if (texture) {
            printf("Deleting a texture\n");
        }

        glDeleteTextures(1, &this->texture);
    }

    Texture2D &operator=(Texture2D &&other) {
        Texture2D tmp{std::move(*this)};

        this->texture = other.texture;
        other.texture = 0;

        return *this;
    }

    auto useTex() -> void { glBindTexture(GL_TEXTURE_2D, this->texture); }

    auto solidColor(Color color, Dims dims) -> void {
        this->useTex();

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

    auto grayscale(unsigned char g, Dims dims) {
        solidColor(Color::grayscale(g), dims);
    }

    auto bindAlphaData(Dims dims, unsigned char *pixels) -> void {
        this->useTex();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, dims.width, dims.height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, pixels);
    }
};

inline auto toGlLoc(size_t val, size_t range) -> GLfloat {
    double pct = static_cast<double>(val) / static_cast<double>(range);
    double gl = pct * 2.0 - 1.0;

    return static_cast<GLfloat>(gl);
}

struct QuadProgram {
    struct WithGuard {
        QuadProgram &ref;
        Texture2D &texture;

        ~WithGuard() { this->ref.swapTexture(this->texture); }
    };

    Program program;
    GLint n_pos;
    GLint n_tex_p;
    GLint tex;

    GLuint vao;
    GLuint vbo;

    Texture2D texture;

    QuadProgram(Program &&program, GLint n_pos, GLint n_tex_p, GLint tex,
                Texture2D &&texture)
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

    auto renderAt(Location rect_loc, Dims rect_dims, float tex_loc_x0,
                  float tex_loc_y0, float tex_loc_x1, float tex_loc_y1,
                  Dims window_dims) -> void {
        this->program.useProgram();
        this->setPosition(rect_loc, rect_dims, tex_loc_x0, tex_loc_y0,
                          tex_loc_x1, tex_loc_y1, window_dims);

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(this->tex, 0); // GL_TEXTURE0
        this->texture.useTex();    // bind texture to GL_TEXTURE0

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    auto renderAt(Location loc, Dims dims, Dims window_dims) -> void {
        this->renderAt(loc, dims, 0.0f, 0.0f, 1.0f, 1.0f, window_dims);
    }

    auto setPosition(Location rect_loc, Dims rect_dims, float tex_loc_x0,
                     float tex_loc_y0, float tex_loc_x1, float tex_loc_y1,
                     Dims window_dims) -> void {
        glBindVertexArray(this->vao);

        Location rnw = rect_loc;
        Location rne = Location{rect_loc.row, rect_loc.col + rect_dims.width};
        Location rsw = Location{rect_loc.row + rect_dims.height, rect_loc.col};
        Location rse = Location{rect_loc.row + rect_dims.height,
                                rect_loc.col + rect_dims.width};

        size_t ww = window_dims.width;
        size_t wh = window_dims.height;

        GLfloat nw_r = toGlLoc(wh - rnw.row, wh);
        GLfloat nw_c = toGlLoc(rnw.col, ww);
        GLfloat ne_r = toGlLoc(wh - rne.row, wh);
        GLfloat ne_c = toGlLoc(rne.col, ww);
        GLfloat sw_r = toGlLoc(wh - rsw.row, wh);
        GLfloat sw_c = toGlLoc(rsw.col, ww);
        GLfloat se_r = toGlLoc(wh - rse.row, wh);
        GLfloat se_c = toGlLoc(rse.col, ww);

        GLfloat data[] = {
            nw_c, nw_r, tex_loc_x0, tex_loc_y0, //
            sw_c, sw_r, tex_loc_x0, tex_loc_y1, //
            ne_c, ne_r, tex_loc_x1, tex_loc_y0, //
            se_c, se_r, tex_loc_x1, tex_loc_y1, //
        };

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    }

    auto setPosition(Location loc, Dims dims, Dims window_dims) -> void {
        glBindVertexArray(this->vao);

        Location nw = loc;
        Location ne = Location{loc.row, loc.col + dims.width};
        Location sw = Location{loc.row + dims.height, loc.col};
        Location se = Location{loc.row + dims.height, loc.col + dims.width};

        GLfloat nw_r = toGlLoc(window_dims.height - nw.row, window_dims.height);
        GLfloat nw_c = toGlLoc(nw.col, window_dims.width);
        GLfloat ne_r = toGlLoc(window_dims.height - ne.row, window_dims.height);
        GLfloat ne_c = toGlLoc(ne.col, window_dims.width);
        GLfloat sw_r = toGlLoc(window_dims.height - sw.row, window_dims.height);
        GLfloat sw_c = toGlLoc(sw.col, window_dims.width);
        GLfloat se_r = toGlLoc(window_dims.height - se.row, window_dims.height);
        GLfloat se_c = toGlLoc(se.col, window_dims.width);

        GLfloat data[] = {
            nw_c, nw_r, 0.0f, 0.0f, //
            sw_c, sw_r, 1.0f, 0.0f, //
            ne_c, ne_r, 0.0f, 1.0f, //
            se_c, se_r, 1.0f, 1.0f, //
        };

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    }

    auto swapTexture(Texture2D &other) -> void {
        std::swap(this->texture, other);
    }

    auto withTexture(Texture2D &texture) -> WithGuard {
        this->swapTexture(texture);
        return WithGuard{*this, texture};
    }
};

inline auto glColor(unsigned char c) -> GLfloat {
    GLfloat c_float = c;
    return c_float / 255.0f;
}

struct BakedFont {
    float pixel_height;
    Dims bmp_dims;
    stbtt_bakedchar chardata[96]; // printable characters
    QuadProgram quad_program;
    GLint font_color;

    BakedFont(float pixel_height, Dims bmp_dims, stbtt_bakedchar chardata[96],
              QuadProgram &&quad_progam, GLint font_color)
        : pixel_height(pixel_height), bmp_dims{bmp_dims}, chardata{},
          quad_program{std::move(quad_progam)}, font_color(font_color) {
        memcpy(this->chardata, chardata, 96 * sizeof(*chardata));
    }

    auto setColor(Color color) -> void {
        this->quad_program.program.useProgram();

        GLfloat color_vec[3]{
            glColor(color.r),
            glColor(color.g),
            glColor(color.b),
        };

        glUniform3fv(this->font_color, 1, color_vec);
    }

    auto renderTextBaseline(Location loc, StrSlice text, Dims window_dims)
        -> void {
        float xpos = loc.col;
        float ypos = loc.row;
        for (char c : text) {
            // fallback to Space
            char render_c = inRange<char>(32, c, 127) ? c : 32;

            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(this->chardata, this->bmp_dims.width,
                               this->bmp_dims.height, render_c - 32, &xpos,
                               &ypos, &q, 1);

            Location char_loc_ul{static_cast<size_t>(q.y0),
                                 static_cast<size_t>(q.x0)};
            Location char_loc_br{static_cast<size_t>(q.y1),
                                 static_cast<size_t>(q.x1)};

            Dims char_dims = Dims::fromRect(char_loc_ul, char_loc_br);

            this->quad_program.renderAt(char_loc_ul, char_dims, q.s0, q.t0,
                                        q.s1, q.t1, window_dims);
        }
    }

    auto renderText(Location loc, StrSlice text, Dims window_dims) -> void {
        size_t new_row = loc.row + this->pixel_height;
        this->renderTextBaseline(Location{new_row, loc.col}, text, window_dims);
    }
};

template <typename T> struct Window;

template <typename T> struct HasFramebufferSizeCallback {
    template <typename U,
              typename = decltype(std::declval<U>().framebufferSizeCallback(
                  std::declval<Window<U> &>(), std::declval<int>(),
                  std::declval<int>()))>
    static std::true_type test(int);

    template <typename> static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(std::declval<int>()))::value;
};

template <typename T> struct HasCursorPosCallback {
    template <typename U,
              typename = decltype(std::declval<U>().cursorPosCallback(
                  std::declval<Window<U> &>(), std::declval<double>(),
                  std::declval<double>()))>
    static std::true_type test(int);

    template <typename> static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(std::declval<int>()))::value;
};

template <typename T> struct HasMouseButtonCallback {
    template <typename U,
              typename = decltype(std::declval<U>().mouseButtonCallback(
                  std::declval<Window<U> &>(), std::declval<int>(),
                  std::declval<int>(), std::declval<int>()))>
    static std::true_type test(int);

    template <typename> static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(std::declval<int>()))::value;
};

template <typename T> struct HasRender {
    template <typename U, typename = decltype(std::declval<U>().render(
                              std::declval<Window<U> &>()))>
    static std::true_type test(int);

    template <typename> static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(std::declval<int>()))::value;
};

template <typename T> struct HasProcessEvents {
    template <typename U, typename = decltype(std::declval<U>().processEvents(
                              std::declval<Window<U> &>()))>
    static std::true_type test(int);

    template <typename> static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(std::declval<int>()))::value;
};

auto getInitWindow(int width, int height, char const *title) -> GLFWwindow * {
    GLFWwindow *w = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwMakeContextCurrent(w);

    // During init, enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    return w;
}

template <typename T> struct Window {
    GLFWwindow *window;
    Shader QuadVertShader;
    Shader QuadFragShader;
    Shader FontFragShader;
    T ctx;

    bool needs_render;

    template <typename... Args>
    Window(int width, int height, char const *title, Args &&...args)
        : window(getInitWindow(width, height, title)),
          QuadVertShader{Shader::fromSource(GL_VERTEX_SHADER,
                                            STR_SLICE(QuadVertShaderText))},
          QuadFragShader{Shader::fromSource(GL_FRAGMENT_SHADER,
                                            STR_SLICE(QuadFragShaderText))},
          FontFragShader{Shader::fromSource(GL_FRAGMENT_SHADER,
                                            STR_SLICE(FontFragShaderText))},
          ctx(*this, std::forward<Args>(args)...), needs_render(true) {
        glfwSetFramebufferSizeCallback(this->window, &windowFramebufferSize);
        glfwSetCursorPosCallback(this->window, &windowCursorPos);
        glfwSetMouseButtonCallback(this->window, &windowMouseButton);
    }

    ~Window() { glfwDestroyWindow(this->window); }

    static void windowFramebufferSize(GLFWwindow *window, int width,
                                      int height) {
        if constexpr (HasFramebufferSizeCallback<T>::value) {
            void *user_ptr = glfwGetWindowUserPointer(window);
            if (user_ptr == nullptr) {
                return;
            }

            Window *wrapped = static_cast<Window *>(user_ptr);
            wrapped->ctx.framebufferSizeCallback(*wrapped, width, height);
        }
    }

    static void windowCursorPos(GLFWwindow *window, double xpos, double ypos) {
        if constexpr (HasCursorPosCallback<T>::value) {
            void *user_ptr = glfwGetWindowUserPointer(window);
            if (user_ptr == nullptr) {
                return;
            }

            Window *wrapped = static_cast<Window *>(user_ptr);
            wrapped->ctx.cursorPosCallback(*wrapped, xpos, ypos);
        }
    }

    static void windowMouseButton(GLFWwindow *window, int button, int action,
                                  int mods) {
        if constexpr (HasMouseButtonCallback<T>::value) {
            void *user_ptr = glfwGetWindowUserPointer(window);
            if (user_ptr == nullptr) {
                return;
            }

            Window *wrapped = static_cast<Window *>(user_ptr);
            wrapped->ctx.mouseButtonCallback(*wrapped, button, action, mods);
        }
    }

    auto setPin() -> void { glfwSetWindowUserPointer(this->window, this); }

    auto shouldClose() -> bool { return glfwWindowShouldClose(this->window); }

    auto close() -> void { glfwSetWindowShouldClose(this->window, 1); }

    auto setPos(int x, int y) -> void { glfwSetWindowPos(this->window, x, y); }

    auto show() -> void { glfwShowWindow(this->window); }

    auto render() -> void {
        if (this->needs_render) {
            this->needs_render = false;
            this->renderNow();
        }
    }

    auto renderNow() -> void {
        glClear(GL_COLOR_BUFFER_BIT);
        if constexpr (HasRender<T>::value) {
            this->ctx.render(*this);
        }
        glfwSwapBuffers(this->window);
    }

    auto processEvents() -> void {
        glfwPollEvents();

        if constexpr (HasProcessEvents<T>::value) {
            this->ctx.processEvents(*this);
        }
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

    auto getClampedMouseLocation() -> Location {
        double xpos, ypos;
        glfwGetCursorPos(this->window, &xpos, &ypos);

        Dims window_dims = this->getDims();
        double dw = window_dims.width;
        double dh = window_dims.height;

        double clamped_row = clamp(0.0, ypos, dh);
        double clamped_col = clamp(0.0, xpos, dw);

        return Location{static_cast<size_t>(clamped_row),
                        static_cast<size_t>(clamped_col)};
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

        return QuadProgram{std::move(p), n_pos, n_tex_p, tex, Texture2D{}};
    }

    auto quadProgram() -> QuadProgram {
        Op<QuadProgram> op_program = Window::quadProgramOp();

        QuadProgram result{std::move(op_program.get())};
        return result;
    }

    auto bakedFontOp(Arena &arena, char const *font_file, float pixel_height)
        -> Op<BakedFont> {
        auto marker = arena.mark();

        char const *file_contents = getContentsZ(arena, font_file);
        unsigned char const *casted = static_cast<unsigned char const *>(
            static_cast<void const *>(file_contents));

        stbtt_bakedchar chardata[96]; // printable characters
        Dims bmp_dims{512, 512};
        unsigned char *pixels = arena.pushTN<unsigned char>(bmp_dims.area());

        stbtt_BakeFontBitmap(casted, 0, pixel_height, pixels, bmp_dims.width,
                             bmp_dims.height, 32 /* space */,
                             96 /* 127 - 32 + 1 */, chardata);

        Program p{};

        p.attachShader(this->QuadVertShader);
        p.attachShader(this->FontFragShader);
        if (!p.link()) {
            return Op<BakedFont>::empty();
        }

        glUseProgram(p.program);
        GLint n_pos = glGetAttribLocation(p.program, "n_pos");
        GLint n_tex_p = glGetAttribLocation(p.program, "n_tex_p");
        GLint font_color = glGetUniformLocation(p.program, "font_color");
        GLint tex = glGetUniformLocation(p.program, "tex");

        QuadProgram quad_program{std::move(p), n_pos, n_tex_p, tex,
                                 Texture2D{}};
        quad_program.texture.bindAlphaData(bmp_dims, pixels);

        return BakedFont{pixel_height, bmp_dims, chardata,
                         std::move(quad_program), font_color};
    }

    auto bakedFont(Arena &arena, char const *font_file, float pixel_height)
        -> BakedFont {
        Op<BakedFont> op_font =
            Window::bakedFontOp(arena, font_file, pixel_height);

        BakedFont result{std::move(op_font.get())};
        return result;
    }

    auto renderQuad(QuadProgram &p, Location loc, Dims dims) -> void {
        Dims window_dims = this->getDims();
        p.renderAt(loc, dims, window_dims);
    }

    auto renderText(BakedFont &p, Location loc, StrSlice text) -> void {
        Dims window_dims = this->getDims();
        p.renderText(loc, text, window_dims);
    }
};
