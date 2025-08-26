#pragma once

#include "../op.cc"
#include "../strslice.cc"
#include "gl.cc"

#include <stdio.h>
#include <utility>

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
