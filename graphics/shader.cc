#pragma once

#include "../arena.cc"
#include "../op.cc"
#include "../strslice.cc"
#include "gl.cc"

#include <assert.h>
#include <stdio.h>

struct Shader {
    GLuint shader;

    auto source(StrSlice source_slice) -> void {
        GLint source_len = source_slice.len;
        glShaderSource(this->shader, 1, &source_slice.ptr, &source_len);
    }

    auto compile(Arena *arena) -> bool {
        glCompileShader(this->shader);

        GLint status = GL_FALSE;
        glGetShaderiv(this->shader, GL_COMPILE_STATUS, &status);

        if (status != GL_TRUE) {
            GLint len = 0;
            glGetShaderiv(this->shader, GL_INFO_LOG_LENGTH, &len);

            auto marker = arena->mark();
            char *log = arena->pushTN<char>(len);
            glGetShaderInfoLog(this->shader, len, nullptr, log);
            fprintf(stderr, "Shader Log: %.*s\n", len, log);

            return false;
        }
        return true;
    }
};

struct Program {
    GLuint program;

    auto attachShader(Shader shader) -> void {
        glAttachShader(this->program, shader.shader);
    }

    auto link(Arena *arena) -> bool {
        glLinkProgram(this->program);

        GLint status = GL_FALSE;
        glGetProgramiv(this->program, GL_LINK_STATUS, &status);

        if (status != GL_TRUE) {
            GLint len = 0;
            glGetProgramiv(this->program, GL_INFO_LOG_LENGTH, &len);

            auto marker = arena->mark();
            char *log = arena->pushTN<char>(len);
            glGetProgramInfoLog(this->program, len, nullptr, log);
            fprintf(stderr, "Program Log: %.*s\n", len, log);

            return false;
        }
        return true;
    }

    auto useProgram() -> void { glUseProgram(this->program); }
};

auto makeShader(Arena *arena, GLenum type, Slice<StrSlice> sources) -> Shader {
    Shader s{glCreateShader(type)};

    for (StrSlice &source : sources) {
        s.source(source);
    }
    assert(s.compile(arena));

    return s;
}

auto makeShader(Arena *arena, GLenum type, StrSlice source) -> Shader {
    return makeShader(arena, type, Slice<StrSlice>{&source, 1});
}

auto deleteShader(Shader *shader) -> void {
    glDeleteShader(shader->shader);
    shader->shader = 0;
}

auto makeProgram(Arena *arena, Slice<Shader> shaders) -> Program {
    Program p{glCreateProgram()};

    for (Shader &shader : shaders) {
        p.attachShader(shader);
    }
    assert(p.link(arena));

    return p;
}

auto deleteProgram(Program *program) -> void {
    glDeleteProgram(program->program);
    program->program = 0;
}
