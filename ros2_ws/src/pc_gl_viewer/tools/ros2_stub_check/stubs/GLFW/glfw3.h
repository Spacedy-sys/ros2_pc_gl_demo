#pragma once
// 桩：GLFW/glfw3.h（只声明本 demo 用到的入口与常量）
// 真实环境（Ubuntu）用 sudo apt install libglfw3-dev 提供的真头文件。

#ifdef __cplusplus
extern "C" {
#endif

struct GLFWwindow;
struct GLFWmonitor;

#define GLFW_TRUE 1
#define GLFW_FALSE 0

#define GLFW_PRESS 1
#define GLFW_RELEASE 0
#define GLFW_REPEAT 2

#define GLFW_MOUSE_BUTTON_LEFT 0
#define GLFW_MOUSE_BUTTON_RIGHT 1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

#define GLFW_CONTEXT_VERSION_MAJOR 0x00022002
#define GLFW_CONTEXT_VERSION_MINOR 0x00022003
#define GLFW_OPENGL_PROFILE 0x00022008
#define GLFW_OPENGL_CORE_PROFILE 0x00032001
#define GLFW_OPENGL_FORWARD_COMPAT 0x00022006

#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_1 49
#define GLFW_KEY_2 50
#define GLFW_KEY_3 51
#define GLFW_KEY_C 67
#define GLFW_KEY_G 71
#define GLFW_KEY_H 72
#define GLFW_KEY_P 80
#define GLFW_KEY_R 82
#define GLFW_KEY_MINUS 45
#define GLFW_KEY_EQUAL 61
#define GLFW_KEY_KP_ADD 334
#define GLFW_KEY_KP_SUBTRACT 333

typedef void (*GLFWmousebuttonfun)(GLFWwindow*, int, int, int);
typedef void (*GLFWcursorposfun)(GLFWwindow*, double, double);
typedef void (*GLFWscrollfun)(GLFWwindow*, double, double);
typedef void (*GLFWkeyfun)(GLFWwindow*, int, int, int, int);

int glfwInit(void);
void glfwTerminate(void);
void glfwWindowHint(int hint, int value);
GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor,
                             GLFWwindow* share);
void glfwDestroyWindow(GLFWwindow* window);
void glfwMakeContextCurrent(GLFWwindow* window);
void glfwSwapInterval(int interval);
void* glfwGetProcAddress(const char* procname);
void glfwSetWindowUserPointer(GLFWwindow* window, void* pointer);
void* glfwGetWindowUserPointer(GLFWwindow* window);
void glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun callback);
void glfwSetCursorPosCallback(GLFWwindow* window, GLFWcursorposfun callback);
void glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun callback);
void glfwSetKeyCallback(GLFWwindow* window, GLFWkeyfun callback);
void glfwPollEvents(void);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwSwapBuffers(GLFWwindow* window);
void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
void glfwGetCursorPos(GLFWwindow* window, double* xpos, double* ypos);

#ifdef __cplusplus
}
#endif
