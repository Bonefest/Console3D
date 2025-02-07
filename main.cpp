#include <cmath>
#include <cctype>
#include <vector>

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
            m_row(-90.0f), m_pitch(0.0f) {
    recalculateTransform();
  }
  
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

  void setData(const vec3& position, real row, real pitch) {
    m_position = position;
    m_row = row;
    m_pitch = pitch;

    recalculateTransform();
  }

  vec3 getPosition() const {
    return m_position;
  }

  real getRow() const {
    return m_row;
  }

  real getPitch() const {
    return m_pitch;
  }
  
  vec3 convertToCameraCoord(vec3 vector) {
    return vec3(m_transform * vec4(vector, 1.0));
  }

  mat4 getMatrix() const {
    return m_transform;
  }

  vec3 getFront() const {
    return m_front;
  }
  
private:

  void recalculateTransform() {
    real row = radians(m_row);
    real pitch = radians(m_pitch);
    
    m_front.x = std::cos(pitch) * std::cos(row);
    m_front.y = std::sin(pitch);
    m_front.z = std::cos(pitch) * std::sin(row);

    m_front = normalize(m_front);
    
    vec3 side = cross(m_up, m_front);
    vec3 up = cross(m_front, side);

    m_transform = mat4(1.0);
    
    m_transform[0] = vec4(side, 0.0);
    m_transform[1] = vec4(up, 0.0);
    m_transform[2] = vec4(m_front, 0.0);

    m_transform = translate(mat4(1.0), m_position) * m_transform;
    
    m_transform = inverse(m_transform);
    
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
  real tan_ratio = std::tan(radians(fov/2));

  projection[0] = vec4(1.0 / (tan_ratio * aspect_ratio), 0.0, 0.0, 0.0);
  projection[1] = vec4(0.0, 1.0 / tan_ratio, 0.0, 0.0);
  projection[2] = vec4(0.0, 0.0, (near + far) / (near - far), -1);
  projection[3] = vec4(0.0, 0.0, 2 * near * far / (near - far), 0.0);

  return projection;
}

mat4 viewportMatrix(int screen_width, int screen_height) {

  mat4 viewportMat = mat4(1.0);
  viewportMat[0] = vec4(0.5 * screen_width, 0.0, 0.0, 0.0);
  viewportMat[1] = vec4(0.0, 0.5 * screen_height, 0.0, 0.0);
  viewportMat[2] = vec4(0.0, 0.0, 0.5, 0.0);

  viewportMat = viewportMat * translate(mat4(1.0), vec3(1.0, 1.0, 1.0));

  return viewportMat;
}

class Application {
public:

  void init() {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    halfdelay(1);
    
    start_color();

    initColors();

    getmaxyx(stdscr, m_terminal_height, m_terminal_width);
    m_total_size = m_terminal_width * m_terminal_height;

    initBuffers();
  }

  void run() {

    init();

    m_projection = perpesctiveMatrix(0.1, 25.0, 90.0, real(m_terminal_width) / m_terminal_height);
    m_viewport = viewportMatrix(m_terminal_width, m_terminal_height);

    m_projection = perspective<float>(45.0, float(m_terminal_width) / real(m_terminal_height) * 0.5, 0.1f, 25.0f);
    
    initCube();
    initSphere(radians(60.0), radians(90.0));

    m_player_speed = 0.2;
    m_player_rot_speed = 5.0;

    m_cube_angle = 0.0f;
    
    m_running = true;
    while(m_running) {

      m_cube_angle += 5.0;
      m_model = translate(mat4(1.0), vec3(0.0, 0.0, 5.0)) * rotate(mat4(1.0), radians(m_cube_angle), vec3(0.0, 1.0, 0.0)) * rotate(mat4(1.0), radians(m_cube_angle), vec3(1.0, 0.0, 0.0));
      
      handleInput();
      draw();
      
    }

    freeBuffers();
    endwin();

  }

  void handleInput() {
    int key = getch();

    vec3 camera_front = m_camera.getFront();
    vec3 camera_side = cross(camera_front, vec3(0.0, 1.0, 0.0));
    vec3 camera_position = m_camera.getPosition();

    real row = m_camera.getRow();
    real pitch = m_camera.getPitch();
    
    //ESC
    if(key == 27) {
      m_running = false;
    }

    key = tolower(key);
    
    if(key == 'w') {
      camera_position -= camera_front * m_player_speed;
    }

    else if(key == 's') {
      camera_position += camera_front * m_player_speed;
    }

    else if(key == 'a') {
      camera_position += camera_side * m_player_speed;
    }

    else if(key == 'd') {
      camera_position -= camera_side * m_player_speed;
    }

    else if(key == 'q') {
      row += m_player_rot_speed;
    }

    else if(key == 'e') {
      row -= m_player_rot_speed;
    }

    m_camera.setData(camera_position, row, pitch);
  }
  
  void draw() {

    clear();
    clearBuffers();
    for(int i = 0;i < 12; i++) {
      startDrawing(&m_cube[i], ' ');
    }
    //    for(int i = 0; i < m_sphere.size(); i ++) {
    //   startDrawing(&m_sphere[i], ' ');
    // }
    
    for(int y = 0; y < m_terminal_height; ++y) {
      for(int x = 0; x < m_terminal_width; ++x) {
        int pair = m_buffers.pixel_buffer[y * m_terminal_width + x];
        if(m_buffers.depth_buffer[y * m_terminal_width + x] < 100.0) {
          char symbol = 'a' + rand() % ('z' - 'a');
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.9) {
          //   symbol = '9';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.7) {
          //   symbol = '8';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.5) {
          //   symbol = '7';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.3) {
          //   symbol = '6';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.1) {
          //   symbol = '5';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.05) {
          //   symbol = '4';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 1.0) {
          //   symbol = '3';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 0.95) {
          //   symbol = '2';
          // }
          // if(m_buffers.depth_buffer[y * m_terminal_width + x] < 0.9) {
          //   symbol = '1';
          // }
          
          attron(COLOR_PAIR(pair));
          mvaddch(y, x, symbol);
          attroff(COLOR_PAIR(pair));
        }
      }

    }

    refresh();

  }
  
private:

  void initCube() {

    m_model = mat4(1.0);
    
    // Front face
    m_cube[0].vertices[0].position = vec3(-0.5, -0.5, -0.5);
    m_cube[0].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[0].vertices[1].position = vec3(-0.5, 0.5, -0.5);
    m_cube[0].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[0].vertices[2].position = vec3(0.5, 0.5, -0.5);
    m_cube[0].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[1].vertices[0].position = vec3(0.5, 0.5, -0.5);
    m_cube[1].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[1].vertices[1].position = vec3(0.5, -0.5, -0.5);
    m_cube[1].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[1].vertices[2].position = vec3(-0.5, -0.5, -0.5);
    m_cube[1].vertices[2].color = vec3(0.5, 0.5, 0.5);

    // Back face
    m_cube[2].vertices[0].position = vec3(-0.5, -0.5, 0.5);
    m_cube[2].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[2].vertices[1].position = vec3(-0.5, 0.5, 0.5);
    m_cube[2].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[2].vertices[2].position = vec3(0.5, 0.5, 0.5);
    m_cube[2].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[3].vertices[0].position = vec3(0.5, 0.5, 0.5);
    m_cube[3].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[3].vertices[1].position = vec3(0.5, -0.5, 0.5);
    m_cube[3].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[3].vertices[2].position = vec3(-0.5, -0.5, 0.5);
    m_cube[3].vertices[2].color = vec3(0.5, 0.5, 0.5);
    
    // Left face
    m_cube[4].vertices[0].position = vec3(-0.5, -0.5, -0.5);
    m_cube[4].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[4].vertices[1].position = vec3(-0.5,  0.5, -0.5);
    m_cube[4].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[4].vertices[2].position = vec3(-0.5, 0.5, 0.5);
    m_cube[4].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[5].vertices[0].position = vec3(-0.5, 0.5, 0.5);
    m_cube[5].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[5].vertices[1].position = vec3(-0.5,-0.5, 0.5);
    m_cube[5].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[5].vertices[2].position = vec3(-0.5, -0.5, -0.5);
    m_cube[5].vertices[2].color = vec3(0.5, 0.5, 0.5);    

    // Right face
    m_cube[6].vertices[0].position = vec3(0.5, -0.5, -0.5);
    m_cube[6].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[6].vertices[1].position = vec3(0.5,  0.5, -0.5);
    m_cube[6].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[6].vertices[2].position = vec3(0.5, 0.5, 0.5);
    m_cube[6].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[7].vertices[0].position = vec3(0.5, 0.5, 0.5);
    m_cube[7].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[7].vertices[1].position = vec3(0.5,-0.5, 0.5);
    m_cube[7].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[7].vertices[2].position = vec3(0.5, -0.5, -0.5);
    m_cube[7].vertices[2].color = vec3(0.5, 0.5, 0.5);    

    // Top face
    m_cube[8].vertices[0].position = vec3(-0.5, 0.5, -0.5);
    m_cube[8].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[8].vertices[1].position = vec3(-0.5, 0.5, 0.5);
    m_cube[8].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[8].vertices[2].position = vec3( 0.5, 0.5, 0.5);
    m_cube[8].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[9].vertices[0].position = vec3(0.5, 0.5, 0.5);
    m_cube[9].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[9].vertices[1].position = vec3(0.5, 0.5, -0.5);
    m_cube[9].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[9].vertices[2].position = vec3(-0.5, 0.5, -0.5);
    m_cube[9].vertices[2].color = vec3(0.5, 0.5, 0.5);

    // Bottom face
    m_cube[10].vertices[0].position = vec3(-0.5, -0.5, -0.5);
    m_cube[10].vertices[0].color = vec3(0.5, 0.5, 0.5);
    m_cube[10].vertices[1].position = vec3(-0.5, -0.5, 0.5);
    m_cube[10].vertices[1].color = vec3(1.0, 1.0, 1.0);
    m_cube[10].vertices[2].position = vec3( 0.5, -0.5, 0.5);
    m_cube[10].vertices[2].color = vec3(1.0, 1.0, 1.0);

    m_cube[11].vertices[0].position = vec3(0.5, -0.5, 0.5);
    m_cube[11].vertices[0].color = vec3(1.0, 1.0, 1.0);
    m_cube[11].vertices[1].position = vec3(0.5, -0.5, -0.5);
    m_cube[11].vertices[1].color = vec3(0.5, 0.5, 0.5);
    m_cube[11].vertices[2].position = vec3(-0.5, -0.5, -0.5);
    m_cube[11].vertices[2].color = vec3(0.5, 0.5, 0.5);
    
    
  }

  void initSphere(float theta_step, float fi_step) {
    float fi = -radians(90.0f);

    auto getVector = [](float f, float t) {
	       vec3 result;
	       result.x = std::cos(f) * std::cos(t);
	       result.y = std::sin(f);
	       result.z = std::cos(f) * std::sin(t);

	       return result;
	     };
    
    while(fi <= radians(90.0)) {

      for(float theta = 0.0f; theta <= radians(360.0f); theta += theta_step) {
        Triangle triangle;
        triangle.vertices[0].position = getVector(fi, theta);
        triangle.vertices[1].position = getVector(fi + fi_step, theta);
        triangle.vertices[2].position = getVector(fi + fi_step, theta + theta_step);

        float color = 0.5f + (fi + radians(90.0f)) / radians(180.0f) * 0.5;
        
        triangle.vertices[0].color = vec3(color, color, color);
        triangle.vertices[1].color = vec3(color, color, color);
        triangle.vertices[2].color = vec3(color, color, color);

        m_sphere.push_back(triangle);

        triangle.vertices[0].position = getVector(fi + fi_step, theta + theta_step);
        triangle.vertices[1].position = getVector(fi, theta + theta_step);
        triangle.vertices[2].position = getVector(fi, theta);
        
        m_sphere.push_back(triangle);
      }
      
      fi += fi_step;
    }
    
  }
  
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
      vertex_position  = m_projection * m_cameraTransform * m_model * vertex_position;
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
      minX = std::min<int>(minX, transformedVertices[i].position.x - 1.5);
      maxX = std::max<int>(maxX, transformedVertices[i].position.x + 1.5);

      minY = std::min<int>(minY, transformedVertices[i].position.y - 1.5);
      maxY = std::max<int>(maxY, transformedVertices[i].position.y + 1.5);
    }

    minX = std::max(minX, 0); minY = std::max(minY, 0);
    maxX = std::min(maxX, m_terminal_width); maxY = std::min(maxY, m_terminal_height);
    
    // Rasterization
    for(int x = minX; x < maxX; x++) {
      for(int y = minY; y < maxY; y++) {

        real e1 = ((x + 0.5) - transformedVertices[0].position.x) * (transformedVertices[1].position.y - transformedVertices[0].position.y) - ((y + 0.5) - transformedVertices[0].position.y) * (transformedVertices[1].position.x - transformedVertices[0].position.x);

        real e2 = ((x + 0.5) - transformedVertices[1].position.x) * (transformedVertices[2].position.y - transformedVertices[1].position.y) - ((y + 0.5) - transformedVertices[1].position.y) * (transformedVertices[2].position.x - transformedVertices[1].position.x);

        real e3 = ((x + 0.5) - transformedVertices[2].position.x) * (transformedVertices[0].position.y - transformedVertices[2].position.y) - ((y + 0.5) - transformedVertices[2].position.y) * (transformedVertices[0].position.x - transformedVertices[2].position.x);

        if(!(e1 < 0.0 && e2 < 0.0 && e3 < 0.0) && !(e1 > 0.0 && e2 > 0.0 && e3 > 0.0)) {
          continue;
        }
        
        
        Barycentric coordinate = convertToBarycentric(transformedVertices[0].position,
			      transformedVertices[1].position,
			      transformedVertices[2].position,
			      vec3(x + 0.5, y + 0.5, 0));

        // if(coordinate.a <= -0.5 || coordinate.b <= -0.5 || (coordinate.a + coordinate.b) >= 1.5) {
        //   continue;
        // }

        vec3 v0 = transformedVertices[1].position - transformedVertices[0].position;
        vec3 v1 = transformedVertices[2].position - transformedVertices[0].position;
        
        vec3 pos = v0 * coordinate.a + v1 * coordinate.b + transformedVertices[0].position;
        
        // Depth test
        if(pos.z >= m_buffers.depth_buffer[y * m_terminal_width + x]) {
          continue;
        }

        vec3 cv0 = transformedVertices[1].color - transformedVertices[0].color;
        vec3 cv1 = transformedVertices[2].color - transformedVertices[0].color;

        vec3 color = cv0 * coordinate.a + cv1 * coordinate.c + transformedVertices[0].color;

        // TODO: Normal

        m_buffers.pixel_buffer[y * m_terminal_width + x] = 254 * color.r; // set char???
        m_buffers.depth_buffer[y * m_terminal_width + x] = pos.z;
        
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
    for(int i = 0;i < 255; ++i) {
      init_color(i, i, i, i);
      init_pair(i, i, COLOR_WHITE);
    }
  }
  
  void initBuffers() {
    m_buffers.depth_buffer = new real[m_total_size];
    m_buffers.pixel_buffer = new short[m_total_size];

  }
  
  void freeBuffers() {
    delete [] m_buffers.depth_buffer;
    delete [] m_buffers.pixel_buffer;
  }

  bool m_running;
  
  int m_terminal_width;
  int m_terminal_height;
  int m_total_size;

  Buffers m_buffers;
  Camera  m_camera;
  mat4    m_projection;
  mat4    m_viewport;
  mat4    m_model;

  real    m_player_speed;
  real    m_player_rot_speed;

  real    m_cube_angle;
  
  Triangle m_cube[12];
  std::vector<Triangle> m_sphere;
};

int main()
{
  Application application;
  application.run();
  
  return 0;
}
