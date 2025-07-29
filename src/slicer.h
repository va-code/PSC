#ifndef SLICER_H
#define SLICER_H

#include "stl_parser.h"
#include "bvh.h"
#include "convex_decomposition.h"

// Slicing parameters
typedef struct {
    float layer_height;     // Height of each layer
    float infill_density;   // Infill density (0.0 to 1.0)
    float shell_thickness;  // Shell thickness
    int num_shells;         // Number of shell layers
    float print_speed;      // Print speed (mm/s)
    float travel_speed;     // Travel speed (mm/s)
    float nozzle_diameter;  // Nozzle diameter (mm)
    float filament_diameter; // Filament diameter (mm)
} slicing_params_t;

// Point structure for 2D coordinates
typedef struct {
    float x, y;
} point2d_t;

// Contour structure (closed loop of points)
typedef struct {
    point2d_t* points;
    int num_points;
} contour_t;

// Layer structure
typedef struct {
    float z_height;         // Z height of this layer
    contour_t* contours;    // Array of contours (outer shell + holes)
    int num_contours;       // Number of contours
    point2d_t* infill_points; // Infill pattern points
    int num_infill_points;  // Number of infill points
} layer_t;

// Sliced model structure
typedef struct {
    layer_t* layers;        // Array of layers
    int num_layers;         // Number of layers
    slicing_params_t params; // Slicing parameters
} sliced_model_t;

// Function declarations
sliced_model_t* slice_model(const stl_file_t* stl, const slicing_params_t* params);
sliced_model_t* slice_model_with_bvh(const stl_file_t* stl, const slicing_params_t* params, 
                                    const spatial_partition_t* partition);
sliced_model_t* slice_model_with_convex_decomposition(const stl_file_t* stl, const slicing_params_t* params,
                                                     const convex_decomposition_t* decomp);
void free_sliced_model(sliced_model_t* model);
int calculate_num_layers(const stl_file_t* stl, float layer_height);
void generate_contours(layer_t* layer, const stl_file_t* stl, float z_height);
void generate_contours_with_bvh(layer_t* layer, const stl_file_t* stl, const spatial_partition_t* partition, 
                               float z_height, unsigned int partition_id);
void generate_contours_with_convex_parts(layer_t* layer, const stl_file_t* stl, const convex_decomposition_t* decomp,
                                        float z_height, unsigned int part_id);
void generate_infill(layer_t* layer, const slicing_params_t* params);
void print_slicing_info(const sliced_model_t* model);

#endif // SLICER_H 