#include "pcview/gl_api.hpp"

// 注意：这些函数指针在头文件里声明于【全局作用域】（这样 #define glGenBuffers pcgl_GenBuffers
// 在任何命名空间里都能解析到同一个符号），因此定义也必须放在全局作用域。
// 若定义放进 namespace pcview，链接时就会出现
//   "无法解析的外部符号 pcgl_BindBuffer" + "已定义且可能匹配的符号 pcview::pcgl_BindBuffer"
// 这种「看起来有定义却链不上」的怪现象。
PCGL_GenBuffers pcgl_GenBuffers = nullptr;
PCGL_BindBuffer pcgl_BindBuffer = nullptr;
PCGL_BufferData pcgl_BufferData = nullptr;
PCGL_BufferSubData pcgl_BufferSubData = nullptr;
PCGL_DeleteBuffers pcgl_DeleteBuffers = nullptr;
PCGL_GenVertexArrays pcgl_GenVertexArrays = nullptr;
PCGL_BindVertexArray pcgl_BindVertexArray = nullptr;
PCGL_DeleteVertexArrays pcgl_DeleteVertexArrays = nullptr;
PCGL_EnableVertexAttribArray pcgl_EnableVertexAttribArray = nullptr;
PCGL_DisableVertexAttribArray pcgl_DisableVertexAttribArray = nullptr;
PCGL_VertexAttribPointer pcgl_VertexAttribPointer = nullptr;
PCGL_CreateShader pcgl_CreateShader = nullptr;
PCGL_ShaderSource pcgl_ShaderSource = nullptr;
PCGL_CompileShader pcgl_CompileShader = nullptr;
PCGL_GetShaderiv pcgl_GetShaderiv = nullptr;
PCGL_GetShaderInfoLog pcgl_GetShaderInfoLog = nullptr;
PCGL_DeleteShader pcgl_DeleteShader = nullptr;
PCGL_CreateProgram pcgl_CreateProgram = nullptr;
PCGL_AttachShader pcgl_AttachShader = nullptr;
PCGL_LinkProgram pcgl_LinkProgram = nullptr;
PCGL_GetProgramiv pcgl_GetProgramiv = nullptr;
PCGL_GetProgramInfoLog pcgl_GetProgramInfoLog = nullptr;
PCGL_DeleteProgram pcgl_DeleteProgram = nullptr;
PCGL_UseProgram pcgl_UseProgram = nullptr;
PCGL_GetUniformLocation pcgl_GetUniformLocation = nullptr;
PCGL_Uniform1f pcgl_Uniform1f = nullptr;
PCGL_Uniform1i pcgl_Uniform1i = nullptr;
PCGL_Uniform2f pcgl_Uniform2f = nullptr;
PCGL_Uniform3f pcgl_Uniform3f = nullptr;
PCGL_Uniform4f pcgl_Uniform4f = nullptr;
PCGL_UniformMatrix4fv pcgl_UniformMatrix4fv = nullptr;
PCGL_GenTextures pcgl_GenTextures = nullptr;
PCGL_BindTexture pcgl_BindTexture = nullptr;
PCGL_TexImage2D pcgl_TexImage2D = nullptr;
PCGL_TexParameteri pcgl_TexParameteri = nullptr;
PCGL_DeleteTextures pcgl_DeleteTextures = nullptr;
PCGL_PixelStorei pcgl_PixelStorei = nullptr;
PCGL_ActiveTexture pcgl_ActiveTexture = nullptr;

namespace pcview {

namespace {

GetProcAddrFn g_get_proc = nullptr;

void* resolve(const char* name) {
  void* p = g_get_proc ? g_get_proc(name) : nullptr;
#if defined(_WIN32)
  // 部分驱动对 1.x 函数返回空，这里再兜底查一次 opengl32.dll。
  if (!p) {
    static HMODULE gl = ::LoadLibraryA("opengl32.dll");
    if (gl) p = reinterpret_cast<void*>(::GetProcAddress(gl, name));
  }
#endif
  return p;
}

template <typename T>
bool load_one(T& fn, const char* name, std::string* err) {
  void* p = resolve(name);
  if (!p) {
    if (err) *err = std::string("缺少 GL 入口: ") + name;
    return false;
  }
  fn = reinterpret_cast<T>(p);
  return true;
}

}  // namespace

bool gl_load(GetProcAddrFn get_proc, std::string* err) {
  g_get_proc = get_proc;
#define PCGL_LOAD(fn)                                   \
  do {                                                  \
    if (!load_one(pcgl_##fn, "gl" #fn, err)) return false; \
  } while (0)

  PCGL_LOAD(GenBuffers);
  PCGL_LOAD(BindBuffer);
  PCGL_LOAD(BufferData);
  PCGL_LOAD(BufferSubData);
  PCGL_LOAD(DeleteBuffers);
  PCGL_LOAD(GenVertexArrays);
  PCGL_LOAD(BindVertexArray);
  PCGL_LOAD(DeleteVertexArrays);
  PCGL_LOAD(EnableVertexAttribArray);
  PCGL_LOAD(DisableVertexAttribArray);
  PCGL_LOAD(VertexAttribPointer);
  PCGL_LOAD(CreateShader);
  PCGL_LOAD(ShaderSource);
  PCGL_LOAD(CompileShader);
  PCGL_LOAD(GetShaderiv);
  PCGL_LOAD(GetShaderInfoLog);
  PCGL_LOAD(DeleteShader);
  PCGL_LOAD(CreateProgram);
  PCGL_LOAD(AttachShader);
  PCGL_LOAD(LinkProgram);
  PCGL_LOAD(GetProgramiv);
  PCGL_LOAD(GetProgramInfoLog);
  PCGL_LOAD(DeleteProgram);
  PCGL_LOAD(UseProgram);
  PCGL_LOAD(GetUniformLocation);
  PCGL_LOAD(Uniform1f);
  PCGL_LOAD(Uniform1i);
  PCGL_LOAD(Uniform2f);
  PCGL_LOAD(Uniform3f);
  PCGL_LOAD(Uniform4f);
  PCGL_LOAD(UniformMatrix4fv);
  PCGL_LOAD(GenTextures);
  PCGL_LOAD(BindTexture);
  PCGL_LOAD(TexImage2D);
  PCGL_LOAD(TexParameteri);
  PCGL_LOAD(DeleteTextures);
  PCGL_LOAD(PixelStorei);
  PCGL_LOAD(ActiveTexture);
#undef PCGL_LOAD
  return true;
}

namespace {
std::string gl_string(GLenum e) {
  const GLubyte* s = glGetString(e);
  return s ? std::string(reinterpret_cast<const char*>(s)) : std::string("unknown");
}

GLuint compile_shader(GLenum type, const char* src, std::string* err) {
  const GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint status = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
  if (!status) {
    GLint len = 0;
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<std::size_t>(len > 1 ? len : 1), '\0');
    glGetShaderInfoLog(sh, len, nullptr, &log[0]);
    if (err) *err = std::string(type == GL_VERTEX_SHADER ? "顶点着色器编译失败: " : "片元着色器编译失败: ") + log;
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}
}  // namespace

GLuint compile_program(const char* vs_src, const char* fs_src, std::string* err) {
  const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, err);
  if (!vs) return 0;
  const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, err);
  if (!fs) {
    glDeleteShader(vs);
    return 0;
  }
  const GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint status = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (!status) {
    GLint len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<std::size_t>(len > 1 ? len : 1), '\0');
    glGetProgramInfoLog(prog, len, nullptr, &log[0]);
    if (err) *err = "着色器链接失败: " + log;
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}


std::string gl_vendor() { return gl_string(GL_VENDOR); }
std::string gl_renderer() { return gl_string(GL_RENDERER); }
std::string gl_version() { return gl_string(GL_VERSION); }

}  // namespace pcview
