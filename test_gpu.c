#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "topology_evaluator.h"
#include "gpu_accelerator.h"

void print_gpu_test_usage(const char* program_name) {
    printf("GPU Acceleration Test Program\n");
    printf("Usage: %s <stl_file> [gpu_mode]\n\n", program_name);
    printf("GPU Modes:\n");
    printf("  cpu       - Force CPU-only execution\n");
    printf("  gpu       - Force GPU-only execution\n");
    printf("  auto      - Automatic selection (default)\n");
    printf("  preferred - Try GPU first, fallback to CPU\n\n");
    printf("Example: %s test_cube.stl auto\n", program_name);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_gpu_test_usage(argv[0]);
        return 1;
    }
    
    char* filename = argv[1];
    gpu_mode_t gpu_mode = GPU_MODE_AUTO;
    
    if (argc > 2) {
        char* mode_str = argv[2];
        if (strcmp(mode_str, "cpu") == 0) gpu_mode = GPU_MODE_CPU_ONLY;
        else if (strcmp(mode_str, "gpu") == 0) gpu_mode = GPU_MODE_GPU_ONLY;
        else if (strcmp(mode_str, "auto") == 0) gpu_mode = GPU_MODE_AUTO;
        else if (strcmp(mode_str, "preferred") == 0) gpu_mode = GPU_MODE_GPU_PREFERRED;
        else {
            fprintf(stderr, "Error: Invalid GPU mode '%s'\n", mode_str);
            return 1;
        }
    }
    
    printf("GPU Acceleration Test Program\n");
    printf("============================\n\n");
    
    // Load STL file
    printf("Loading STL file: %s\n", filename);
    stl_file_t* stl = stl_load_file(filename);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file\n");
        return 1;
    }
    
    stl_print_info(stl);
    printf("\n");
    
    // Initialize GPU acceleration
    printf("Initializing GPU acceleration (mode: %d)...\n", gpu_mode);
    gpu_context_t* gpu_ctx = gpu_init(gpu_mode);
    
    if (gpu_ctx && gpu_is_available(gpu_ctx)) {
        printf("✓ GPU acceleration initialized successfully\n");
        gpu_capabilities_t caps = gpu_get_capabilities(gpu_ctx);
        gpu_print_capabilities(&caps);
        
        // Test GPU-accelerated topology analysis
        printf("\nTesting GPU-accelerated topology analysis...\n");
        topology_evaluation_t* eval = evaluate_topology(stl, TOPO_ANALYSIS_COMPLETE);
        if (eval) {
            printf("✓ Topology evaluation created\n");
            
            // Test GPU connectivity analysis
            printf("Testing GPU connectivity analysis...\n");
            if (gpu_analyze_connectivity(stl, eval, gpu_ctx)) {
                printf("✓ GPU connectivity analysis completed\n");
            } else {
                printf("✗ GPU connectivity analysis failed\n");
            }
            
            // Test GPU curvature analysis
            printf("Testing GPU curvature analysis...\n");
            if (gpu_analyze_curvature(stl, eval, gpu_ctx)) {
                printf("✓ GPU curvature analysis completed\n");
            } else {
                printf("✗ GPU curvature analysis failed\n");
            }
            
            // Test GPU triangle sorting
            printf("Testing GPU triangle sorting...\n");
            unsigned int* indices = malloc(stl->num_triangles * sizeof(unsigned int));
            if (indices) {
                if (gpu_sort_triangles_by_axis(stl, indices, stl->num_triangles, 0, gpu_ctx)) {
                    printf("✓ GPU triangle sorting completed\n");
                } else {
                    printf("✗ GPU triangle sorting failed\n");
                }
                free(indices);
            }
            
            // Test GPU contour generation
            printf("Testing GPU contour generation...\n");
            contour_t contours[10];
            unsigned int num_contours = 0;
            float z_height = (stl->bounds[2] + stl->bounds[5]) / 2.0f; // Middle Z
            if (gpu_generate_contours(stl, z_height, contours, &num_contours, gpu_ctx)) {
                printf("✓ GPU contour generation completed (%u contours)\n", num_contours);
            } else {
                printf("✗ GPU contour generation failed\n");
            }
            
            free_topology_evaluation(eval);
        } else {
            printf("✗ Failed to create topology evaluation\n");
        }
        
    } else {
        if (gpu_mode == GPU_MODE_GPU_ONLY) {
            fprintf(stderr, "Error: GPU-only mode requested but GPU not available\n");
            stl_free(stl);
            return 1;
        } else {
            printf("✗ GPU acceleration not available, falling back to CPU\n");
        }
    }
    
    // Cleanup
    if (gpu_ctx) gpu_cleanup(gpu_ctx);
    stl_free(stl);
    
    printf("\nGPU acceleration test completed!\n");
    return 0;
} 