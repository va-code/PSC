#include "PSC_model_inspector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define _POSIX_C_SOURCE 200809L  // For usleep
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global variables for mouse tracking
static double last_x = 0.0;
static double last_y = 0.0;
static bool first_mouse = true;

// Mouse movement callback
static void cursor_position_callback(GLFWwindow* window, double x_pos, double y_pos) {
    stl_viewer_t* viewer = (stl_viewer_t*)glfwGetWindowUserPointer(window);
    if (!viewer) return;
    
    // Only rotate when left mouse button is pressed
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        // Reset first_mouse when button is released so we get fresh positions on next click
        first_mouse = true;
        return;
    }

    // Update last position on first mouse press or after release
    if (first_mouse) {
        last_x = x_pos;
        last_y = y_pos;
        first_mouse = false;
        return;
    }
    
    float x_offset = (float)(x_pos - last_x) * viewer->rotation_speed;
    float y_offset = (float)(last_y - y_pos) * viewer->rotation_speed;
    
    last_x = x_pos;
    last_y = y_pos;
    
    // Calculate relative camera position from target
    float rel_pos[3] = {
        viewer->camera_pos[0] - viewer->camera_target[0],
        viewer->camera_pos[1] - viewer->camera_target[1],
        viewer->camera_pos[2] - viewer->camera_target[2]
    };
    
    // Calculate current distance from target
    float dist = sqrtf(rel_pos[0] * rel_pos[0] + 
                      rel_pos[1] * rel_pos[1] + 
                      rel_pos[2] * rel_pos[2]);
    
    // Convert current position to spherical coordinates
    float theta = atan2f(rel_pos[2], rel_pos[0]);
    float phi = acosf(rel_pos[1] / dist);
    
    // Update angles (removed negatives to flip rotation direction)
    theta += x_offset;  // Rotate around Y axis
    phi += y_offset;    // Rotate up/down
    
    // Clamp phi to avoid flipping
    if (phi < 0.1f) phi = 0.1f;
    if (phi > M_PI - 0.1f) phi = M_PI - 0.1f;
    
    // Convert back to Cartesian coordinates (relative to target)
    rel_pos[0] = dist * sinf(phi) * cosf(theta);
    rel_pos[1] = dist * cosf(phi);
    rel_pos[2] = dist * sinf(phi) * sinf(theta);
    
    // Update camera position relative to target
    viewer->camera_pos[0] = viewer->camera_target[0] + rel_pos[0];
    viewer->camera_pos[1] = viewer->camera_target[1] + rel_pos[1];
    viewer->camera_pos[2] = viewer->camera_target[2] + rel_pos[2];
    
    // Maintain world up vector
    viewer->world_up[0] = 0.0f;
    viewer->world_up[1] = -1.0f;
    viewer->world_up[2] = 0.0f;
}

// Vertex shader source
const char* vertex_shader_source = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec3 normal;\n"
    "layout (location = 2) in vec3 color;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec3 FragNormal;\n"
    "out vec3 FragColor;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
    "    FragNormal = mat3(transpose(inverse(model))) * normal;\n"
    "    FragColor = color;\n"
    "}\n";

// Fragment shader source for solid mesh
const char* fragment_shader_source = 
    "#version 330 core\n"
    "in vec3 FragNormal;\n"
    "in vec3 FragColor;\n"
    "out vec4 OutColor;\n"
    "uniform vec3 lightDir;\n"
    "void main() {\n"
    "    vec3 norm = normalize(FragNormal);\n"
    "    float diff = max(dot(norm, lightDir), 0.0);\n"
    "    vec3 color = FragColor * (0.3 + 0.7 * diff);\n"
    "    OutColor = vec4(color, 1.0);\n"
    "}\n";

// Fragment shader source for edges
const char* edge_fragment_shader_source = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"  // Black color for edges
    "}\n";

// Vertex shader source for axis lines
const char* axis_vertex_shader_source = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 position;\n"
    "layout (location = 1) in vec3 color;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec3 FragColor;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * vec4(position, 1.0);\n"
    "    FragColor = color;\n"
    "}\n";

// Fragment shader source for axis lines
const char* axis_fragment_shader_source = 
    "#version 330 core\n"
    "in vec3 FragColor;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    outColor = vec4(FragColor, 1.0);\n"
    "}\n";

// Helper function to compile shaders
static GLuint compile_shader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        fprintf(stderr, "Shader compilation failed: %s\n", info_log);
        return 0;
    }
    return shader;
}

// Initialize the viewer
stl_viewer_t* viewer_init(int width, int height) {
    stl_viewer_t* viewer = malloc(sizeof(stl_viewer_t));
    if (!viewer) return NULL;
    
    // Initialize GLFW
    if (!glfwInit()) {
        free(viewer);
        return NULL;
    }
    
    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    viewer->window = glfwCreateWindow(width, height, "STL Viewer", NULL, NULL);
    if (!viewer->window) {
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    glfwMakeContextCurrent(viewer->window);
    glfwSetWindowTitle(viewer->window, "STL Viewer");
    
    // Camera initialization is handled in the camera setup section
    
    // Set window pointer
    glfwSetWindowUserPointer(viewer->window, viewer);
    
    // Set cursor position callback
    glfwSetCursorPosCallback(viewer->window, cursor_position_callback);
    
    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Create and compile shaders for solid mesh
    GLuint vertex_shader = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
    GLuint fragment_shader = compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!vertex_shader || !fragment_shader) {
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Create shader program for solid mesh
    viewer->shader = glCreateProgram();
    glAttachShader(viewer->shader, vertex_shader);
    glAttachShader(viewer->shader, fragment_shader);
    glLinkProgram(viewer->shader);
    
    GLint success;
    glGetProgramiv(viewer->shader, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(viewer->shader, 512, NULL, info_log);
        fprintf(stderr, "Shader program linking failed: %s\n", info_log);
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Clean up solid mesh shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    // Create and compile shaders for edges
    vertex_shader = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
    GLuint edge_fragment_shader = compile_shader(edge_fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!vertex_shader || !edge_fragment_shader) {
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Create shader program for edges
    viewer->edge_shader = glCreateProgram();
    glAttachShader(viewer->edge_shader, vertex_shader);
    glAttachShader(viewer->edge_shader, edge_fragment_shader);
    glLinkProgram(viewer->edge_shader);
    
    glGetProgramiv(viewer->edge_shader, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(viewer->edge_shader, 512, NULL, info_log);
        fprintf(stderr, "Edge shader program linking failed: %s\n", info_log);
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Clean up edge shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(edge_fragment_shader);

    // Create and compile shaders for axis lines
    vertex_shader = compile_shader(axis_vertex_shader_source, GL_VERTEX_SHADER);
    GLuint axis_fragment_shader = compile_shader(axis_fragment_shader_source, GL_FRAGMENT_SHADER);
    if (!vertex_shader || !axis_fragment_shader) {
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Create shader program for axis lines
    viewer->axis_shader = glCreateProgram();
    glAttachShader(viewer->axis_shader, vertex_shader);
    glAttachShader(viewer->axis_shader, axis_fragment_shader);
    glLinkProgram(viewer->axis_shader);
    
    glGetProgramiv(viewer->axis_shader, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(viewer->axis_shader, 512, NULL, info_log);
        fprintf(stderr, "Axis shader program linking failed: %s\n", info_log);
        glfwDestroyWindow(viewer->window);
        glfwTerminate();
        free(viewer);
        return NULL;
    }
    
    // Clean up axis shaders
    glDeleteShader(vertex_shader);
    glDeleteShader(axis_fragment_shader);
    
    // Create VAO and VBO for the model
    glGenVertexArrays(1, &viewer->vao);
    glGenBuffers(1, &viewer->vbo);

    // Create and set up axis visualization
    glGenVertexArrays(1, &viewer->axis_vao);
    glGenBuffers(1, &viewer->axis_vbo);
    
    // Create axis vertex data (position and color for each vertex)
    float axis_vertices[] = {
        // X axis (red)
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  // Origin
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,  // X direction
        
        // Y axis (green)
        0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  // Origin
        0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  // Y direction
        
        // Z axis (blue)
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  // Origin
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f   // Z direction
    };
    
    glBindVertexArray(viewer->axis_vao);
    glBindBuffer(GL_ARRAY_BUFFER, viewer->axis_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axis_vertices), axis_vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Initialize viewer state
    viewer->num_triangles = 0;
    viewer->vertex_data = NULL;
    viewer->triangle_colors = NULL;
    viewer->edge_colors = NULL;
    viewer->triangle_checked = NULL;
    viewer->edge_checked = NULL;
    
    // Set camera position to (20,20,20)
    viewer->camera_pos[0] = 20.0f;
    viewer->camera_pos[1] = 20.0f;
    viewer->camera_pos[2] = 20.0f;
    
    // Initialize camera target (will be updated when model is loaded)
    viewer->camera_target[0] = 0.0f;
    viewer->camera_target[1] = 0.0f;
    viewer->camera_target[2] = 0.0f;
    
    // Set world up vector to Y-axis
    viewer->world_up[0] = 0.0f;
    viewer->world_up[1] = -1.0f;
    viewer->world_up[2] = 0.0f;
    
    viewer->rotation_speed = 0.005f;
    viewer->window_width = width;
    viewer->window_height = height;
    viewer->vertex_data = NULL;
    viewer->num_vertices = 0;
    
    return viewer;
}

// Load STL data into the viewer
int viewer_load_stl(stl_viewer_t* viewer, const stl_file_t* stl) {
    if (!viewer || !stl) return 0;
    
    // Update triangle count
    viewer->num_triangles = stl->num_triangles;
    
    // Allocate vertex data array (position + normal + color for each vertex)
    size_t vertex_data_size = stl->num_triangles * 3 * 9 * sizeof(float); // 3 vertices per triangle, 9 floats per vertex
    float* vertex_data = malloc(vertex_data_size);
    if (!vertex_data) return 0;
    
    // Allocate color arrays
    viewer->triangle_colors = malloc(stl->num_triangles * 3 * sizeof(float)); // RGB per triangle
    viewer->edge_colors = malloc(stl->num_triangles * 3 * 3 * sizeof(float)); // RGB per edge (3 edges per triangle)
    viewer->triangle_checked = calloc(stl->num_triangles, sizeof(int));
    viewer->edge_checked = calloc(stl->num_triangles * 3, sizeof(int));
    
    if (!viewer->triangle_colors || !viewer->edge_colors || 
        !viewer->triangle_checked || !viewer->edge_checked) {
        free(vertex_data);
        free(viewer->triangle_colors);
        free(viewer->edge_colors);
        free(viewer->triangle_checked);
        free(viewer->edge_checked);
        return 0;
    }
    
    // Calculate model center
    float min_bounds[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max_bounds[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    
    // Fill vertex data array and calculate bounds
    size_t idx = 0;
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        for (int j = 0; j < 3; j++) {
            // Update bounds
            for (int k = 0; k < 3; k++) {
                if (tri->vertices[j][k] < min_bounds[k]) min_bounds[k] = tri->vertices[j][k];
                if (tri->vertices[j][k] > max_bounds[k]) max_bounds[k] = tri->vertices[j][k];
            }
            
            // Position
            vertex_data[idx++] = tri->vertices[j][0];
            vertex_data[idx++] = tri->vertices[j][1];
            vertex_data[idx++] = tri->vertices[j][2];
            // Normal
            vertex_data[idx++] = tri->normal[0];
            vertex_data[idx++] = tri->normal[1];
            vertex_data[idx++] = tri->normal[2];
            // Color (default to light gray)
            vertex_data[idx++] = 0.7f;
            vertex_data[idx++] = 0.7f;
            vertex_data[idx++] = 0.7f;
        }
    }
    
    // Calculate model center and bounding box
    float max_dimension = 0.0f;
    for (int i = 0; i < 3; i++) {
        // Set camera target to model center
        viewer->camera_target[i] = (min_bounds[i] + max_bounds[i]) * 0.5f;
        
        float dimension = max_bounds[i] - min_bounds[i];
        if (dimension > max_dimension) max_dimension = dimension;
    }
    
    // Calculate required distance based on FOV and model size
    float fov = 45.0f * M_PI / 180.0f;  // 45 degrees in radians
    float half_fov = fov * 0.5f;
    
    // Distance needed to fit bounding box in view with 15% margin
    float required_dist = (max_dimension * 1.15f) / (2.0f * tanf(half_fov));
    
    // Unit vector in (1,1,1) direction
    float dir_x = 1.0f / sqrtf(3.0f);
    float dir_y = 1.0f / sqrtf(3.0f);
    float dir_z = 1.0f / sqrtf(3.0f);
    
    // Position camera along (1,1,1) at the required distance
    viewer->camera_pos[0] = viewer->camera_target[0] + (dir_x * required_dist);
    viewer->camera_pos[1] = viewer->camera_target[1] + (dir_y * required_dist);
    viewer->camera_pos[2] = viewer->camera_target[2] + (dir_z * required_dist);
    
    // Upload data to GPU
    glBindVertexArray(viewer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, viewer->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_data_size, vertex_data, GL_STATIC_DRAW);
    
    // Set vertex attributes
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Store vertex data
    free(viewer->vertex_data);
    viewer->vertex_data = vertex_data;
    viewer->num_vertices = stl->num_triangles * 3;
    
    return 1;
}

// Display the STL model
void viewer_display(stl_viewer_t* viewer) {
    if (!viewer) return;
    
    // Clear the screen
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable depth testing and proper depth function
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);  // Ensure proper depth testing
    glClearDepth(1.0f);
    
    // Use shader program
    glUseProgram(viewer->shader);
    
    // Calculate view matrix using lookAt approach
    float view[16] = {0};
    
    // Calculate forward vector (from camera_pos to target)
    float forward[3] = {
        viewer->camera_target[0] - viewer->camera_pos[0],
        viewer->camera_target[1] - viewer->camera_pos[1],
        viewer->camera_target[2] - viewer->camera_pos[2]
    };
    
    // Normalize forward vector
    float forward_len = sqrtf(forward[0] * forward[0] + 
                            forward[1] * forward[1] + 
                            forward[2] * forward[2]);
    forward[0] /= forward_len;
    forward[1] /= forward_len;
    forward[2] /= forward_len;
    
    // Calculate right vector (cross product of forward and world_up)
    float right[3] = {
        viewer->world_up[1] * forward[2] - viewer->world_up[2] * forward[1],
        viewer->world_up[2] * forward[0] - viewer->world_up[0] * forward[2],
        viewer->world_up[0] * forward[1] - viewer->world_up[1] * forward[0]
    };
    
    // Normalize right vector
    float right_len = sqrtf(right[0] * right[0] + 
                          right[1] * right[1] + 
                          right[2] * right[2]);
    right[0] /= right_len;
    right[1] /= right_len;
    right[2] /= right_len;
    
    // Calculate new up vector (cross product of right and forward)
    float up[3] = {
        right[1] * forward[2] - right[2] * forward[1],
        right[2] * forward[0] - right[0] * forward[2],
        right[0] * forward[1] - right[1] * forward[0]
    };
    
    // Build view matrix
    view[0] = right[0];
    view[1] = up[0];
    view[2] = -forward[0];
    view[3] = 0.0f;
    
    view[4] = right[1];
    view[5] = up[1];
    view[6] = -forward[1];
    view[7] = 0.0f;
    
    view[8] = right[2];
    view[9] = up[2];
    view[10] = -forward[2];
    view[11] = 0.0f;
    
    // Translation
    view[12] = -(right[0] * viewer->camera_pos[0] + right[1] * viewer->camera_pos[1] + right[2] * viewer->camera_pos[2]);
    view[13] = -(up[0] * viewer->camera_pos[0] + up[1] * viewer->camera_pos[1] + up[2] * viewer->camera_pos[2]);
    view[14] = (forward[0] * viewer->camera_pos[0] + forward[1] * viewer->camera_pos[1] + forward[2] * viewer->camera_pos[2]);
    view[15] = 1.0f;
    
    // Set uniforms for solid mesh shader
    
    // Set up projection matrix
    float aspect = (float)viewer->window_width / (float)viewer->window_height;
    float fov = 45.0f * M_PI / 180.0f;
    
    // Calculate dynamic clipping planes based on model size and camera distance
    float camera_distance = sqrtf(
        (viewer->camera_pos[0] - viewer->camera_target[0]) * (viewer->camera_pos[0] - viewer->camera_target[0]) +
        (viewer->camera_pos[1] - viewer->camera_target[1]) * (viewer->camera_pos[1] - viewer->camera_target[1]) +
        (viewer->camera_pos[2] - viewer->camera_target[2]) * (viewer->camera_pos[2] - viewer->camera_target[2])
    );
    
    // Near plane: close to camera but not too close
    float near = camera_distance * 0.01f;  // 1% of camera distance
    if (near < 0.1f) near = 0.1f;  // Minimum near plane
    
    // Far plane: beyond the model, with some margin
    float far = camera_distance * 2.0f;  // 2x camera distance for safety
    
    // Debug: Log clipping plane information (commented out to prevent terminal spam)
    // printf("DEBUG: Camera distance: %.3f, Near plane: %.3f, Far plane: %.3f\n", 
    //        camera_distance, near, far);
    
    float f = 1.0f / tanf(fov / 2.0f);
    
    float projection[16] = {0};
    projection[0] = f / aspect;
    projection[5] = f;
    projection[10] = (far + near) / (near - far);
    projection[11] = -1.0f;
    projection[14] = (2.0f * far * near) / (near - far);
    
    // Set uniforms for solid mesh shader
    glUniformMatrix4fv(glGetUniformLocation(viewer->shader, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(viewer->shader, "projection"), 1, GL_FALSE, projection);
    
    // Identity matrix for model
    float model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(viewer->shader, "model"), 1, GL_FALSE, model);
    
    // Light direction
    float light_dir[3] = {0.2f, 1.0f, 0.3f};
    float light_len = sqrtf(light_dir[0] * light_dir[0] + light_dir[1] * light_dir[1] + light_dir[2] * light_dir[2]);
    light_dir[0] /= light_len;
    light_dir[1] /= light_len;
    light_dir[2] /= light_len;
    glUniform3fv(glGetUniformLocation(viewer->shader, "lightDir"), 1, light_dir);
    
    // Draw the solid model
    glBindVertexArray(viewer->vao);
    glDrawArrays(GL_TRIANGLES, 0, viewer->num_vertices);

    // Draw the edges
    glUseProgram(viewer->edge_shader);
    glUniformMatrix4fv(glGetUniformLocation(viewer->edge_shader, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(viewer->edge_shader, "projection"), 1, GL_FALSE, projection);
    glUniformMatrix4fv(glGetUniformLocation(viewer->edge_shader, "model"), 1, GL_FALSE, model);
    
    // Enable polygon offset to prevent z-fighting
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    
    // Draw edges using line mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);
    glDrawArrays(GL_TRIANGLES, 0, viewer->num_vertices);
    
    // Reset polygon mode
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Draw axis lines
    glUseProgram(viewer->axis_shader);
    
    // Set uniforms for axis shader
    glUniformMatrix4fv(glGetUniformLocation(viewer->axis_shader, "view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(viewer->axis_shader, "projection"), 1, GL_FALSE, projection);
    
    // Create model matrix for axis that scales them relative to model size
    // Calculate camera distance for axis scaling
    float camera_dist = sqrtf(viewer->camera_pos[0] * viewer->camera_pos[0] +
                            viewer->camera_pos[1] * viewer->camera_pos[1] +
                            viewer->camera_pos[2] * viewer->camera_pos[2]);
    float axis_scale = camera_dist * 0.2f;  // Scale axes based on camera distance
    float axis_model[16] = {
        axis_scale, 0.0f, 0.0f, 0.0f,
        0.0f, axis_scale, 0.0f, 0.0f,
        0.0f, 0.0f, axis_scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(viewer->axis_shader, "model"), 1, GL_FALSE, axis_model);
    
    // Draw the axis lines
    glBindVertexArray(viewer->axis_vao);
    glLineWidth(2.0f);  // Make axis lines thicker
    glDrawArrays(GL_LINES, 0, 6);  // 3 axes, 2 vertices each
}

// Clean up viewer resources
void viewer_set_triangle_color(stl_viewer_t* viewer, unsigned int triangle_index, float r, float g, float b) {
    if (!viewer || triangle_index >= viewer->num_triangles) return;
    
    // Update color in triangle colors array
    viewer->triangle_colors[triangle_index * 3] = r;
    viewer->triangle_colors[triangle_index * 3 + 1] = g;
    viewer->triangle_colors[triangle_index * 3 + 2] = b;
    
    // Update vertex buffer for all vertices of this triangle
    for (int i = 0; i < 3; i++) {
        unsigned int vertex_offset = triangle_index * 3 * 9 + i * 9; // 9 floats per vertex (pos + normal + color)
        viewer->vertex_data[vertex_offset + 6] = r;
        viewer->vertex_data[vertex_offset + 7] = g;
        viewer->vertex_data[vertex_offset + 8] = b;
    }
    
    // Upload updated data to GPU
    glBindBuffer(GL_ARRAY_BUFFER, viewer->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, viewer->num_vertices * 9 * sizeof(float), viewer->vertex_data);
}

void viewer_set_edge_color(stl_viewer_t* viewer, unsigned int edge_index, float r, float g, float b) {
    if (!viewer || edge_index >= viewer->num_triangles * 3) return; // 3 edges per triangle
    
    // Update color in edge colors array
    viewer->edge_colors[edge_index * 3] = r;
    viewer->edge_colors[edge_index * 3 + 1] = g;
    viewer->edge_colors[edge_index * 3 + 2] = b;
}

void viewer_mark_triangle_checked(stl_viewer_t* viewer, unsigned int triangle_index) {
    if (!viewer || triangle_index >= viewer->num_triangles) return;
    
    viewer->triangle_checked[triangle_index] = 1;
    
    // Change color to indicate checked status (light green)
    viewer_set_triangle_color(viewer, triangle_index, 0.5f, 1.0f, 0.5f);
}

void viewer_mark_edge_checked(stl_viewer_t* viewer, unsigned int edge_index) {
    if (!viewer || edge_index >= viewer->num_triangles * 3) return;
    
    viewer->edge_checked[edge_index] = 1;
    
    // Change color to indicate checked status (blue)
    viewer_set_edge_color(viewer, edge_index, 0.0f, 0.0f, 1.0f);
}

// Global viewer pointer for callback access
static stl_viewer_t* g_viewer = NULL;

void topology_edge_callback(unsigned int edge_index, void* user_data) {
    (void)user_data; // Unused
    if (g_viewer) {
        // Mark edge as processed
        viewer_mark_edge_checked(g_viewer, edge_index);
        
        // Update display to show progress
        viewer_display(g_viewer);
        glfwSwapBuffers(g_viewer->window);
        glfwPollEvents();
        
    }
}

void display_topology_visualization(const stl_file_t* stl) {
    if (!stl) return;
    
    // Initialize viewer
    g_viewer = viewer_init(800, 600);
    if (!g_viewer) return;
    
    // Load STL data
    if (!viewer_load_stl(g_viewer, stl)) {
        viewer_cleanup(g_viewer);
        g_viewer = NULL;
        return;
    }
    
    // Create topology evaluation
    topology_evaluation_t* eval = malloc(sizeof(topology_evaluation_t));
    if (!eval) {
        viewer_cleanup(g_viewer);
        g_viewer = NULL;
        return;
    }
    
    // Initialize evaluation structure
    memset(eval, 0, sizeof(topology_evaluation_t));
    
    // Initialize vertices
    eval->num_vertices = stl->num_triangles * 3;
    eval->vertices = malloc(eval->num_vertices * sizeof(topology_vertex_t));
    if (!eval->vertices) {
        free(eval);
        viewer_cleanup(g_viewer);
        g_viewer = NULL;
        return;
    }
    
    // Find unique vertices
    eval->num_vertices = find_unique_vertices(stl, eval->vertices);
    
    // Initialize triangles
    eval->num_triangles = stl->num_triangles;
    eval->triangles = malloc(eval->num_triangles * sizeof(topology_triangle_t));
    if (!eval->triangles) {
        free(eval->vertices);
        free(eval);
        viewer_cleanup(g_viewer);
        g_viewer = NULL;
        return;
    }
    
    // Initialize edges (estimate)
    eval->num_edges = stl->num_triangles * 3;
    eval->edges = malloc(eval->num_edges * sizeof(topology_edge_t));
    if (!eval->edges) {
        free(eval->triangles);
        free(eval->vertices);
        free(eval);
        viewer_cleanup(g_viewer);
        g_viewer = NULL;
        return;
    }
    
    printf("Building edge list with visualization. Press ESC to stop...\n");
    
    // Build edge list with visualization callback
    eval->num_edges = build_edge_list(stl, eval, topology_edge_callback, NULL);
    
    // Wait for ESC key
    while (!glfwWindowShouldClose(g_viewer->window)) {
        viewer_display(g_viewer);
        glfwSwapBuffers(g_viewer->window);
        glfwPollEvents();
        
        if (glfwGetKey(g_viewer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(g_viewer->window, GLFW_TRUE);
        }
    }
    
    // Cleanup
    free(eval->edges);
    free(eval->triangles);
    free(eval->vertices);
    free(eval);
    viewer_cleanup(g_viewer);
    g_viewer = NULL;
}

void viewer_cleanup(stl_viewer_t* viewer) {
    if (!viewer) return;
    
    glDeleteVertexArrays(1, &viewer->vao);
    glDeleteBuffers(1, &viewer->vbo);
    glDeleteVertexArrays(1, &viewer->axis_vao);
    glDeleteBuffers(1, &viewer->axis_vbo);
    glDeleteProgram(viewer->shader);
    glDeleteProgram(viewer->edge_shader);
    glDeleteProgram(viewer->axis_shader);
    
    free(viewer->vertex_data);
    free(viewer);
    
    glfwTerminate();
}

// Generate cutting plane geometry as triangles within mesh bounds
int generate_cutting_plane_triangles(const cutting_plane_t* plane, const float* mesh_bounds, 
                                    float* vertices, float* normals, float* colors, float plane_size) {
    if (!plane->is_valid) return 0;
    
    // Calculate plane bounds based on mesh bounds and desired size
    float mesh_size = sqrtf(
        (mesh_bounds[3] - mesh_bounds[0]) * (mesh_bounds[3] - mesh_bounds[0]) +
        (mesh_bounds[4] - mesh_bounds[1]) * (mesh_bounds[4] - mesh_bounds[1]) +
        (mesh_bounds[5] - mesh_bounds[2]) * (mesh_bounds[5] - mesh_bounds[2])
    );
    float half_size = mesh_size * plane_size * 0.5f;
    
    // Create two perpendicular vectors in the plane
    float up[3] = {0.0f, 1.0f, 0.0f};
    float right[3];
    
    // Cross product: right = normal × up
    right[0] = plane->normal[1] * up[2] - plane->normal[2] * up[1];
    right[1] = plane->normal[2] * up[0] - plane->normal[0] * up[2];
    right[2] = plane->normal[0] * up[1] - plane->normal[1] * up[0];
    
    // If normal is parallel to up, use different reference
    float right_len = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    if (right_len < 1e-6f) {
        up[0] = 1.0f; up[1] = 0.0f; up[2] = 0.0f;
        right[0] = plane->normal[1] * up[2] - plane->normal[2] * up[1];
        right[1] = plane->normal[2] * up[0] - plane->normal[0] * up[2];
        right[2] = plane->normal[0] * up[1] - plane->normal[1] * up[0];
        right_len = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
    }
    
    // Normalize right vector
    if (right_len > 1e-6f) {
        right[0] /= right_len;
        right[1] /= right_len;
        right[2] /= right_len;
    }
    
    // Calculate up vector in plane: up = right × normal
    up[0] = right[1] * plane->normal[2] - right[2] * plane->normal[1];
    up[1] = right[2] * plane->normal[0] - right[0] * plane->normal[2];
    up[2] = right[0] * plane->normal[1] - right[1] * plane->normal[0];
    
    // Generate quad vertices centered on plane center
    float quad_verts[4][3];
    for (int i = 0; i < 4; i++) {
        float x_sign = (i == 0 || i == 3) ? -1.0f : 1.0f;
        float y_sign = (i < 2) ? -1.0f : 1.0f;
        
        quad_verts[i][0] = plane->center[0] + half_size * (x_sign * right[0] + y_sign * up[0]);
        quad_verts[i][1] = plane->center[1] + half_size * (x_sign * right[1] + y_sign * up[1]);
        quad_verts[i][2] = plane->center[2] + half_size * (x_sign * right[2] + y_sign * up[2]);
    }
    
    // Create two triangles from the quad
    // Triangle 1: 0, 1, 2
    // Triangle 2: 0, 2, 3
    int tri_indices[6] = {0, 1, 2, 0, 2, 3};
    
    // Semi-transparent blue color for cutting planes
    float plane_color[3] = {0.3f, 0.6f, 1.0f};
    
    for (int tri = 0; tri < 2; tri++) {
        for (int vert = 0; vert < 3; vert++) {
            int idx = tri * 3 + vert;
            int quad_idx = tri_indices[idx];
            
            // Copy vertex position
            vertices[idx * 3 + 0] = quad_verts[quad_idx][0];
            vertices[idx * 3 + 1] = quad_verts[quad_idx][1];
            vertices[idx * 3 + 2] = quad_verts[quad_idx][2];
            
            // Copy normal
            normals[idx * 3 + 0] = plane->normal[0];
            normals[idx * 3 + 1] = plane->normal[1];
            normals[idx * 3 + 2] = plane->normal[2];
            
            // Copy color
            colors[idx * 3 + 0] = plane_color[0];
            colors[idx * 3 + 1] = plane_color[1];
            colors[idx * 3 + 2] = plane_color[2];
        }
    }
    
    return 6; // 2 triangles × 3 vertices = 6 vertices
}

// Collect all cutting planes from the tree recursively
void collect_cutting_planes(const mesh_tree_node_t* node, cutting_plane_t* planes, int* count, int max_planes) {
    if (!node || *count >= max_planes) return;
    
    if (node->cutting_plane.is_valid) {
        planes[*count] = node->cutting_plane;
        (*count)++;
    }
    
    if (node->left_child) {
        collect_cutting_planes(node->left_child, planes, count, max_planes);
    }
    if (node->right_child) {
        collect_cutting_planes(node->right_child, planes, count, max_planes);
    }
}

// Print cutting plane information recursively
void print_cutting_planes(const mesh_tree_node_t* node, int indent) {
    if (!node) return;
    
    // Print indentation
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
    
    if (node->cutting_plane.is_valid) {
        printf("Node depth=%d: Cutting plane defined by:\n", node->depth);
        
        for (int i = 0; i < indent + 1; i++) {
            printf("  ");
        }
        printf("Point 1: (%.3f, %.3f, %.3f)\n", 
               node->cutting_plane.point1[0], node->cutting_plane.point1[1], node->cutting_plane.point1[2]);
        
        for (int i = 0; i < indent + 1; i++) {
            printf("  ");
        }
        printf("Point 2: (%.3f, %.3f, %.3f)\n", 
               node->cutting_plane.point2[0], node->cutting_plane.point2[1], node->cutting_plane.point2[2]);
        
        for (int i = 0; i < indent + 1; i++) {
            printf("  ");
        }
        printf("Point 3: (%.3f, %.3f, %.3f)\n", 
               node->cutting_plane.point3[0], node->cutting_plane.point3[1], node->cutting_plane.point3[2]);
        
        for (int i = 0; i < indent + 1; i++) {
            printf("  ");
        }
        printf("Normal: (%.3f, %.3f, %.3f)\n", 
               node->cutting_plane.normal[0], node->cutting_plane.normal[1], node->cutting_plane.normal[2]);
        
        for (int i = 0; i < indent + 1; i++) {
            printf("  ");
        }
        printf("Center: (%.3f, %.3f, %.3f)\n", 
               node->cutting_plane.center[0], node->cutting_plane.center[1], node->cutting_plane.center[2]);
    } else {
        printf("Node depth=%d: No cutting plane (leaf node)\n", node->depth);
    }
    
    // Recursively print child nodes
    if (node->left_child) {
        print_cutting_planes(node->left_child, indent + 1);
    }
    if (node->right_child) {
        print_cutting_planes(node->right_child, indent + 1);
    }
}

// Generate random color for a given index using a simple hash-based approach
void generate_random_color(int index, float* r, float* g, float* b) {
    // Use a simple hash function to generate consistent random colors for each index
    unsigned int hash = (unsigned int)index;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
    hash = (hash >> 16) ^ hash;
    
    // Extract RGB components from hash
    *r = ((hash & 0xFF) / 255.0f);
    *g = (((hash >> 8) & 0xFF) / 255.0f);
    *b = (((hash >> 16) & 0xFF) / 255.0f);
    
    // Ensure colors are vibrant (avoid very dark colors)
    if (*r < 0.3f) *r += 0.3f;
    if (*g < 0.3f) *g += 0.3f;
    if (*b < 0.3f) *b += 0.3f;
    
    // Clamp to [0,1]
    if (*r > 1.0f) *r = 1.0f;
    if (*g > 1.0f) *g = 1.0f;
    if (*b > 1.0f) *b = 1.0f;
}

// Load and display decomposition tree in viewer with random colors
int viewer_load_decomposition_tree(stl_viewer_t* viewer, const decomposition_tree_t* tree, int show_cutting_planes) {
    if (!viewer || !tree || !tree->root) {
        return 0;
    }
    
    // Get all leaf nodes (final decomposed meshes)
    mesh_tree_node_t* leaves[256];
    int num_leaves = get_leaf_nodes(tree, leaves, 256);
    
    if (num_leaves == 0) {
        printf("No leaf nodes found in decomposition tree\n");
        return 0;
    }
    
    printf("Loading %d decomposed mesh pieces with random colors\n", num_leaves);
    
    // Calculate total triangles needed for mesh pieces
    unsigned int mesh_triangles = 0;
    for (int i = 0; i < num_leaves; i++) {
        if (leaves[i]->mesh) {
            mesh_triangles += leaves[i]->mesh->num_triangles;
        }
    }
    
    // Add cutting plane triangles if requested
    unsigned int cutting_plane_triangles = 0;
    cutting_plane_t cutting_planes[64];  // Support up to 64 cutting planes
    int num_cutting_planes = 0;
    
    if (show_cutting_planes) {
        collect_cutting_planes(tree->root, cutting_planes, &num_cutting_planes, 64);
        cutting_plane_triangles = num_cutting_planes * 2; // 2 triangles per plane
        printf("Adding %d cutting planes (%d triangles) to visualization\n", 
               num_cutting_planes, cutting_plane_triangles);
    }
    
    unsigned int total_triangles = mesh_triangles + cutting_plane_triangles;
    if (total_triangles == 0) {
        printf("No triangles found in decomposed meshes\n");
        return 0;
    }
    
    // Allocate vertex data for all triangles
    unsigned int total_vertices = total_triangles * 3;
    viewer->vertex_data = malloc(total_vertices * 9 * sizeof(float)); // 3 pos + 3 normal + 3 color per vertex
    viewer->triangle_colors = malloc(total_triangles * 3 * sizeof(float)); // RGB per triangle
    viewer->triangle_checked = malloc(total_triangles * sizeof(int));
    
    if (!viewer->vertex_data || !viewer->triangle_colors || !viewer->triangle_checked) {
        printf("Failed to allocate memory for decomposed meshes\n");
        return 0;
    }
    
    viewer->num_vertices = total_vertices;
    viewer->num_triangles = total_triangles;
    
    // Fill vertex data with colored decomposed meshes
    unsigned int vertex_offset = 0;
    unsigned int triangle_offset = 0;
    
    for (int leaf_idx = 0; leaf_idx < num_leaves; leaf_idx++) {
        const stl_file_t* mesh = leaves[leaf_idx]->mesh;
        if (!mesh) continue;
        
        // Generate random color for this leaf node
        float r, g, b;
        generate_random_color(leaf_idx, &r, &g, &b);
        
        printf("Leaf %d: %u triangles, color (%.2f, %.2f, %.2f), concavity %.3f\n", 
               leaf_idx, mesh->num_triangles, r, g, b, leaves[leaf_idx]->concavity_score);
        
        // Add triangles from this mesh
        for (unsigned int tri_idx = 0; tri_idx < mesh->num_triangles; tri_idx++) {
            const stl_triangle_t* triangle = &mesh->triangles[tri_idx];
            
            // Store triangle color
            viewer->triangle_colors[triangle_offset * 3 + 0] = r;
            viewer->triangle_colors[triangle_offset * 3 + 1] = g;
            viewer->triangle_colors[triangle_offset * 3 + 2] = b;
            viewer->triangle_checked[triangle_offset] = 0;
            
            // Add the three vertices of the triangle
            for (int v = 0; v < 3; v++) {
                unsigned int vert_idx = vertex_offset + v;
                
                // Position (XYZ)
                viewer->vertex_data[vert_idx * 9 + 0] = triangle->vertices[v][0];
                viewer->vertex_data[vert_idx * 9 + 1] = triangle->vertices[v][1];
                viewer->vertex_data[vert_idx * 9 + 2] = triangle->vertices[v][2];
                
                // Normal (XYZ)
                viewer->vertex_data[vert_idx * 9 + 3] = triangle->normal[0];
                viewer->vertex_data[vert_idx * 9 + 4] = triangle->normal[1];
                viewer->vertex_data[vert_idx * 9 + 5] = triangle->normal[2];
                
                // Color (RGB)
                viewer->vertex_data[vert_idx * 9 + 6] = r;
                viewer->vertex_data[vert_idx * 9 + 7] = g;
                viewer->vertex_data[vert_idx * 9 + 8] = b;
            }
            
            vertex_offset += 3;
            triangle_offset++;
        }
    }
    
    // Add cutting plane geometry if requested
    if (show_cutting_planes && num_cutting_planes > 0) {
        // Calculate overall mesh bounds for plane sizing
        float overall_bounds[6] = {FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (int leaf_idx = 0; leaf_idx < num_leaves; leaf_idx++) {
            const stl_file_t* mesh = leaves[leaf_idx]->mesh;
            if (mesh) {
                if (mesh->bounds[0] < overall_bounds[0]) overall_bounds[0] = mesh->bounds[0];
                if (mesh->bounds[1] < overall_bounds[1]) overall_bounds[1] = mesh->bounds[1];
                if (mesh->bounds[2] < overall_bounds[2]) overall_bounds[2] = mesh->bounds[2];
                if (mesh->bounds[3] > overall_bounds[3]) overall_bounds[3] = mesh->bounds[3];
                if (mesh->bounds[4] > overall_bounds[4]) overall_bounds[4] = mesh->bounds[4];
                if (mesh->bounds[5] > overall_bounds[5]) overall_bounds[5] = mesh->bounds[5];
            }
        }
        
        // Add each cutting plane as 2 triangles (6 vertices)
        for (int plane_idx = 0; plane_idx < num_cutting_planes; plane_idx++) {
            float plane_vertices[18];  // 6 vertices × 3 coords = 18 floats
            float plane_normals[18];   // 6 vertices × 3 coords = 18 floats
            float plane_colors[18];    // 6 vertices × 3 coords = 18 floats
            
            int verts_generated = generate_cutting_plane_triangles(
                &cutting_planes[plane_idx], overall_bounds, 
                plane_vertices, plane_normals, plane_colors, 0.8f  // 80% of mesh size
            );
            
            if (verts_generated == 6) {
                // Copy cutting plane vertices to main vertex data
                for (int v = 0; v < 6; v++) {
                    int vert_idx = vertex_offset + v;
                    
                    // Position (XYZ)
                    viewer->vertex_data[vert_idx * 9 + 0] = plane_vertices[v * 3 + 0];
                    viewer->vertex_data[vert_idx * 9 + 1] = plane_vertices[v * 3 + 1];
                    viewer->vertex_data[vert_idx * 9 + 2] = plane_vertices[v * 3 + 2];
                    
                    // Normal (XYZ)
                    viewer->vertex_data[vert_idx * 9 + 3] = plane_normals[v * 3 + 0];
                    viewer->vertex_data[vert_idx * 9 + 4] = plane_normals[v * 3 + 1];
                    viewer->vertex_data[vert_idx * 9 + 5] = plane_normals[v * 3 + 2];
                    
                    // Color (RGB) - semi-transparent blue
                    viewer->vertex_data[vert_idx * 9 + 6] = plane_colors[v * 3 + 0];
                    viewer->vertex_data[vert_idx * 9 + 7] = plane_colors[v * 3 + 1];
                    viewer->vertex_data[vert_idx * 9 + 8] = plane_colors[v * 3 + 2];
                }
                
                vertex_offset += 6;
                
                // Set triangle colors for the 2 triangles of this plane
                for (int t = 0; t < 2; t++) {
                    viewer->triangle_colors[triangle_offset * 3 + 0] = 0.3f; // Blue
                    viewer->triangle_colors[triangle_offset * 3 + 1] = 0.6f;
                    viewer->triangle_colors[triangle_offset * 3 + 2] = 1.0f;
                    viewer->triangle_checked[triangle_offset] = 0;
                    triangle_offset++;
                }
            }
        }
    }
    
    // Set up camera position (exactly same as regular STL viewer)
    // Calculate model center and bounding box from all vertices
    float min_bounds[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float max_bounds[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    
    // Calculate bounds from all decomposed mesh vertices
    printf("DEBUG: Calculating bounds from %d leaf meshes\n", num_leaves);
    for (int leaf_idx = 0; leaf_idx < num_leaves; leaf_idx++) {
        const stl_file_t* mesh = leaves[leaf_idx]->mesh;
        if (!mesh) continue;
        
        printf("DEBUG: Leaf %d has %u triangles\n", leaf_idx, mesh->num_triangles);
        
        for (unsigned int i = 0; i < mesh->num_triangles; i++) {
            const stl_triangle_t* tri = &mesh->triangles[i];
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    if (tri->vertices[j][k] < min_bounds[k]) min_bounds[k] = tri->vertices[j][k];
                    if (tri->vertices[j][k] > max_bounds[k]) max_bounds[k] = tri->vertices[j][k];
                }
            }
        }
    }
    
    // Calculate model center and bounding box (exactly same as regular viewer)
    float max_dimension = 0.0f;
    for (int i = 0; i < 3; i++) {
        // Set camera target to model center
        viewer->camera_target[i] = (min_bounds[i] + max_bounds[i]) * 0.5f;
        
        float dimension = max_bounds[i] - min_bounds[i];
        if (dimension > max_dimension) max_dimension = dimension;
    }
    
    // Calculate required distance based on FOV and model size (exactly same as regular viewer)
    float fov = 45.0f * M_PI / 180.0f;  // 45 degrees in radians
    float half_fov = fov * 0.5f;
    
    // Distance needed to fit bounding box in view with 15% margin
    float required_dist = (max_dimension * 1.15f) / (2.0f * tanf(half_fov));
    
    // Unit vector in (1,1,1) direction
    float dir_x = 1.0f / sqrtf(3.0f);
    float dir_y = 1.0f / sqrtf(3.0f);
    float dir_z = 1.0f / sqrtf(3.0f);
    
    // Position camera along (1,1,1) at the required distance
    viewer->camera_pos[0] = viewer->camera_target[0] + (dir_x * required_dist);
    viewer->camera_pos[1] = viewer->camera_target[1] + (dir_y * required_dist);
    viewer->camera_pos[2] = viewer->camera_target[2] + (dir_z * required_dist);
    
    // Log camera and model information for debugging (removed to prevent terminal spam)
    
    viewer->world_up[0] = 0.0f;
    viewer->world_up[1] = -1.0f;
    viewer->world_up[2] = 0.0f;
    viewer->rotation_speed = 0.005f;
    
    // Upload vertex data to GPU
    glBindVertexArray(viewer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, viewer->vbo);
    glBufferData(GL_ARRAY_BUFFER, viewer->num_vertices * 9 * sizeof(float), viewer->vertex_data, GL_STATIC_DRAW);
    
    // Set up vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    return 1;
}

// Display convex decomposition tree with random colors for each node
void display_convex_decomposition_tree(const stl_file_t* stl, float concavity_threshold, int max_depth, plane_generation_method_t plane_method, int show_cutting_planes) {
    printf("Starting convex decomposition...\n");
    printf("Concavity threshold: %.3f\n", concavity_threshold);
    printf("Max depth: %d\n", max_depth);
    
    // Perform convex decomposition using the specified plane method
    decomposition_tree_t* tree = decompose_mesh_tree(stl, concavity_threshold, max_depth, plane_method);
    if (!tree) {
        printf("Failed to decompose mesh\n");
        return;
    }
    
    // Print tree structure
    print_decomposition_tree(tree);
    
    // Print cutting plane information if requested
    if (show_cutting_planes) {
        printf("\nCutting Plane Information:\n");
        printf("==========================\n");
        print_cutting_planes(tree->root, 0);
        printf("\n");
    }
    
    // Initialize viewer
    stl_viewer_t* viewer = viewer_init(1000, 800);
    if (!viewer) {
        printf("Failed to initialize viewer\n");
        free_decomposition_tree(tree);
        return;
    }
    
    // Load decomposition tree into viewer
    if (!viewer_load_decomposition_tree(viewer, tree, show_cutting_planes)) {
        printf("Failed to load decomposition tree into viewer\n");
        viewer_cleanup(viewer);
        free_decomposition_tree(tree);
        return;
    }
    
    printf("Convex decomposition viewer ready!\n");
    printf("Controls:\n");
    printf("  - Left mouse button + drag: Rotate view\n");
    printf("  - ESC: Exit viewer\n");
    printf("  - Each decomposed piece is shown in a different random color\n");
    
    // Main display loop
    while (!glfwWindowShouldClose(viewer->window)) {
        viewer_display(viewer);
        glfwSwapBuffers(viewer->window);
        glfwPollEvents();
        
        // Check for ESC key
        if (glfwGetKey(viewer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(viewer->window, GLFW_TRUE);
        }
    }
    
    // Cleanup
    viewer_cleanup(viewer);
    free_decomposition_tree(tree);
    printf("Convex decomposition viewer closed.\n");
}




