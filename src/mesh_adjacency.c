#include "mesh_adjacency.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

// Tolerance for vertex comparison
#define VERTEX_TOLERANCE 1e-6f

// Check if two vertices are the same (within tolerance)
static int vertices_equal(const float* v1, const float* v2) {
    float dx = v1[0] - v2[0];
    float dy = v1[1] - v2[1];
    float dz = v1[2] - v2[2];
    return (dx * dx + dy * dy + dz * dz) < (VERTEX_TOLERANCE * VERTEX_TOLERANCE);
}

// Check if two meshes are adjacent by finding shared vertices
int check_mesh_adjacency(const stl_file_t* mesh1, const stl_file_t* mesh2, mesh_adjacency_t* adjacency) {
    if (!mesh1 || !mesh2 || !adjacency) {
        return 0;
    }
    
    int shared_vertices = 0;
    int max_shared = mesh1->num_triangles * 3; // Maximum possible shared vertices
    
    // Create arrays to track which vertices from mesh1 are shared
    int* shared_indices = malloc(max_shared * sizeof(int));
    if (!shared_indices) return 0;
    
    // Check each vertex in mesh1 against all vertices in mesh2
    for (unsigned int i = 0; i < mesh1->num_triangles; i++) {
        for (int v1 = 0; v1 < 3; v1++) {
            const float* vertex1 = mesh1->triangles[i].vertices[v1];
            
            for (unsigned int j = 0; j < mesh2->num_triangles; j++) {
                for (int v2 = 0; v2 < 3; v2++) {
                    const float* vertex2 = mesh2->triangles[j].vertices[v2];
                    
                    if (vertices_equal(vertex1, vertex2)) {
                        // Check if this vertex was already counted
                        int already_counted = 0;
                        for (int k = 0; k < shared_vertices; k++) {
                            if (shared_indices[k] == (int)(i * 3 + v1)) {
                                already_counted = 1;
                                break;
                            }
                        }
                        
                        if (!already_counted) {
                            shared_indices[shared_vertices++] = i * 3 + v1;
                        }
                    }
                }
            }
        }
    }
    
    free(shared_indices);
    
    // Meshes are adjacent if they share at least 3 vertices (forming a face)
    if (shared_vertices >= 3) {
        adjacency->shared_vertices = shared_vertices;
        
        // Calculate approximate contact area (shared vertices * average triangle area)
        float avg_triangle_area = 0.0f;
        int total_triangles = mesh1->num_triangles + mesh2->num_triangles;
        
        // Calculate average area from both meshes
        for (unsigned int i = 0; i < mesh1->num_triangles; i++) {
            const stl_triangle_t* tri = &mesh1->triangles[i];
            float area = 0.5f * sqrtf(
                (tri->vertices[1][0] - tri->vertices[0][0]) * (tri->vertices[1][0] - tri->vertices[0][0]) +
                (tri->vertices[1][1] - tri->vertices[0][1]) * (tri->vertices[1][1] - tri->vertices[0][1]) +
                (tri->vertices[1][2] - tri->vertices[0][2]) * (tri->vertices[1][2] - tri->vertices[0][2])
            ) * sqrtf(
                (tri->vertices[2][0] - tri->vertices[0][0]) * (tri->vertices[2][0] - tri->vertices[0][0]) +
                (tri->vertices[2][1] - tri->vertices[0][1]) * (tri->vertices[2][1] - tri->vertices[0][1]) +
                (tri->vertices[2][2] - tri->vertices[0][2]) * (tri->vertices[2][2] - tri->vertices[0][2])
            );
            avg_triangle_area += area;
        }
        
        for (unsigned int i = 0; i < mesh2->num_triangles; i++) {
            const stl_triangle_t* tri = &mesh2->triangles[i];
            float area = 0.5f * sqrtf(
                (tri->vertices[1][0] - tri->vertices[0][0]) * (tri->vertices[1][0] - tri->vertices[0][0]) +
                (tri->vertices[1][1] - tri->vertices[0][1]) * (tri->vertices[1][1] - tri->vertices[0][1]) +
                (tri->vertices[1][2] - tri->vertices[0][2]) * (tri->vertices[1][2] - tri->vertices[0][2])
            ) * sqrtf(
                (tri->vertices[2][0] - tri->vertices[0][0]) * (tri->vertices[2][0] - tri->vertices[0][0]) +
                (tri->vertices[2][1] - tri->vertices[0][1]) * (tri->vertices[2][1] - tri->vertices[0][1]) +
                (tri->vertices[2][2] - tri->vertices[0][2]) * (tri->vertices[2][2] - tri->vertices[0][2])
            );
            avg_triangle_area += area;
        }
        
        avg_triangle_area /= total_triangles;
        adjacency->contact_area = shared_vertices * avg_triangle_area * 0.1f; // Rough estimate
        
        return 1;
    }
    
    adjacency->shared_vertices = 0;
    adjacency->contact_area = 0.0f;
    return 0;
}

// Detect adjacency between decomposed meshes
adjacency_graph_t* detect_mesh_adjacency(mesh_tree_node_t** leaves, int num_leaves) {
    if (!leaves || num_leaves <= 0) {
        return NULL;
    }
    
    adjacency_graph_t* graph = malloc(sizeof(adjacency_graph_t));
    if (!graph) return NULL;
    
    graph->num_meshes = num_leaves;
    graph->num_adjacencies = 0;
    graph->adjacencies = NULL;
    graph->adjacency_counts = calloc(num_leaves, sizeof(int));
    graph->adjacency_lists = malloc(num_leaves * sizeof(int*));
    
    if (!graph->adjacency_counts || !graph->adjacency_lists) {
        free_adjacency_graph(graph);
        return NULL;
    }
    
    // Initialize adjacency lists
    for (int i = 0; i < num_leaves; i++) {
        graph->adjacency_lists[i] = malloc(num_leaves * sizeof(int));
        if (!graph->adjacency_lists[i]) {
            free_adjacency_graph(graph);
            return NULL;
        }
        graph->adjacency_counts[i] = 0;
    }
    
    // Check adjacency between all pairs of meshes
    int max_adjacencies = num_leaves * (num_leaves - 1) / 2;
    graph->adjacencies = malloc(max_adjacencies * sizeof(mesh_adjacency_t));
    if (!graph->adjacencies) {
        free_adjacency_graph(graph);
        return NULL;
    }
    
    for (int i = 0; i < num_leaves; i++) {
        for (int j = i + 1; j < num_leaves; j++) {
            if (leaves[i]->mesh && leaves[j]->mesh) {
                mesh_adjacency_t adjacency;
                adjacency.mesh1_index = i;
                adjacency.mesh2_index = j;
                
                if (check_mesh_adjacency(leaves[i]->mesh, leaves[j]->mesh, &adjacency)) {
                    // Add to adjacencies array
                    graph->adjacencies[graph->num_adjacencies] = adjacency;
                    
                    // Add to adjacency lists
                    graph->adjacency_lists[i][graph->adjacency_counts[i]++] = j;
                    graph->adjacency_lists[j][graph->adjacency_counts[j]++] = i;
                    
                    graph->num_adjacencies++;
                }
            }
        }
    }
    
    // Resize adjacencies array to actual size
    graph->adjacencies = realloc(graph->adjacencies, graph->num_adjacencies * sizeof(mesh_adjacency_t));
    
    return graph;
}

// Create export data structure for decomposed meshes
mesh_export_data_t* create_export_data(const decomposition_tree_t* tree, const char* base_filename) {
    if (!tree || !tree->root || !base_filename) {
        return NULL;
    }
    
    mesh_export_data_t* export_data = malloc(sizeof(mesh_export_data_t));
    if (!export_data) return NULL;
    
    strncpy(export_data->base_filename, base_filename, 255);
    export_data->base_filename[255] = '\0';
    export_data->tree = (decomposition_tree_t*)tree;
    
    // Get leaf nodes
    mesh_tree_node_t* leaves[256];
    int num_leaves = get_leaf_nodes(tree, leaves, 256);
    export_data->num_meshes = num_leaves;
    
    // Allocate mesh info array
    export_data->mesh_info = malloc(num_leaves * sizeof(mesh_export_info_t));
    if (!export_data->mesh_info) {
        free_export_data(export_data);
        return NULL;
    }
    
    // Fill mesh info
    for (int i = 0; i < num_leaves; i++) {
        mesh_export_info_t* info = &export_data->mesh_info[i];
        mesh_tree_node_t* leaf = leaves[i];
        
        info->mesh_index = i;
        info->concavity_score = leaf->concavity_score;
        info->depth = leaf->depth;
        info->num_triangles = leaf->mesh ? leaf->mesh->num_triangles : 0;
        
        if (leaf->mesh) {
            memcpy(info->bounds, leaf->mesh->bounds, 6 * sizeof(float));
        }
        
        // Generate filename
        snprintf(info->filename, 255, "%s_part_%03d.stl", base_filename, i);
    }
    
    // Detect adjacency
    export_data->adjacency = detect_mesh_adjacency(leaves, num_leaves);
    
    return export_data;
}

// Export individual meshes to STL files
int export_individual_meshes(const mesh_export_data_t* export_data, const char* output_dir) {
    if (!export_data || !output_dir) {
        return 0;
    }
    
    // Get leaf nodes
    mesh_tree_node_t* leaves[256];
    int num_leaves = get_leaf_nodes(export_data->tree, leaves, 256);
    
    for (int i = 0; i < num_leaves; i++) {
        if (!leaves[i]->mesh) continue;
        
        // Create full output path
        char output_path[512];
        snprintf(output_path, 511, "%s/%s", output_dir, export_data->mesh_info[i].filename);
        
        // Open output file
        FILE* file = fopen(output_path, "wb");
        if (!file) {
            printf("Warning: Could not create output file: %s\n", output_path);
            continue;
        }
        
        // Write STL header
        char header[80] = "PSC Decomposed Mesh Part";
        snprintf(header + 25, 54, " %03d - Concavity: %.3f", i, export_data->mesh_info[i].concavity_score);
        fwrite(header, 1, 80, file);
        
        // Write triangle count
        uint32_t num_triangles = export_data->mesh_info[i].num_triangles;
        fwrite(&num_triangles, 4, 1, file);
        
        // Write triangles
        for (unsigned int j = 0; j < num_triangles; j++) {
            const stl_triangle_t* tri = &leaves[i]->mesh->triangles[j];
            
            // Write normal
            fwrite(tri->normal, 4, 3, file);
            
            // Write vertices
            fwrite(tri->vertices, 4, 9, file);
            
            // Write attribute byte count (0)
            uint16_t attr = 0;
            fwrite(&attr, 2, 1, file);
        }
        
        fclose(file);
        printf("Exported mesh part %d: %s (%u triangles)\n", i, output_path, num_triangles);
    }
    
    return 1;
}

// Export adjacency information to a text file
int export_adjacency_info(const mesh_export_data_t* export_data, const char* output_dir) {
    if (!export_data || !output_dir || !export_data->adjacency) {
        return 0;
    }
    
    char output_path[512];
    snprintf(output_path, 511, "%s/%s_adjacency.txt", output_dir, export_data->base_filename);
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        printf("Warning: Could not create adjacency file: %s\n", output_path);
        return 0;
    }
    
    fprintf(file, "Mesh Adjacency Information\n");
    fprintf(file, "==========================\n\n");
    fprintf(file, "Total meshes: %d\n", export_data->num_meshes);
    fprintf(file, "Total adjacencies: %d\n\n", export_data->adjacency->num_adjacencies);
    
    // Write adjacency matrix
    fprintf(file, "Adjacency Matrix:\n");
    fprintf(file, "   ");
    for (int i = 0; i < export_data->num_meshes; i++) {
        fprintf(file, "%3d", i);
    }
    fprintf(file, "\n");
    
    for (int i = 0; i < export_data->num_meshes; i++) {
        fprintf(file, "%2d ", i);
        for (int j = 0; j < export_data->num_meshes; j++) {
            int adjacent = 0;
            for (int k = 0; k < export_data->adjacency->adjacency_counts[i]; k++) {
                if (export_data->adjacency->adjacency_lists[i][k] == j) {
                    adjacent = 1;
                    break;
                }
            }
            fprintf(file, "%3s", adjacent ? "X" : ".");
        }
        fprintf(file, "\n");
    }
    
    fprintf(file, "\nDetailed Adjacency Information:\n");
    fprintf(file, "==============================\n\n");
    
    for (int i = 0; i < export_data->adjacency->num_adjacencies; i++) {
        const mesh_adjacency_t* adj = &export_data->adjacency->adjacencies[i];
        fprintf(file, "Mesh %d <-> Mesh %d:\n", adj->mesh1_index, adj->mesh2_index);
        fprintf(file, "  Shared vertices: %d\n", adj->shared_vertices);
        fprintf(file, "  Contact area: %.6f\n\n", adj->contact_area);
    }
    
    fclose(file);
    printf("Exported adjacency information: %s\n", output_path);
    return 1;
}

// Export complete decomposition information to JSON format
int export_decomposition_info_json(const mesh_export_data_t* export_data, const char* output_dir) {
    if (!export_data || !output_dir) {
        return 0;
    }
    
    char output_path[512];
    snprintf(output_path, 511, "%s/%s_decomposition.json", output_dir, export_data->base_filename);
    
    FILE* file = fopen(output_path, "w");
    if (!file) {
        printf("Warning: Could not create JSON file: %s\n", output_path);
        return 0;
    }
    
    fprintf(file, "{\n");
    fprintf(file, "  \"decomposition_info\": {\n");
    fprintf(file, "    \"base_filename\": \"%s\",\n", export_data->base_filename);
    fprintf(file, "    \"num_meshes\": %d,\n", export_data->num_meshes);
    fprintf(file, "    \"tree_depth\": %d,\n", export_data->tree->max_depth_reached);
    fprintf(file, "    \"total_nodes\": %d,\n", export_data->tree->total_nodes);
    fprintf(file, "    \"leaf_nodes\": %d\n", export_data->tree->leaf_nodes);
    fprintf(file, "  },\n");
    
    // Write mesh information
    fprintf(file, "  \"meshes\": [\n");
    for (int i = 0; i < export_data->num_meshes; i++) {
        const mesh_export_info_t* info = &export_data->mesh_info[i];
        fprintf(file, "    {\n");
        fprintf(file, "      \"index\": %d,\n", info->mesh_index);
        fprintf(file, "      \"filename\": \"%s\",\n", info->filename);
        fprintf(file, "      \"concavity_score\": %.6f,\n", info->concavity_score);
        fprintf(file, "      \"depth\": %d,\n", info->depth);
        fprintf(file, "      \"num_triangles\": %d,\n", info->num_triangles);
        fprintf(file, "      \"bounds\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f]\n", 
                info->bounds[0], info->bounds[1], info->bounds[2],
                info->bounds[3], info->bounds[4], info->bounds[5]);
        
        if (i < export_data->num_meshes - 1) {
            fprintf(file, "    },\n");
        } else {
            fprintf(file, "    }\n");
        }
    }
    fprintf(file, "  ],\n");
    
    // Write adjacency information
    if (export_data->adjacency) {
        fprintf(file, "  \"adjacencies\": [\n");
        for (int i = 0; i < export_data->adjacency->num_adjacencies; i++) {
            const mesh_adjacency_t* adj = &export_data->adjacency->adjacencies[i];
            fprintf(file, "    {\n");
            fprintf(file, "      \"mesh1\": %d,\n", adj->mesh1_index);
            fprintf(file, "      \"mesh2\": %d,\n", adj->mesh2_index);
            fprintf(file, "      \"shared_vertices\": %d,\n", adj->shared_vertices);
            fprintf(file, "      \"contact_area\": %.6f\n", adj->contact_area);
            
            if (i < export_data->adjacency->num_adjacencies - 1) {
                fprintf(file, "    },\n");
            } else {
                fprintf(file, "    }\n");
            }
        }
        fprintf(file, "  ]\n");
    } else {
        fprintf(file, "  \"adjacencies\": []\n");
    }
    
    fprintf(file, "}\n");
    
    fclose(file);
    printf("Exported decomposition information: %s\n", output_path);
    return 1;
}

// Free adjacency graph structure
void free_adjacency_graph(adjacency_graph_t* graph) {
    if (!graph) return;
    
    if (graph->adjacencies) free(graph->adjacencies);
    if (graph->adjacency_counts) free(graph->adjacency_counts);
    
    if (graph->adjacency_lists) {
        for (int i = 0; i < graph->num_meshes; i++) {
            if (graph->adjacency_lists[i]) free(graph->adjacency_lists[i]);
        }
        free(graph->adjacency_lists);
    }
    
    free(graph);
}

// Free export data structure
void free_export_data(mesh_export_data_t* export_data) {
    if (!export_data) return;
    
    if (export_data->mesh_info) free(export_data->mesh_info);
    if (export_data->adjacency) free_adjacency_graph(export_data->adjacency);
    
    free(export_data);
}
