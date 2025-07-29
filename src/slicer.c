#include "slicer.h"
#include <math.h>
#include <string.h>

sliced_model_t* slice_model(const stl_file_t* stl, const slicing_params_t* params) {
    if (!stl || !params) return NULL;
    
    sliced_model_t* model = malloc(sizeof(sliced_model_t));
    if (!model) return NULL;
    
    model->params = *params;
    model->num_layers = calculate_num_layers(stl, params->layer_height);
    model->layers = malloc(model->num_layers * sizeof(layer_t));
    
    if (!model->layers) {
        free(model);
        return NULL;
    }
    
    // Initialize layers
    for (int i = 0; i < model->num_layers; i++) {
        model->layers[i].z_height = stl->bounds[2] + i * params->layer_height;
        model->layers[i].contours = NULL;
        model->layers[i].num_contours = 0;
        model->layers[i].infill_points = NULL;
        model->layers[i].num_infill_points = 0;
    }
    
    // Generate contours and infill for each layer
    for (int i = 0; i < model->num_layers; i++) {
        generate_contours(&model->layers[i], stl, model->layers[i].z_height);
        generate_infill(&model->layers[i], params);
    }
    
    return model;
}

sliced_model_t* slice_model_with_bvh(const stl_file_t* stl, const slicing_params_t* params, 
                                     const spatial_partition_t* partition) {
    if (!stl || !params || !partition) return NULL;
    
    sliced_model_t* model = malloc(sizeof(sliced_model_t));
    if (!model) return NULL;
    
    model->params = *params;
    model->num_layers = calculate_num_layers(stl, params->layer_height);
    model->layers = malloc(model->num_layers * sizeof(layer_t));
    
    if (!model->layers) {
        free(model);
        return NULL;
    }
    
    // Initialize layers
    for (int i = 0; i < model->num_layers; i++) {
        model->layers[i].z_height = stl->bounds[2] + i * params->layer_height;
        model->layers[i].contours = NULL;
        model->layers[i].num_contours = 0;
        model->layers[i].infill_points = NULL;
        model->layers[i].num_infill_points = 0;
    }
    
    // Generate contours and infill for each layer using BVH partitions
    for (int i = 0; i < model->num_layers; i++) {
        // Generate contours for each partition
        for (unsigned int partition_id = 0; partition_id < partition->num_partitions; partition_id++) {
            generate_contours_with_bvh(&model->layers[i], stl, partition, model->layers[i].z_height, partition_id);
        }
        generate_infill(&model->layers[i], params);
    }
    
    return model;
}

void free_sliced_model(sliced_model_t* model) {
    if (!model) return;
    
    for (int i = 0; i < model->num_layers; i++) {
        layer_t* layer = &model->layers[i];
        
        // Free contours
        for (int j = 0; j < layer->num_contours; j++) {
            if (layer->contours[j].points) {
                free(layer->contours[j].points);
            }
        }
        if (layer->contours) {
            free(layer->contours);
        }
        
        // Free infill points
        if (layer->infill_points) {
            free(layer->infill_points);
        }
    }
    
    if (model->layers) {
        free(model->layers);
    }
    free(model);
}

int calculate_num_layers(const stl_file_t* stl, float layer_height) {
    if (layer_height <= 0) return 0;
    
    float model_height = stl->bounds[5] - stl->bounds[2];
    return (int)ceil(model_height / layer_height);
}

void generate_contours(layer_t* layer, const stl_file_t* stl, float z_height) {
    // This is a simplified contour generation
    // In a real implementation, you would:
    // 1. Find all triangles that intersect with the z-plane
    // 2. Calculate intersection points
    // 3. Sort points to form closed contours
    // 4. Handle multiple contours (outer shell + holes)
    
    // For now, create a simple bounding box contour
    float margin = 5.0f; // 5mm margin
    
    layer->num_contours = 1;
    layer->contours = malloc(sizeof(contour_t));
    layer->contours[0].num_points = 4;
    layer->contours[0].points = malloc(4 * sizeof(point2d_t));
    
    // Create a simple rectangular contour
    layer->contours[0].points[0] = (point2d_t){stl->bounds[0] - margin, stl->bounds[1] - margin};
    layer->contours[0].points[1] = (point2d_t){stl->bounds[3] + margin, stl->bounds[1] - margin};
    layer->contours[0].points[2] = (point2d_t){stl->bounds[3] + margin, stl->bounds[4] + margin};
    layer->contours[0].points[3] = (point2d_t){stl->bounds[0] - margin, stl->bounds[4] + margin};
}

sliced_model_t* slice_model_with_convex_decomposition(const stl_file_t* stl, const slicing_params_t* params,
                                                     const convex_decomposition_t* decomp) {
    if (!stl || !params || !decomp) return NULL;
    
    sliced_model_t* model = malloc(sizeof(sliced_model_t));
    if (!model) return NULL;
    
    model->params = *params;
    model->num_layers = calculate_num_layers(stl, params->layer_height);
    model->layers = malloc(model->num_layers * sizeof(layer_t));
    
    if (!model->layers) {
        free(model);
        return NULL;
    }
    
    // Initialize layers
    for (int i = 0; i < model->num_layers; i++) {
        model->layers[i].z_height = stl->bounds[2] + i * params->layer_height;
        model->layers[i].contours = NULL;
        model->layers[i].num_contours = 0;
        model->layers[i].infill_points = NULL;
        model->layers[i].num_infill_points = 0;
    }
    
    // Generate contours and infill for each layer using convex parts
    for (int i = 0; i < model->num_layers; i++) {
        // Generate contours for each convex part
        for (unsigned int part_id = 0; part_id < decomp->num_parts; part_id++) {
            generate_contours_with_convex_parts(&model->layers[i], stl, decomp, model->layers[i].z_height, part_id);
        }
        generate_infill(&model->layers[i], params);
    }
    
    return model;
}

void generate_contours_with_bvh(layer_t* layer, const stl_file_t* stl, const spatial_partition_t* partition, 
                               float z_height, unsigned int partition_id) {
    if (!layer || !stl || !partition || partition_id >= partition->num_partitions) return;
    
    // Get partition bounds
    unsigned int base_idx = partition_id * 6;
    float min_x = partition->partition_bounds[base_idx + 0];
    float min_y = partition->partition_bounds[base_idx + 1];
    float min_z = partition->partition_bounds[base_idx + 2];
    float max_x = partition->partition_bounds[base_idx + 3];
    float max_y = partition->partition_bounds[base_idx + 4];
    float max_z = partition->partition_bounds[base_idx + 5];
    
    // Check if this Z height intersects with the partition
    if (z_height < min_z || z_height > max_z) return;
    
    // Count triangles in this partition at this Z height
    unsigned int triangles_in_partition = 0;
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        if (partition->partition_ids[i] == partition_id) {
            const stl_triangle_t* triangle = &stl->triangles[i];
            
            // Check if triangle intersects with Z plane
            float min_triangle_z = triangle->vertices[0][2];
            float max_triangle_z = triangle->vertices[0][2];
            
            for (int j = 1; j < 3; j++) {
                if (triangle->vertices[j][2] < min_triangle_z) min_triangle_z = triangle->vertices[j][2];
                if (triangle->vertices[j][2] > max_triangle_z) max_triangle_z = triangle->vertices[j][2];
            }
            
            if (z_height >= min_triangle_z && z_height <= max_triangle_z) {
                triangles_in_partition++;
            }
        }
    }
    
    if (triangles_in_partition == 0) return;
    
    // Create contour for this partition
    float margin = 2.0f; // Smaller margin for partitions
    
    // Expand layer contours array
    layer->num_contours++;
    layer->contours = realloc(layer->contours, layer->num_contours * sizeof(contour_t));
    
    contour_t* new_contour = &layer->contours[layer->num_contours - 1];
    new_contour->num_points = 4;
    new_contour->points = malloc(4 * sizeof(point2d_t));
    
    // Create rectangular contour for this partition
    new_contour->points[0] = (point2d_t){min_x - margin, min_y - margin};
    new_contour->points[1] = (point2d_t){max_x + margin, min_y - margin};
    new_contour->points[2] = (point2d_t){max_x + margin, max_y + margin};
    new_contour->points[3] = (point2d_t){min_x - margin, max_y + margin};
}

void generate_contours_with_convex_parts(layer_t* layer, const stl_file_t* stl, const convex_decomposition_t* decomp,
                                        float z_height, unsigned int part_id) {
    if (!layer || !stl || !decomp || part_id >= decomp->num_parts) return;
    
    const convex_part_t* part = decomp->parts[part_id];
    if (!part || part->num_triangles == 0) return;
    
    // Check if this Z height intersects with the part
    if (z_height < part->hull.bounds[2] || z_height > part->hull.bounds[5]) return;
    
    // Count triangles in this part at this Z height
    unsigned int triangles_in_part = 0;
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        // Check if triangle intersects with Z plane
        float min_triangle_z = triangle->vertices[0][2];
        float max_triangle_z = triangle->vertices[0][2];
        
        for (int j = 1; j < 3; j++) {
            if (triangle->vertices[j][2] < min_triangle_z) min_triangle_z = triangle->vertices[j][2];
            if (triangle->vertices[j][2] > max_triangle_z) max_triangle_z = triangle->vertices[j][2];
        }
        
        if (z_height >= min_triangle_z && z_height <= max_triangle_z) {
            triangles_in_part++;
        }
    }
    
    if (triangles_in_part == 0) return;
    
    // Create contour for this convex part
    float margin = 1.0f; // Smaller margin for convex parts
    
    // Expand layer contours array
    layer->num_contours++;
    layer->contours = realloc(layer->contours, layer->num_contours * sizeof(contour_t));
    
    contour_t* new_contour = &layer->contours[layer->num_contours - 1];
    new_contour->num_points = 4;
    new_contour->points = malloc(4 * sizeof(point2d_t));
    
    // Create rectangular contour based on part bounds
    new_contour->points[0] = (point2d_t){part->hull.bounds[0] - margin, part->hull.bounds[1] - margin};
    new_contour->points[1] = (point2d_t){part->hull.bounds[3] + margin, part->hull.bounds[1] - margin};
    new_contour->points[2] = (point2d_t){part->hull.bounds[3] + margin, part->hull.bounds[4] + margin};
    new_contour->points[3] = (point2d_t){part->hull.bounds[0] - margin, part->hull.bounds[4] + margin};
}

void generate_infill(layer_t* layer, const slicing_params_t* params) {
    if (params->infill_density <= 0.0f) return;
    
    // Simple linear infill pattern
    // In a real implementation, you would generate more sophisticated patterns
    // like grid, honeycomb, triangles, etc.
    
    float spacing = 10.0f / params->infill_density; // Base spacing of 10mm
    if (spacing < 1.0f) spacing = 1.0f;
    
    // Calculate infill area (simplified - using bounding box)
    float min_x = layer->contours[0].points[0].x;
    float max_x = layer->contours[0].points[2].x;
    float min_y = layer->contours[0].points[0].y;
    float max_y = layer->contours[0].points[2].y;
    
    int num_lines = (int)((max_x - min_x) / spacing) + 1;
    layer->num_infill_points = num_lines * 2; // Start and end points for each line
    layer->infill_points = malloc(layer->num_infill_points * sizeof(point2d_t));
    
    for (int i = 0; i < num_lines; i++) {
        float x = min_x + i * spacing;
        layer->infill_points[i * 2] = (point2d_t){x, min_y};
        layer->infill_points[i * 2 + 1] = (point2d_t){x, max_y};
    }
}

void print_slicing_info(const sliced_model_t* model) {
    printf("Slicing Information:\n");
    printf("Number of layers: %d\n", model->num_layers);
    printf("Layer height: %.3f mm\n", model->params.layer_height);
    printf("Infill density: %.1f%%\n", model->params.infill_density * 100.0f);
    printf("Shell thickness: %.3f mm\n", model->params.shell_thickness);
    printf("Print speed: %.1f mm/s\n", model->params.print_speed);
    printf("Travel speed: %.1f mm/s\n", model->params.travel_speed);
    printf("Nozzle diameter: %.3f mm\n", model->params.nozzle_diameter);
    printf("Filament diameter: %.3f mm\n", model->params.filament_diameter);
} 