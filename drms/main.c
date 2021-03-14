
#include "../glad/glad.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 600

// How many samples MSAA will use. This will break things
#define MULTISAMPLE_SAMPLES 4

static struct {
  bool exit;

  int ms_mode; // 0 = no MS, 1 = 4S, 2 = Half resolution MS w/ DR
  int lowres; // 0 = full res, 1 = half res, 2 = quarter res

  // Window
  GLFWwindow *window;
  int window_size[2];
  int window_size_low[2];

  // Framebuffer(s)
  GLuint fbo_scene;
  GLuint fbo_scene_color;

  // Mesh(es)
  GLuint vao_triangle;
  GLuint vbo_triangle;

  GLuint vao_quad;
  GLuint vbo_quad;

  // Shader program(s)
  GLuint program_mesh;
  GLuint program_resolve;
} state = {
  .window_size = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, },
};

// ========================================
//
// Framebuffer(s)

void deinitFramebuffers(void) {
  if(!state.fbo_scene) {
    return;
  }

  glDeleteFramebuffers(1, &state.fbo_scene);
  state.fbo_scene = 0;

  glDeleteTextures(1, &state.fbo_scene_color);
  state.fbo_scene_color = 0;
}

void reinitFramebuffers(void) {
  deinitFramebuffers();

  printf("Framebuffer size: %dx%d\n", state.window_size_low[0], state.window_size_low[1]);

  glGenFramebuffers(1, &state.fbo_scene);

  glGenTextures(1, &state.fbo_scene_color);

  if(state.ms_mode) {
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, state.fbo_scene_color);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MULTISAMPLE_SAMPLES, GL_RGBA8,
                            state.window_size_low[0], state.window_size_low[1], GL_TRUE);
  } else {
    glBindTexture(GL_TEXTURE_2D, state.fbo_scene_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, state.window_size_low[0], state.window_size_low[1], 0, GL_RGBA, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, state.fbo_scene);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.fbo_scene_color, 0);

  printf("Multisample mode: %d\n", state.ms_mode);
  if(state.ms_mode) {
    printf("Sample positions:\n");
    for(int i = 0; i < MULTISAMPLE_SAMPLES; i++) {
      GLfloat pos[2] = { -1.0, -1.0, };
      glGetMultisamplefv(GL_SAMPLE_POSITION, i, pos);
      printf("  %d: %f, %f\n", i, pos[0], pos[1]);
    }
  }

  /*
    On my system the 4 samples are:
    Bottom left: ivec2(0, 0)
    Bottom right: ivec2(1, 0)
    Top left: ivec2(0, 1)
    Top right: ivec2(1, 1)
  */
}

// ========================================
//
// Mesh(es)

struct vertex {
  float position[2];
  uint8_t color[3];
};

void initMeshes(void) {
  // Triangle
  static const struct vertex triangle_vertices[3] = {
    {
      .position = { -0.5, -0.7, },
      .color = { 255, 0, 0, },
    },
    {
      .position = { 0.4, -0.3, },
      .color = { 0, 0, 255, },
    },
    {
      .position = { 0.0, 0.5, },
      .color = { 0, 255, 0, },
    },
  };

  glGenVertexArrays(1, &state.vao_triangle);
  glBindVertexArray(state.vao_triangle);

  glGenBuffers(1, &state.vbo_triangle);
  glBindBuffer(GL_ARRAY_BUFFER, state.vbo_triangle);
  glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices), triangle_vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void *) offsetof(struct vertex, position));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct vertex), (void *) offsetof(struct vertex, color));

  // Quad
  static const struct vertex quad_vertices[6] = {
    {
      .position = { -1.0, -1.0, },
    },
    {
      .position = { 1.0, -1.0, },
    },
    {
      .position = { -1.0, 1.0, },
    },
    {
      .position = { 1.0, 1.0, },
    },
    {
      .position = { -1.0, 1.0, },
    },
    {
      .position = { 1.0, -1.0, },
    },
  };

  glGenVertexArrays(1, &state.vao_quad);
  glBindVertexArray(state.vao_quad);

  glGenBuffers(1, &state.vbo_quad);
  glBindBuffer(GL_ARRAY_BUFFER, state.vbo_quad);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), offsetof(struct vertex, position));
}

void deinitMeshes(void) {
  glDeleteBuffers(1, &state.vbo_triangle);
  glDeleteVertexArrays(1, &state.vao_triangle);

  glDeleteBuffers(1, &state.vbo_quad);
  glDeleteVertexArrays(1, &state.vao_quad);
}

// ========================================
//
// Shader(s)

static const char *shader_source_mesh_vert = "#version 330\n"
  "layout (location = 0) in vec2 attrib_position;\n"
  "layout (location = 1) in vec3 attrib_color;\n"
  "out vec3 vert_color;\n"
  "void main() {\n"
  "  vert_color = attrib_color;\n"
  "  gl_Position = vec4(attrib_position, 0.0, 1.0);\n"
  "}\n";

static const char *shader_source_mesh_frag = "#version 330\n"
  "in vec3 vert_color;\n"
  "layout (location = 0) out vec4 frag_out_color;\n"
  "float rand(vec2 co) {\n"
  "  return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
  "}\n"
  "void main() {\n"
  "  frag_out_color = vec4(vert_color * rand(gl_FragCoord.xy), 1.0);\n"
  "}\n";

static const char *shader_source_resolve_vert = "#version 330\n"
  "layout (location = 0) in vec2 attrib_position;\n"
  "out vec2 vert_uv;\n"
  "void main() {\n"
  "  vert_uv = attrib_position * 0.5 + 0.5;\n"
  "  gl_Position = vec4(attrib_position, 0.0, 1.0);\n"
  "}\n";

static const char *shader_source_resolve_frag = "#version 330\n"
  "uniform sampler2D texture_scene_color;\n"
  "uniform sampler2DMS texture_scene_color_ms;\n"
  "uniform int ms_mode;\n"
  "uniform int lowres;\n"
  "in vec2 vert_uv;\n"
  "layout (location = 0) out vec4 frag_out_color;\n"
  "void main() {\n"
  "  ivec2 texel_coord = ivec2(gl_FragCoord.xy) / (1<<lowres);\n"
  "  if(ms_mode == 1) {\n"
  "    frag_out_color.rgb = texelFetch(texture_scene_color_ms, texel_coord, 0).rgb;\n"
  "    frag_out_color.rgb += texelFetch(texture_scene_color_ms, texel_coord, 1).rgb;\n"
  "    frag_out_color.rgb += texelFetch(texture_scene_color_ms, texel_coord, 2).rgb;\n"
  "    frag_out_color.rgb += texelFetch(texture_scene_color_ms, texel_coord, 3).rgb;\n"
  "    frag_out_color.rgb /= 4.0;\n"
  "  } else if(ms_mode == 2) {\n"
  "    int sample_index = texel_coord.x % 2;\n"
  "    sample_index += (texel_coord.y % 2) * 2;\n"
  "    texel_coord /= 2;\n"
  "    frag_out_color.rgb = texelFetch(texture_scene_color_ms, texel_coord, sample_index).rgb;\n"
  "  } else {\n"
  "    frag_out_color.rgb = texelFetch(texture_scene_color, texel_coord, 0).rgb;\n"
  "  }\n"
  "  if(gl_FragCoord.x < 20.0 && (texel_coord.y % 2) == 0) {\n"
  "    frag_out_color.rgb = vec3(0.0, 0.0, 1.0);\n"
  "  }\n"
  "  if(gl_FragCoord.y < 20.0 && (texel_coord.x % 2) == 0) {\n"
  "    frag_out_color.rgb = vec3(1.0, 0.0, 0.0);\n"
  "  }\n"
  "  frag_out_color.a = 1.0;\n"
  "}\n";

void initShaders(void) {
  // Mesh
  {
    GLuint shader_vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader_vert, 1, (const GLchar **) &shader_source_mesh_vert, NULL);
    glCompileShader(shader_vert);

    GLuint shader_frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_frag, 1, (const GLchar **) &shader_source_mesh_frag, NULL);
    glCompileShader(shader_frag);

    state.program_mesh = glCreateProgram();

    glAttachShader(state.program_mesh, shader_vert);
    glAttachShader(state.program_mesh, shader_frag);

    glLinkProgram(state.program_mesh);

    glDeleteShader(shader_vert);
    glDeleteShader(shader_frag);
  }

  // Resolve
  {
    GLuint shader_vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader_vert, 1, (const GLchar **) &shader_source_resolve_vert, NULL);
    glCompileShader(shader_vert);

    GLuint shader_frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shader_frag, 1, (const GLchar **) &shader_source_resolve_frag, NULL);
    glCompileShader(shader_frag);

    state.program_resolve = glCreateProgram();

    glAttachShader(state.program_resolve, shader_vert);
    glAttachShader(state.program_resolve, shader_frag);

    glLinkProgram(state.program_resolve);

    glDeleteShader(shader_vert);
    glDeleteShader(shader_frag);
  }
}

void deinitShaders(void) {
  glDeleteProgram(state.program_mesh);
  glDeleteProgram(state.program_resolve);
}

// ========================================
//
// GLFW callbacks

void callbackResize(GLFWwindow *window, int width, int height) {
  state.window_size[0] = width;
  state.window_size[1] = height;

  if(state.lowres == 1) {
    state.window_size_low[0] = state.window_size[0] / 2;
    state.window_size_low[1] = state.window_size[1] / 2;
  } else if(state.lowres == 2) {
    state.window_size_low[0] = state.window_size[0] / 4;
    state.window_size_low[1] = state.window_size[1] / 4;
  } else {
    state.window_size_low[0] = state.window_size[0];
    state.window_size_low[1] = state.window_size[1];
  }

  if(state.ms_mode == 2) {
    state.window_size_low[0] /= 2;
    state.window_size_low[1] /= 2;
  }

  reinitFramebuffers();

  char title[256];
  snprintf(title, sizeof(title), "GLPrototypes - DRMS (mode = %d, lowres = %d)", state.ms_mode, state.lowres);
  glfwSetWindowTitle(state.window, title);
}

void callbackKey(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if(key == GLFW_KEY_M && action == GLFW_PRESS) {
    state.ms_mode++;
    if(state.ms_mode > 2) {
      state.ms_mode = 0;
    }

    callbackResize(state.window, state.window_size[0], state.window_size[1]);
  }

  if(key == GLFW_KEY_D && action == GLFW_PRESS) {
    if(state.ms_mode == 0) {
      state.ms_mode = 2;
    } else {
      state.ms_mode = 0;
    }

    callbackResize(state.window, state.window_size[0], state.window_size[1]);
  }

  if(key == GLFW_KEY_R && action == GLFW_PRESS) {
    state.lowres++;
    if(state.lowres > 2) {
      state.lowres = 0;
    }

    callbackResize(state.window, state.window_size[0], state.window_size[1]);
  }
}

int main(int argc, char **argv) {
  printf("Keys:\n");
  printf("  M cycles through multisample modes\n");
  printf("    0: No multisample\n");
  printf("    1: 4x multisample\n");
  printf("    2: 4x multisample with dual resolution\n");
  printf("  D toggles between no MS and DRMS\n");
  printf("  R cycles between full, half, and quarter resolution\n");

  // ====================
  //
  // Init

  if(!glfwInit()) {
    printf("GLFW init failed\n");
    return 1;
  }

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

  state.window = glfwCreateWindow(state.window_size[0], state.window_size[1], "GLPrototypes - DRMS (mode = 0)", NULL, NULL);
  if(!state.window) {
    printf("Window creation failed\n");
    return 1;
  }

  glfwSwapInterval(1);

  glfwMakeContextCurrent(state.window);

  glfwSetWindowSizeCallback(state.window, callbackResize);
  glfwSetKeyCallback(state.window, callbackKey);

  if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
    printf("GLAD init failed\n");
    return 1;
  }

  initMeshes();
  initShaders();

  callbackResize(state.window, state.window_size[0], state.window_size[1]);

  // ====================
  //
  // Mainloop

  while(!glfwWindowShouldClose(state.window) && !state.exit) {
    glfwPollEvents();

    // Geometry
    glViewport(0, 0, state.window_size_low[0], state.window_size_low[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, state.fbo_scene);
    static const GLenum drawbuffers[] = { GL_COLOR_ATTACHMENT0, };
    glDrawBuffers(1, drawbuffers);
    static const GLfloat clear_color[] = { 1.0, 1.0, 1.0, 1.0, };
    glClearBufferfv(GL_COLOR, 0, clear_color);
    glUseProgram(state.program_mesh);
    glBindVertexArray(state.vao_triangle);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Resolve
    glViewport(0, 0, state.window_size[0], state.window_size[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(state.program_resolve);
    glUniform1i(glGetUniformLocation(state.program_resolve, "ms_mode"), state.ms_mode);
    glUniform1i(glGetUniformLocation(state.program_resolve, "lowres"), state.lowres);

    if(state.ms_mode) {
      glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, state.fbo_scene_color);
      glUniform1i(glGetUniformLocation(state.program_resolve, "texture_scene_color"), 1);
      glUniform1i(glGetUniformLocation(state.program_resolve, "texture_scene_color_ms"), 0);
    } else {
      glBindTexture(GL_TEXTURE_2D, state.fbo_scene_color);
      glUniform1i(glGetUniformLocation(state.program_resolve, "texture_scene_color"), 0);
      glUniform1i(glGetUniformLocation(state.program_resolve, "texture_scene_color_ms"), 1);
    }

    glBindVertexArray(state.vao_quad);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glfwSwapBuffers(state.window);
  }

  // ====================
  //
  // Deinit

  deinitFramebuffers();
  deinitMeshes();
  deinitShaders();

  return 0;
}
