#ifndef PSC_MODEL_INSPECTOR_H
#define PSC_MODEL_INSPECTOR_H

#include "stl_parser.h"
#include "topology_evaluator.h"
#include "convex_decomposition_simple.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>

typedef struct {
    GLuint vao;           // Vertex Array Object
    GLuint vbo;           // Vertex Buffer Object
    GLuint shader;        // Shader program for solid mesh
    GLuint edge_shader;   // Shader program for edges
    GLuint axis_shader;   // Shader program for axis lines
    GLuint axis_vao;      // VAO for axis lines
    GLuint axis_vbo;      // VBO for axis lines
    GLuint num_vertices;  // Number of vertices to render
    GLuint num_triangles; // Number of triangles
    float* vertex_data;   // Vertex data array
    float* triangle_colors; // Color for each triangle (RGB)
    float* edge_colors;    // Color for each edge (RGB)
    int* triangle_checked; // Flag for each triangle if it's been checked
    int* edge_checked;    // Flag for each edge if it's been checked
    float camera_pos[3];    // Camera position in world space
    float camera_target[3];  // Point camera is looking at
    float world_up[3];       // World up vector (typically Y axis)
    float rotation_speed; // Camera rotation sensitivity
    int window_width;
    int window_height;
    GLFWwindow* window;   // Store window handle for callbacks
} stl_viewer_t;

// Initialize the viewer
stl_viewer_t* viewer_init(int width, int height);

// Load STL data into the viewer
int viewer_load_stl(stl_viewer_t* viewer, const stl_file_t* stl);

// Display the STL model
void viewer_display(stl_viewer_t* viewer);

// Clean up viewer resources
void viewer_cleanup(stl_viewer_t* viewer);

// Update triangle color
void viewer_set_triangle_color(stl_viewer_t* viewer, unsigned int triangle_index, float r, float g, float b);

// Update edge color
void viewer_set_edge_color(stl_viewer_t* viewer, unsigned int edge_index, float r, float g, float b);

// Mark triangle as checked
void viewer_mark_triangle_checked(stl_viewer_t* viewer, unsigned int triangle_index);

// Mark edge as checked
void viewer_mark_edge_checked(stl_viewer_t* viewer, unsigned int edge_index);

// Display STL model with topology visualization
void display_topology_visualization(const stl_file_t* stl);

// Edge callback for topology visualization
void topology_edge_callback(unsigned int edge_index, void* user_data);

// Display convex decomposition results with random colors for each part
void display_convex_decomposition_results(const stl_file_t* stl, float concavity_threshold, int max_parts);

// Generate random color for a given index
void generate_random_color(int index, float* r, float* g, float* b);



#endif // PSC_MODEL_INSPECTOR_H