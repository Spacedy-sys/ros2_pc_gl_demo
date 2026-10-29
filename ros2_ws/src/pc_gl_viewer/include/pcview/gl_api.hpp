// 零依赖的 OpenGL 3.3 Core 函数加载器。
//
// 为什么不直接用 GLEW/GLAD？
//   - Windows 上 opengl32.dll 只导出 GL 1.1，2.0+ 的函数必须走 wglGetProcAddress；
//     引入 GLEW/GLAD 就得额外带一个第三方库（cmake/vcpkg 环境依赖）。
//   - 本 demo 想在「裸机 + MSVC + 系统自带 OpenGL」和「Ubuntu + GLFW」两种环境下用同一份代码，
//     因此自己写一个 ~40 个入口的最小加载器，代价可控，也说明清楚 GPU 侧的调用约定。
//
// 用法：
//   pcview::gl_load(pcview::GetProcAddrFn fn, &err);   // 由平台层提供 wglGetProcAddress / glfwGetProcAddress
// 之后所有 pcview 代码正常调用 glGenBuffers 等（宏把名字重定向到加载到的函数指针）。
#pragma once

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include <cstddef>
#include <string>

#include <GL/gl.h>

// ---- 系统 gl.h 未提供 GL 2.0+ 的类型/常量时自行补齐（不覆盖已有定义） ----
#ifndef GL_VERSION_2_0
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif

#ifndef GL_ARRAY_BUFFER
#  define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#  define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#  define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_VERTEX_SHADER
#  define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#  define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#  define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#  define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#  define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#  define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_TEXTURE0
#  define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE_MIN_FILTER
#  define GL_TEXTURE_MIN_FILTER 0x2801
#endif
#ifndef GL_TEXTURE_MAG_FILTER
#  define GL_TEXTURE_MAG_FILTER 0x2800
#endif
#ifndef GL_TEXTURE_WRAP_S
#  define GL_TEXTURE_WRAP_S 0x2802
#endif
#ifndef GL_TEXTURE_WRAP_T
#  define GL_TEXTURE_WRAP_T 0x2803
#endif
#ifndef GL_CLAMP_TO_EDGE
#  define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR
#  define GL_LINEAR 0x2601
#endif
#ifndef GL_R8
#  define GL_R8 0x8229
#endif
#ifndef GL_RED
#  define GL_RED 0x1903
#endif
#ifndef GL_UNPACK_ALIGNMENT
#  define GL_UNPACK_ALIGNMENT 0x0CF5
#endif
#ifndef GL_LINE_SMOOTH
#  define GL_LINE_SMOOTH 0x0B20
#endif
#ifndef GL_MULTISAMPLE
#  define GL_MULTISAMPLE 0x809D
#endif
#ifndef GL_BLEND
#  define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
#  define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#  define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_DEPTH_TEST
#  define GL_DEPTH_TEST 0x0B71
#endif
#ifndef APIENTRY
#  define APIENTRY
#endif

namespace pcview {

using GetProcAddrFn = void* (*)(const char*);

// 解析全部入口；成功返回 true，失败时 err 里给出第一个缺失的函数名。
bool gl_load(GetProcAddrFn get_proc, std::string* err = nullptr);

// 编译+链接着色器程序；失败时 err 里带上 shader 的编译日志（调试必看）。
GLuint compile_program(const char* vs_src, const char* fs_src, std::string* err = nullptr);

// 便于 HUD 上打印驱动/显卡信息。
std::string gl_vendor();
std::string gl_renderer();
std::string gl_version();

}  // namespace pcview

// ---- 函数指针（pcgl_ 前缀实现，用宏暴露成标准 GL 名字） ----
typedef void (APIENTRY* PCGL_GenBuffers)(GLsizei, GLuint*);
typedef void (APIENTRY* PCGL_BindBuffer)(GLenum, GLuint);
typedef void (APIENTRY* PCGL_BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void (APIENTRY* PCGL_BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void (APIENTRY* PCGL_DeleteBuffers)(GLsizei, const GLuint*);
typedef void (APIENTRY* PCGL_GenVertexArrays)(GLsizei, GLuint*);
typedef void (APIENTRY* PCGL_BindVertexArray)(GLuint);
typedef void (APIENTRY* PCGL_DeleteVertexArrays)(GLsizei, const GLuint*);
typedef void (APIENTRY* PCGL_EnableVertexAttribArray)(GLuint);
typedef void (APIENTRY* PCGL_DisableVertexAttribArray)(GLuint);
typedef void (APIENTRY* PCGL_VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef GLuint (APIENTRY* PCGL_CreateShader)(GLenum);
typedef void (APIENTRY* PCGL_ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void (APIENTRY* PCGL_CompileShader)(GLuint);
typedef void (APIENTRY* PCGL_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PCGL_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY* PCGL_DeleteShader)(GLuint);
typedef GLuint (APIENTRY* PCGL_CreateProgram)(void);
typedef void (APIENTRY* PCGL_AttachShader)(GLuint, GLuint);
typedef void (APIENTRY* PCGL_LinkProgram)(GLuint);
typedef void (APIENTRY* PCGL_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void (APIENTRY* PCGL_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void (APIENTRY* PCGL_DeleteProgram)(GLuint);
typedef void (APIENTRY* PCGL_UseProgram)(GLuint);
typedef GLint (APIENTRY* PCGL_GetUniformLocation)(GLuint, const GLchar*);
typedef void (APIENTRY* PCGL_Uniform1f)(GLint, GLfloat);
typedef void (APIENTRY* PCGL_Uniform1i)(GLint, GLint);
typedef void (APIENTRY* PCGL_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void (APIENTRY* PCGL_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* PCGL_Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY* PCGL_UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void (APIENTRY* PCGL_GenTextures)(GLsizei, GLuint*);
typedef void (APIENTRY* PCGL_BindTexture)(GLenum, GLuint);
typedef void (APIENTRY* PCGL_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void (APIENTRY* PCGL_TexParameteri)(GLenum, GLenum, GLint);
typedef void (APIENTRY* PCGL_DeleteTextures)(GLsizei, const GLuint*);
typedef void (APIENTRY* PCGL_PixelStorei)(GLenum, GLint);
typedef void (APIENTRY* PCGL_ActiveTexture)(GLenum);

extern PCGL_GenBuffers pcgl_GenBuffers;
extern PCGL_BindBuffer pcgl_BindBuffer;
extern PCGL_BufferData pcgl_BufferData;
extern PCGL_BufferSubData pcgl_BufferSubData;
extern PCGL_DeleteBuffers pcgl_DeleteBuffers;
extern PCGL_GenVertexArrays pcgl_GenVertexArrays;
extern PCGL_BindVertexArray pcgl_BindVertexArray;
extern PCGL_DeleteVertexArrays pcgl_DeleteVertexArrays;
extern PCGL_EnableVertexAttribArray pcgl_EnableVertexAttribArray;
extern PCGL_DisableVertexAttribArray pcgl_DisableVertexAttribArray;
extern PCGL_VertexAttribPointer pcgl_VertexAttribPointer;
extern PCGL_CreateShader pcgl_CreateShader;
extern PCGL_ShaderSource pcgl_ShaderSource;
extern PCGL_CompileShader pcgl_CompileShader;
extern PCGL_GetShaderiv pcgl_GetShaderiv;
extern PCGL_GetShaderInfoLog pcgl_GetShaderInfoLog;
extern PCGL_DeleteShader pcgl_DeleteShader;
extern PCGL_CreateProgram pcgl_CreateProgram;
extern PCGL_AttachShader pcgl_AttachShader;
extern PCGL_LinkProgram pcgl_LinkProgram;
extern PCGL_GetProgramiv pcgl_GetProgramiv;
extern PCGL_GetProgramInfoLog pcgl_GetProgramInfoLog;
extern PCGL_DeleteProgram pcgl_DeleteProgram;
extern PCGL_UseProgram pcgl_UseProgram;
extern PCGL_GetUniformLocation pcgl_GetUniformLocation;
extern PCGL_Uniform1f pcgl_Uniform1f;
extern PCGL_Uniform1i pcgl_Uniform1i;
extern PCGL_Uniform2f pcgl_Uniform2f;
extern PCGL_Uniform3f pcgl_Uniform3f;
extern PCGL_Uniform4f pcgl_Uniform4f;
extern PCGL_UniformMatrix4fv pcgl_UniformMatrix4fv;
extern PCGL_GenTextures pcgl_GenTextures;
extern PCGL_BindTexture pcgl_BindTexture;
extern PCGL_TexImage2D pcgl_TexImage2D;
extern PCGL_TexParameteri pcgl_TexParameteri;
extern PCGL_DeleteTextures pcgl_DeleteTextures;
extern PCGL_PixelStorei pcgl_PixelStorei;
extern PCGL_ActiveTexture pcgl_ActiveTexture;

#define glGenBuffers pcgl_GenBuffers
#define glBindBuffer pcgl_BindBuffer
#define glBufferData pcgl_BufferData
#define glBufferSubData pcgl_BufferSubData
#define glDeleteBuffers pcgl_DeleteBuffers
#define glGenVertexArrays pcgl_GenVertexArrays
#define glBindVertexArray pcgl_BindVertexArray
#define glDeleteVertexArrays pcgl_DeleteVertexArrays
#define glEnableVertexAttribArray pcgl_EnableVertexAttribArray
#define glDisableVertexAttribArray pcgl_DisableVertexAttribArray
#define glVertexAttribPointer pcgl_VertexAttribPointer
#define glCreateShader pcgl_CreateShader
#define glShaderSource pcgl_ShaderSource
#define glCompileShader pcgl_CompileShader
#define glGetShaderiv pcgl_GetShaderiv
#define glGetShaderInfoLog pcgl_GetShaderInfoLog
#define glDeleteShader pcgl_DeleteShader
#define glCreateProgram pcgl_CreateProgram
#define glAttachShader pcgl_AttachShader
#define glLinkProgram pcgl_LinkProgram
#define glGetProgramiv pcgl_GetProgramiv
#define glGetProgramInfoLog pcgl_GetProgramInfoLog
#define glDeleteProgram pcgl_DeleteProgram
#define glUseProgram pcgl_UseProgram
#define glGetUniformLocation pcgl_GetUniformLocation
#define glUniform1f pcgl_Uniform1f
#define glUniform1i pcgl_Uniform1i
#define glUniform2f pcgl_Uniform2f
#define glUniform3f pcgl_Uniform3f
#define glUniform4f pcgl_Uniform4f
#define glUniformMatrix4fv pcgl_UniformMatrix4fv
#define glGenTextures pcgl_GenTextures
#define glBindTexture pcgl_BindTexture
#define glTexImage2D pcgl_TexImage2D
#define glTexParameteri pcgl_TexParameteri
#define glDeleteTextures pcgl_DeleteTextures
#define glPixelStorei pcgl_PixelStorei
#define glActiveTexture pcgl_ActiveTexture
