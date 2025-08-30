#pragma once

#include "../arena.cc"
#include "../dirutils.cc"
#include "../fileutils.cc"
#include "../strslice.cc"
#include "bakedfont.cc"
#include "gl.cc"
#include "quadprogram.cc"
#include "shader.cc"
#include "stb_truetype.cc"

#include <assert.h>
#include <sys/types.h>
#include <utility>

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

template <typename T> struct HasPaint {
    template <typename U, typename = decltype(std::declval<U>().paint(
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

    bool needs_repaint;
    bool needs_rerender;

    template <typename... Args>
    Window(int width, int height, char const *title, Args &&...args)
        : window(getInitWindow(width, height, title)),
          QuadVertShader{Shader::fromSource(GL_VERTEX_SHADER,
                                            STR_SLICE(QuadVertShaderText))},
          QuadFragShader{Shader::fromSource(GL_FRAGMENT_SHADER,
                                            STR_SLICE(QuadFragShaderText))},
          FontFragShader{Shader::fromSource(GL_FRAGMENT_SHADER,
                                            STR_SLICE(FontFragShaderText))},
          ctx(*this, std::forward<Args>(args)...), needs_repaint(true),
          needs_rerender(true) {
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
        if (this->needs_rerender) {
            this->needs_repaint = false;
            this->needs_rerender = false;
            this->renderNow();
        } else if (this->needs_repaint) {
            this->needs_repaint = false;
            this->paintNow();
        }
    }

    auto renderNow() -> void {
        glClear(GL_COLOR_BUFFER_BIT);
        if constexpr (HasRender<T>::value) {
            this->ctx.render(*this);
        }
        glfwSwapBuffers(this->window);
    }

    auto paintNow() -> void {
        glClear(GL_COLOR_BUFFER_BIT);
        if constexpr (HasPaint<T>::value) {
            this->ctx.paint(*this);
        } else if constexpr (HasRender<T>::value) {
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

    auto getMouseLocation() -> SLocation {
        double xpos, ypos;
        glfwGetCursorPos(this->window, &xpos, &ypos);

        return SLocation{static_cast<ssize_t>(ypos),
                         static_cast<ssize_t>(xpos)};
    }

    auto getClampedMouseLocation() -> SLocation {
        Dims dims = this->getDims();
        SLocation mouse_loc = this->getMouseLocation();

        return SLocation{clamp<ssize_t>(0, mouse_loc.row, dims.height),
                         clamp<ssize_t>(0, mouse_loc.col, dims.width)};
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

    auto renderQuad(QuadProgram &p, SRect rect) -> void {
        Dims window_dims = this->getDims();
        p.renderAt(rect, window_dims);
    }

    auto renderText(BakedFont &p, SLocation loc, StrSlice text) -> void {
        Dims window_dims = this->getDims();
        p.renderText(loc, text, window_dims);
    }

    auto renderText(BakedFont &p, SRect rect, StrSlice text) -> void {
        Dims window_dims = this->getDims();
        p.renderText(rect, text, window_dims);
    }

    auto renderCenteredText(BakedFont &p, SRect rect, StrSlice text) -> void {
        Dims window_dims = this->getDims();
        p.renderCenteredText(rect, text, window_dims);
    }
};
