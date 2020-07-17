#include <cmath>

#include "ncurses.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

using namespace glm;

#define static global

using real = float;

struct Buffers {
  real* depth_buffer;
  short* pixel_buffer;
};

struct Vertex {
  vec3 position;
  vec3 color;
  vec3 normal;
};

struct Triangle {
  Vertex vertices[3];
};

class Camera {
public:

  Camera(): m_position(0.0f, 0.0f, 0.0f),
            m_up(0.0f, 1.0f, 0.0f),
            m_front(0.0f, 0.0f, -1.0f),
            m_row(0.0f), m_pitch(0.0f) { }
  
  void setRotation(real row, real pitch) {
    m_row = row;
    m_pitch = pitch;
    recalculateTransform();
  }
  
  void setRow(real value) {
    m_row = value;
    recalculateTransform();
  }
  
  void setPitch(real value) {
    m_pitch = value;
    recalculateTransform();
  }
  
  void setPosition(const vec3& position) {
    m_position = position;
    recalculateTransform();
  }
  
  vec3 convertToCameraCoord(vec3 vector) {
    return vec3(m_transform * vec4(vector, 1.0));
  }
  
private:

  void recalculateTransform() {
    m_front.x = std::cos(m_pitch) * std::cos(m_row);
    m_front.y = std::sin(m_pitch);
    m_front.z = std::cos(m_pitch) * std::sin(m_row);

    vec3 side = cross(m_up, m_front);
    vec3 up = cross(m_front, side);

    m_transform = glm::mat4(0.0);
    m_transform[0] = vec4(m_front, 0.0);
    m_transform[1] = vec4(side, 0.0);
    m_transform[2] = vec4(up, 0.0);

    mat4 translate_matrix = translate(mat4(1.0), m_position);
    m_transform = inverse(translate_matrix * m_transform);
    
  }
  
  vec3 m_position;
  vec3 m_up;
  vec3 m_front;

  real m_row;
  real m_pitch;
  
  mat4 m_transform;
};

class Application {
public:

  void init() {
    initscr();
    raw();
    noecho();
    start_color();

    initColors();

    getmaxyx(stdscr, m_terminal_height, m_terminal_width);

    
  }

  void run() {

    init();

    bool running = true;
    
    while(running) {

      int key = getch();
      if(key == 27) {
        running = false;
      }
      
      refresh();
    }
    
    refresh();
    getch();
    endwin();

  }
  void draw() { }
  
private:

  void startDrawing(Triangle* triangle, char filling_symbol) {
    // Apply vertex transforms:
    //   model-world matrix
    //   world-camera matrix
    //   projection matrix

    // Apply pespective division
    // Rasterize:
    //   use barycentric coordinates
    //   move trough each coordinate and check whether or not it leaves behind current pixel
    //   if it's closest pixel - write it to pixel_buffer and reset depth buffer value

    // Repeat for each triangle
  }
  
  void initColors() {
    for(int i = 0;i < 1000; ++i) {
      init_color(i + 10, i, i, i);
    }
  }
  
  void initBuffers(Buffers* buffers) {
    int total_size = m_terminal_width * m_terminal_height;
    m_buffers.depth_buffer = new real[total_size];
    m_buffers.pixel_buffer = new short[total_size];

  }
  
  void freeBuffers(Buffers* buffers) {
    delete [] buffers->depth_buffer;
    delete [] buffers->pixel_buffer;
  }
  
  int m_terminal_width;
  int m_terminal_height;

  Buffers m_buffers;
  Camera  m_camera;
  
};

int main()
{
  Application application;
  application.run();
  
  return 0;
}
