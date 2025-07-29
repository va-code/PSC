#ifndef STL_PARSER_H
#define STL_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// STL triangle structure
typedef struct {
    float normal[3];    // Normal vector (x, y, z)
    float vertices[3][3]; // Three vertices, each with (x, y, z) coordinates
} stl_triangle_t;

// STL file structure
typedef struct {
    char header[80];    // STL header (80 bytes)
    unsigned int num_triangles; // Number of triangles
    stl_triangle_t* triangles;  // Array of triangles
    float bounds[6];    // Bounding box: [min_x, min_y, min_z, max_x, max_y, max_z]
} stl_file_t;

// Function declarations
stl_file_t* stl_load_file(const char* filename);
void stl_free(stl_file_t* stl);
int stl_parse_ascii(FILE* file, stl_file_t* stl);
int stl_parse_binary(FILE* file, stl_file_t* stl);
void stl_calculate_bounds(stl_file_t* stl);
void stl_print_info(const stl_file_t* stl);

#endif // STL_PARSER_H 