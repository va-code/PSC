#include "coacd_wrapper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cstdint>

// Include CoACD C++ headers
#include "coacd.h"

// Global initialization flag
static int coacd_initialized = 0;

extern "C" {

int coacd_init(void) {
    if (coacd_initialized) {
        return 1; // Already initialized
    }
    
    // Set default log level to info
    CoACD_setLogLevel("info");
    
    coacd_initialized = 1;
    return 0;
}

coacd_result_t* coacd_decompose(const stl_file_t* input_mesh, const coacd_params_t* params) {
    if (!input_mesh || !params) {
        return NULL;
    }
    
    // Convert STL mesh to CoACD mesh
    CoACD_Mesh coacd_input;
    coacd_input.vertices_ptr = (double*)malloc(input_mesh->num_triangles * 3 * 3 * sizeof(double));
    coacd_input.triangles_ptr = (int*)malloc(input_mesh->num_triangles * 3 * sizeof(int));
    coacd_input.vertices_count = input_mesh->num_triangles * 3;
    coacd_input.triangles_count = input_mesh->num_triangles;
    
    if (!coacd_input.vertices_ptr || !coacd_input.triangles_ptr) {
        free(coacd_input.vertices_ptr);
        free(coacd_input.triangles_ptr);
        return NULL;
    }
    
    // Copy vertex data
    for (unsigned int i = 0; i < input_mesh->num_triangles; i++) {
        for (int j = 0; j < 3; j++) {
            int vertex_idx = i * 3 + j;
            coacd_input.vertices_ptr[vertex_idx * 3 + 0] = input_mesh->triangles[i].vertices[j][0];
            coacd_input.vertices_ptr[vertex_idx * 3 + 1] = input_mesh->triangles[i].vertices[j][1];
            coacd_input.vertices_ptr[vertex_idx * 3 + 2] = input_mesh->triangles[i].vertices[j][2];
            coacd_input.triangles_ptr[vertex_idx] = vertex_idx;
        }
    }
    
    // Call CoACD
    CoACD_MeshArray result_array = CoACD_run(coacd_input, 
        params->threshold,
        params->max_convex_hull,
        params->preprocess_mode,
        params->prep_resolution,
        params->sample_resolution,
        params->mcts_nodes,
        params->mcts_iteration,
        params->mcts_max_depth,
        params->pca,
        params->merge,
        params->decimate,
        params->max_ch_vertex,
        params->extrude,
        params->extrude_margin,
        params->apx_mode,
        params->seed
    );
    
    // Free input data
    free(coacd_input.vertices_ptr);
    free(coacd_input.triangles_ptr);
    
    if (result_array.meshes_count == 0) {
        return NULL;
    }
    
    // Convert result back to STL format
    coacd_result_t* result = (coacd_result_t*)malloc(sizeof(coacd_result_t));
    if (!result) {
        CoACD_freeMeshArray(result_array);
        return NULL;
    }
    
    result->count = result_array.meshes_count;
    result->meshes = (stl_file_t**)malloc(result->count * sizeof(stl_file_t*));
    
    if (!result->meshes) {
        free(result);
        CoACD_freeMeshArray(result_array);
        return NULL;
    }
    
    // Convert each mesh
    for (int i = 0; i < result->count; i++) {
        CoACD_Mesh* coacd_mesh = &result_array.meshes_ptr[i];
        
        // Create STL file structure
        stl_file_t* stl = (stl_file_t*)malloc(sizeof(stl_file_t));
        if (!stl) {
            // Cleanup already allocated meshes
            for (int j = 0; j < i; j++) {
                stl_free(result->meshes[j]);
            }
            free(result->meshes);
            free(result);
            CoACD_freeMeshArray(result_array);
            return NULL;
        }
        
        stl->num_triangles = coacd_mesh->triangles_count;
        stl->triangles = (stl_triangle_t*)malloc(stl->num_triangles * sizeof(stl_triangle_t));
        
        if (!stl->triangles) {
            free(stl);
            // Cleanup already allocated meshes
            for (int j = 0; j < i; j++) {
                stl_free(result->meshes[j]);
            }
            free(result->meshes);
            free(result);
            CoACD_freeMeshArray(result_array);
            return NULL;
        }
        
        // Copy triangle data
        for (unsigned int j = 0; j < stl->num_triangles; j++) {
            for (int k = 0; k < 3; k++) {
                int vertex_idx = coacd_mesh->triangles_ptr[j * 3 + k];
                stl->triangles[j].vertices[k][0] = coacd_mesh->vertices_ptr[vertex_idx * 3 + 0];
                stl->triangles[j].vertices[k][1] = coacd_mesh->vertices_ptr[vertex_idx * 3 + 1];
                stl->triangles[j].vertices[k][2] = coacd_mesh->vertices_ptr[vertex_idx * 3 + 2];
            }
            
            // Calculate normal (simplified)
            float v1[3] = {
                stl->triangles[j].vertices[1][0] - stl->triangles[j].vertices[0][0],
                stl->triangles[j].vertices[1][1] - stl->triangles[j].vertices[0][1],
                stl->triangles[j].vertices[1][2] - stl->triangles[j].vertices[0][2]
            };
            float v2[3] = {
                stl->triangles[j].vertices[2][0] - stl->triangles[j].vertices[0][0],
                stl->triangles[j].vertices[2][1] - stl->triangles[j].vertices[0][1],
                stl->triangles[j].vertices[2][2] - stl->triangles[j].vertices[0][2]
            };
            
            // Cross product
            stl->triangles[j].normal[0] = v1[1] * v2[2] - v1[2] * v2[1];
            stl->triangles[j].normal[1] = v1[2] * v2[0] - v1[0] * v2[2];
            stl->triangles[j].normal[2] = v1[0] * v2[1] - v1[1] * v2[0];
            
            // Normalize
            float len = sqrt(stl->triangles[j].normal[0] * stl->triangles[j].normal[0] +
                            stl->triangles[j].normal[1] * stl->triangles[j].normal[1] +
                            stl->triangles[j].normal[2] * stl->triangles[j].normal[2]);
            if (len > 0) {
                stl->triangles[j].normal[0] /= len;
                stl->triangles[j].normal[1] /= len;
                stl->triangles[j].normal[2] /= len;
            }
        }
        
        result->meshes[i] = stl;
    }
    
    // Free CoACD result
    CoACD_freeMeshArray(result_array);
    
    return result;
}

void coacd_free_result(coacd_result_t* result) {
    if (!result) return;
    
    if (result->meshes) {
        for (int i = 0; i < result->count; i++) {
            if (result->meshes[i]) {
                stl_free(result->meshes[i]);
            }
        }
        free(result->meshes);
    }
    
    free(result);
}

void coacd_set_log_level(const char* level) {
    if (coacd_initialized) {
        CoACD_setLogLevel(level);
    }
}

} // extern "C"
