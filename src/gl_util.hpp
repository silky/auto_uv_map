#pragma once

/*
  GLFW
*/
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>

#include <string>
#include <chrono>
#include <ctime>

inline void CheckOpenGLError(const char* stmt, const char* fname, int line)
{
    GLenum err = glGetError();
    //  const GLubyte* sError = gluErrorString(err);

    if (err != GL_NO_ERROR){
	printf("OpenGL error %08x, at %s:%i - for %s.\n", err, fname, line, stmt);
	exit(1);
    }
}

#define _DEBUG

// GL Check Macro.
#ifdef _DEBUG
#define GL_C(stmt) do {					\
	stmt;						\
	CheckOpenGLError(#stmt, __FILE__, __LINE__);	\
    } while (0)
#else
#define GL_C(stmt) stmt
#endif

inline char* GetShaderLogInfo(GLuint shader) {

    GLint len;
    GLsizei actualLen;

    GL_C(glGetShaderiv(shader,  GL_INFO_LOG_LENGTH, &len));
    char* infoLog = new char[len];

    GL_C(glGetShaderInfoLog(shader, len, &actualLen, infoLog));

    return  infoLog;

}

inline GLuint CreateShaderFromString(const std::string& shaderSource, const GLenum shaderType) {

    std::string src = shaderSource;

    GLuint shader;

    GL_C(shader = glCreateShader(shaderType));
    const char *c_str = src.c_str();
    GL_C(glShaderSource(shader, 1, &c_str, NULL ));
    GL_C(glCompileShader(shader));

    GLint compileStatus;
    GL_C(glGetShaderiv(shader,  GL_COMPILE_STATUS, &compileStatus));

    if (compileStatus != GL_TRUE) {
	printf("Could not compile shader\n\n%s \n\n%s\n",  src.c_str(),
	       GetShaderLogInfo(shader) );
	exit(1);
    }

    return shader;
}

/*
  Load shader with only vertex and fragment shader.
*/
inline GLuint LoadNormalShader(const std::string& vsSource, const std::string& fsShader){


    // Create the shaders
    GLuint vs = CreateShaderFromString(vsSource, GL_VERTEX_SHADER);
    GLuint fs =CreateShaderFromString(fsShader, GL_FRAGMENT_SHADER);

    // Link the program
    GLuint shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);



    GLint Result;
    glGetProgramiv(shader, GL_LINK_STATUS, &Result);
    if(Result == GL_FALSE) {
	printf("Could not link shader \n\n%s\n",   GetShaderLogInfo(shader)  );
	exit(1);
    }

    glDetachShader(shader, vs);
    glDetachShader(shader, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return shader;
}
