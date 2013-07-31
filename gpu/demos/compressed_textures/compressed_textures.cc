// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <math.h>
#include <stdio.h>
#include <string>

// Some tests for compressed textures.
#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"

namespace gpu {
namespace demos {

namespace {

int g_frameCount = 0;
int g_textureIndex = 0;
int g_numTextures = 1;
GLuint g_textures[5];
int g_textureLoc = -1;
GLuint g_programObject = 0;
GLuint g_vbo = 0;

void CheckGLError(const char* func_name, int line_no) {
#ifndef NDEBUG
  GLenum error = GL_NO_ERROR;
  while ((error = glGetError()) != GL_NO_ERROR) {
    fprintf(stderr, "GL ERROR in %s at line %d : 0x%04x\n",
            func_name, line_no, error);
  }
#endif
}

GLuint LoadShader(GLenum type, const char* shaderSrc) {
  CheckGLError("LoadShader", __LINE__);
  GLuint shader = glCreateShader(type);
  if (shader == 0) {
    return 0;
  }
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);
  // Compile the shader
  glCompileShader(shader);
  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    fprintf(stderr, "Error compiling shader: %s\n", log.c_str());
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void InitShaders() {
  static const char* vShaderStr =
    "attribute vec2 g_Position;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(g_Position.x, g_Position.y, 0.0, 1.0);\n"
    "   texCoord = g_Position * vec2(0.5) + vec2(0.5);\n"
    "}\n";
  static const char* fShaderStr =
    "precision mediump float;"
    "uniform sampler2D tex;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = texture2D(tex, texCoord);\n"
    "}\n";

  CheckGLError("InitShaders", __LINE__);
  GLuint vertexShader = LoadShader(GL_VERTEX_SHADER, vShaderStr);
  GLuint fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fShaderStr);
  // Create the program object
  GLuint programObject = glCreateProgram();
  if (programObject == 0) {
    fprintf(stderr, "Creating program failed\n");
    return;
  }
  glAttachShader(programObject, vertexShader);
  glAttachShader(programObject, fragmentShader);
  // Bind g_Position to attribute 0
  glBindAttribLocation(programObject, 0, "g_Position");
  // Link the program
  glLinkProgram(programObject);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(programObject, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    fprintf(stderr, "Error linking program: %s\n", log.c_str());
    glDeleteProgram(programObject);
    return;
  }
  g_programObject = programObject;
  g_textureLoc = glGetUniformLocation(g_programObject, "tex");
  glGenBuffers(1, &g_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  static float vertices[] = {
      1.0f,  1.0f,
     -1.0f,  1.0f,
     -1.0f, -1.0f,
      1.0f,  1.0f,
     -1.0f, -1.0f,
      1.0f, -1.0f
  };
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vertices),
               vertices,
               GL_STATIC_DRAW);
  CheckGLError("InitShaders", __LINE__);
}

GLuint CreateCheckerboardTexture() {
  CheckGLError("CreateCheckerboardTexture", __LINE__);
  static unsigned char pixels[] = {
    255, 255, 255,
    0,   0,   0,
    0,   0,   0,
    255, 255, 255,
  };
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixels);
  CheckGLError("CreateCheckerboardTexture", __LINE__);
  return texture;
}

GLuint CreateCompressedTexture(
    const void* data,
    GLsizeiptr size,
    GLenum format,
    GLint width,
    GLint height) {
  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glCompressedTexImage2D(
      GL_TEXTURE_2D, 0, format, width, height, 0, size, data);
  CheckGLError("CreateCompressedTxture", __LINE__);
  return texture;
}

GLuint LoadDXT1RGBTexture() {
  static unsigned char data[] = {
    0x00, 0xd0, 0x00, 0xef, 0x00, 0xaa, 0x95, 0xd5,
    0x00, 0x90, 0x57, 0xff, 0x00, 0xaa, 0xff, 0x55,
    0x00, 0x88, 0x99, 0xff, 0x00, 0xaa, 0xff, 0x55,
    0x20, 0xb8, 0xe4, 0xf6, 0x00, 0xaa, 0x5b, 0x5d,
    0x21, 0x09, 0xe6, 0x27, 0x15, 0x15, 0x15, 0x15,
    0xd7, 0xbd, 0xff, 0xff, 0x56, 0x16, 0x16, 0x56,
    0x00, 0x00, 0xff, 0xff, 0x55, 0x00, 0x00, 0x55,
    0xe0, 0x08, 0xa9, 0x2f, 0x51, 0x58, 0x56, 0x54,
    0xff, 0x07, 0x15, 0x09, 0x40, 0x6a, 0xd5, 0xd5,
    0xd7, 0xbd, 0xff, 0xff, 0x56, 0x16, 0x16, 0x16,
    0x91, 0x08, 0xff, 0xff, 0x55, 0xff, 0x00, 0x03,
    0xfe, 0x07, 0x5b, 0x09, 0x01, 0xa9, 0x55, 0xff,
    0x0d, 0x10, 0x18, 0xe8, 0xea, 0x15, 0x95, 0x55,
    0x08, 0x88, 0x3c, 0xe7, 0x55, 0x55, 0xff, 0x00,
    0x10, 0x20, 0x18, 0xe8, 0xa8, 0x54, 0x56, 0x55,
    0x1f, 0x78, 0x15, 0xf8, 0x00, 0xaa, 0x55, 0x55,
  };
  return CreateCompressedTexture(
      data, sizeof(data), GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 16, 16);
}

GLuint LoadDXT1RGBATexture() {
  static unsigned char data[] = {
    0xff, 0xff, 0x90, 0xee, 0x00, 0xaa, 0xff, 0x55,
    0x53, 0xde, 0xdd, 0xff, 0x55, 0x55, 0x00, 0xff,
    0xc9, 0xb4, 0xdd, 0xff, 0x55, 0x55, 0xaa, 0xff,
    0x6f, 0xee, 0xdd, 0xff, 0x55, 0x55, 0xa8, 0x03,
    0x60, 0xa3, 0xa5, 0xe5, 0x55, 0x55, 0xaa, 0x00,
    0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff,
    0x80, 0xab, 0xc2, 0xc4, 0xff, 0x55, 0xaa, 0xff,
    0x60, 0x9b, 0xa5, 0xe5, 0x57, 0x56, 0xaa, 0x00,
    0x1f, 0xbf, 0xff, 0xf7, 0x55, 0x55, 0xaa, 0x00,
    0x00, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xfe, 0xbe, 0x7e, 0xdf, 0xff, 0xaa, 0x55, 0x00,
    0xfe, 0xbe, 0xff, 0xf7, 0x54, 0x56, 0xaa, 0x00,
    0xfa, 0x4c, 0x7e, 0x9e, 0x55, 0xaa, 0x00, 0x00,
    0xbb, 0x2c, 0x98, 0x54, 0xff, 0xff, 0x55, 0xaa,
    0xfa, 0x4c, 0x7e, 0x9e, 0x55, 0xaa, 0x00, 0x00,
    0xfa, 0x4c, 0x7e, 0x9e, 0x55, 0xaa, 0x00, 0x00,
  };
  return CreateCompressedTexture(
      data, sizeof(data), GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 16, 16);
}

GLuint LoadDXT3RGBATexture() {
  static unsigned char data[] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xa3, 0x22, 0x03, 0x03, 0x55, 0xff, 0xaa, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0xcf, 0x7c, 0xc3, 0x12, 0x55, 0x55, 0x55, 0x54,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x83, 0x1a, 0x03, 0x03, 0x55, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff,
    0xcf, 0x7c, 0xc3, 0x12, 0x55, 0x55, 0x55, 0x54,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc3, 0x59, 0x63, 0x32, 0x55, 0xff, 0xaa, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x00,
    0x8f, 0x94, 0x03, 0x4a, 0x54, 0x94, 0x94, 0x54,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0xa3, 0x59, 0x43, 0x2a, 0x55, 0xff, 0xaa, 0x00,
    0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
    0xaf, 0x84, 0x03, 0x42, 0x54, 0x55, 0x55, 0x55,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc3, 0xa0, 0x83, 0x71, 0x55, 0xff, 0xaa, 0x00,
    0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x0f, 0xb4, 0x43, 0x81, 0x54, 0x94, 0x94, 0x94,
    0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc3, 0xa0, 0x83, 0x69, 0x55, 0xff, 0xaa, 0x00,
    0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc3, 0xa0, 0x83, 0x69, 0x55, 0xfd, 0xaa, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x23, 0xd0, 0xa3, 0xb0, 0x55, 0xff, 0xaa, 0x00,
    0x00, 0x40, 0x00, 0x40, 0xff, 0xff, 0xff, 0xff,
    0xf0, 0xc3, 0x43, 0xc0, 0x94, 0x94, 0x55, 0x55,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x23, 0xd0, 0xa3, 0xb0, 0x55, 0xff, 0xaa, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x23, 0xd0, 0xa3, 0xb0, 0x55, 0xff, 0xaa, 0x00,
  };
  return CreateCompressedTexture(
      data, sizeof(data), GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 16, 16);
}

GLuint LoadDXT5RGBATexture() {
  static unsigned char data[] = {
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x72, 0xdd, 0x03, 0x92, 0x2d, 0x2d, 0x2d, 0x2d,
    0x04, 0xfb, 0xff, 0xff, 0xff, 0x49, 0x02, 0xdb,
    0x97, 0xf6, 0x32, 0xcd, 0xc0, 0xc0, 0x7b, 0xc0,
    0xfb, 0xff, 0x49, 0x92, 0x24, 0x00, 0x60, 0xdb,
    0x11, 0xcd, 0x67, 0x82, 0x78, 0x78, 0x5e, 0x78,
    0x04, 0xfb, 0xff, 0xff, 0xff, 0xf9, 0x8f, 0xff,
    0x2e, 0xac, 0xc4, 0x71, 0x15, 0x15, 0x15, 0x14,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x72, 0xdd, 0x03, 0x92, 0x2d, 0x2d, 0x2d, 0x2d,
    0x04, 0x43, 0xb0, 0x0d, 0x3b, 0xb0, 0x03, 0xdb,
    0xb8, 0xf6, 0xd5, 0xd5, 0x60, 0x60, 0x60, 0x60,
    0xfb, 0x00, 0x49, 0x02, 0x00, 0x00, 0x90, 0x24,
    0xb0, 0xbc, 0x26, 0x7a, 0x78, 0x78, 0x78, 0x78,
    0x04, 0xf7, 0xf8, 0x9f, 0xff, 0xf9, 0x9f, 0xff,
    0x0e, 0xac, 0x83, 0x69, 0x34, 0x35, 0x35, 0x35,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x72, 0xdd, 0x03, 0x92, 0x2d, 0x2d, 0x2d, 0x2d,
    0x04, 0x43, 0xb0, 0x0d, 0x3b, 0xb0, 0x03, 0x3b,
    0xb8, 0xf6, 0xd5, 0xd5, 0x60, 0x60, 0x60, 0x60,
    0xfb, 0xff, 0xb6, 0x0d, 0x00, 0x49, 0x92, 0x24,
    0x11, 0xcd, 0x67, 0x82, 0x78, 0x5e, 0x78, 0x78,
    0xea, 0xff, 0x4a, 0xd2, 0x24, 0x49, 0x92, 0x24,
    0x0e, 0xac, 0xc4, 0x71, 0x15, 0x15, 0x15, 0x15,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x72, 0xdd, 0x03, 0x92, 0x2d, 0x2d, 0x2d, 0x2d,
    0xfd, 0x00, 0x49, 0x9c, 0xc4, 0x00, 0x00, 0x00,
    0xb8, 0xf6, 0x53, 0xcd, 0xe0, 0xe0, 0x7f, 0xe0,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xf1, 0xcc, 0x46, 0x82, 0x78, 0x78, 0x78, 0x78,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0e, 0xac, 0xc4, 0x71, 0x15, 0x15, 0x15, 0x15,
  };
  return CreateCompressedTexture(
      data, sizeof(data), GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16, 16);
}

}  // anonymous namespace.

static int stInit(ESContext *esContext) {
  CheckGLError("GLFromCPPInit", __LINE__);
  g_textures[0] = CreateCheckerboardTexture();
  glClearColor(0.f, 0.f, .7f, 1.f);
  const char* extensions =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  if (strstr(extensions, "GL_EXT_texture_compression_dxt1")) {
    g_textures[g_numTextures++] = LoadDXT1RGBTexture();
    g_textures[g_numTextures++] = LoadDXT1RGBATexture();
  }
  if (strstr(extensions, "GL_CHROMIUM_texture_compression_dxt3")) {
    g_textures[g_numTextures++] = LoadDXT3RGBATexture();
  }
  if (strstr(extensions, "GL_CHROMIUM_texture_compression_dxt5")) {
    g_textures[g_numTextures++] = LoadDXT5RGBATexture();
  }
  InitShaders();
  CheckGLError("GLFromCPPInit", __LINE__);
  return 1;
}

static void stDraw (ESContext *esContext) {
  CheckGLError("GLFromCPPDraw", __LINE__);

  g_frameCount++;
  if (g_frameCount > 60)
  {
    g_frameCount = 0;
    g_textureIndex = (g_textureIndex + 1) % g_numTextures;
  }

  glViewport(0, 0, esContext->width, esContext->height);
  // Clear the color buffer
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Use the program object
  glUseProgram(g_programObject);
  CheckGLError("GLFromCPPDraw", __LINE__);

  // Load the vertex data
  glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Bind the texture to texture unit 0
  glBindTexture(GL_TEXTURE_2D, g_textures[g_textureIndex]);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Point the uniform sampler to texture unit 0
  glUniform1i(g_textureLoc, 0);
  CheckGLError("GLFromCPPDraw", __LINE__);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError("GLFromCPPDraw", __LINE__);
  glFlush();
}

static void stShutDown (ESContext *esContext) {
}

struct STUserData {
  int dummy;
};

class CompressedTextureTest : public gles2_book::Example<STUserData> {
 public:
  CompressedTextureTest() {
    RegisterCallbacks(stInit, NULL, stDraw, stShutDown);
  }

  const wchar_t* Title() const {
    return L"Compressed Texture Test";
  }

  bool IsAnimated() {
    return true;
  }
};

Demo* CreateDemo() {
  return new CompressedTextureTest();
}

}  // namespace demos
}  // namespace gpu


