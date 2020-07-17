#include <cmath>

#include "ncurses.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <limits>

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

struct Barycentric {
  real a;
  real b;
  real c;
};

Barycentric convertToBarycentric(vec3 posA, vec3 posB, vec3 posC, vec3 pos) {
  posA.z = 0;
  posB.z = 0;
  posC.z = 0;
  
  vec3 v0 = posB - posA;
  vec3 v1 = posC - posA;
  vec3 v2 = pos - posA;

  real d00 = dot(v0, v0);
  real d01 = dot(v0, v1);
  real d11 = dot(v1, v1);
  real d20 = dot(v2, v0);
  real d21 = dot(v2, v1);

  real denom = d00 * d11 - d01 * d01;

  Barycentric result;
  result.a = (d11 * d20 - d01 * d21) / denom;
  result.b = (d00 * d21 - d01 * d20) / denom;
  result.c = 1.0 - result.a - result.b;

  return result;
}

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

  mat4 getMatrix() const {
    return m_transform;
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

mat4 perpesctiveMatrix(real near, real far, real fov, real aspect_ratio) {
  
  mat4 projection = mat4(1.0);
  real tan_ratio = std::atan2(radians(fov), 2);
  projection[0] = vec4(tan_ratio / aspect_ratio, 0.0, 0.0, 0.0);
  projection[1] = vec4(0.0, tan_ratio, 0.0, 0.0);
  projection[2] = vec4(0.0, 0.0, (near + far) / (near - far), -1);
  projection[3] = vec4(0.0, 0.0, 2 * near * far / (near - far), 0.0);

  return projection;
}

mat4 viewportMatrix(int screen_width, int screen_height) {

  mat4 viewportMat = mat4(1.0);
  viewportMat[0] = vec4(0.5 * screen_width, 0.0, 0.0, 0.0);
  viewportMat[1] = vec4(0.0, 0.5 * screen_height, 0.0, 0.0);
  viewportMat[2] = vec4(0.0, 0.0, 0.5, 0.0);

  viewportMat = translate(mat4(1.0), vec3(1.0, 1.0, 1.0)) * viewportMat;

  return viewportMat;
}

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

    m_projection = perpesctiveMatrix(0.1, 25.0, 90.0, real(m_terminal_width) / m_terminal_height);
    m_viewport = viewportMatrix(m_terminal_width, m_terminal_height);
    
    bool running = true;
    while(running) {

      int key = getch();
      if(key == 27) {
        running = false;
      }

      draw();
      refresh();
    }
    
    refresh();
    getch();
    endwin();

  }
  void draw() {

    clearBuffers();
    
  }
  
private:

  void startDrawing(Triangle* triangle, char filling_symbol) {

    // ? filling_symbol depends on distance: 0 for closest objects (z < 0.5), 9 for farthest
    
    // Apply vertex transforms:
    //   model-world matrix
    //   world-camera matrix
    //   projection matrix

    // Apply pespective division
    // pseudo-clipping (only need to check is triangle outside NDC)
    // Viewport matrix
    // Rasterize:
    //   use barycentric coordinates
    //   move trough each coordinate and check whether or not it leaves behind current pixel
    //   if it's closest pixel - write it to pixel_buffer and reset depth buffer value

    // Repeat for each triangle

    mat4 m_cameraTransform = m_camera.getMatrix();

    int clippedVertices = 0;
    
    Vertex transformedVertices[3];
    for(int i = 0; i < 3; i++) {
      vec4 vertex_position = vec4(triangle->vertices[i].position, 1.0);
      vertex_position  = m_projection * m_cameraTransform * vertex_position;
      vertex_position /= vertex_position[3];

      if(vertex_position.x > 1.0 || vertex_position.x < -1.0 ||
         vertex_position.y > 1.0 || vertex_position.y < -1.0 ||
         vertex_position.z > 1.0 || vertex_position.z < -1.0) {

        clippedVertices++;
        
      }
      
      transformedVertices[i] = triangle->vertices[i];
      transformedVertices[i].position = vertex_position;
    }

    if(clippedVertices == 3) {
      return;
    }

    for(int i = 0;i < 3; i++) {
      transformedVertices[i].position = vec3(m_viewport * vec4(transformedVertices[i].position, 1.0));
    }

    int minX = m_terminal_width,  maxX = 0;
    int minY = m_terminal_height, maxY = 0;
    
    for(int i = 0; i < 3; i++) {
      minX = std::max<int>(0, transformedVertices[i].position.x);
      maxX = std::max<int>(m_terminal_width, transformedVertices[i].position.x);

      minY = std::max<int>(0, transformedVertices[i].position.y);
      maxY = std::max<int>(m_terminal_height, transformedVertices[i].position.y);
    }

    // Rasterization
    for(int x = minX; x < maxX; x++) {
      for(int y = minY; y < maxY; y++) {
        Barycentric coordinate = convertToBarycentric(transformedVertices[0].position,
			      transformedVertices[1].position,
			      transformedVertices[2].position,
			      vec3(x + 0.5, y + 0.5, 0));

        if(coordinate.a < 0.0 || coordinate.a > 1.0 ||
           coordinate.b < 0.0 || coordinate.b > 1.0 ||
           coordinate.c < 0.0 || coordinate.c > 1.0) {
          continue;
        }

        vec3 v0 = transformedVertices[1].position - transformedVertices[0].position;
        vec3 v1 = transformedVertices[2].position - transformedVertices[0].position;

        vec3 pos = v0 * coordinate.a + v1 * coordinate.b;

        // Depth test
        if(pos.z >= m_buffers.depth_buffer[y * m_terminal_width + x]) {
          continue;
        }

        vec3 cv0 = transformedVertices[1].color - transformedVertices[0].color;
        vec3 cv1 = transformedVertices[2].color - transformedVertices[0].color;

        vec3 color = cv0 * coordinate.a + cv1 * coordinate.b;

        // Normal ...

        m_buffers.pixel_buffer[y * m_terminal_width + x] = color.x; // set char???
        
      }
    }
    
  }

  void clearBuffers() {
    for(int i = 0; i < m_total_size; ++i) {
      m_buffers.depth_buffer[i] = std::numeric_limits<real>::max();
      m_buffers.pixel_buffer[i] = 0;
    }
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
  int m_total_size;

  Buffers m_buffers;
  Camera  m_camera;
  mat4    m_projection;
  mat4    m_viewport;
  
};

int main()
{
  Application application;
  application.run();
  
  return 0;
}
