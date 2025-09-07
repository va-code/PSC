#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "topology_evaluator.h"
#include "PSC_model_inspector.h"
#include "mesh_adjacency.h"
#include "coacd_wrapper.h"


void print_usage(const char* program_name) {
    printf("PSC Model Inspector - 3D Model Analysis Tool\n");
    printf("Usage: %s <input.stl> [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --topology <type>    Analyze mesh topology (connectivity, curvature, features, density, quality, complete)\n");
    printf("  --convex <threshold> <depth>  Perform convex decomposition and visualize with random colors\n");
    printf("                       threshold: 0.0-1.0 (0.8 recommended)\n");
    printf("                       depth: max recursion depth (3-5 recommended)\n");
    printf("  --export <output_dir> Export decomposed meshes and adjacency information\n");
    printf("                       (requires --convex option)\n");
    printf("  --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s model.stl --topology complete\n", program_name);
    printf("  %s model.stl --convex 0.8 4\n", program_name);
    printf("  %s model.stl --convex 0.8 4 --export ./output\n", program_name);
}






int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Check for help
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    
    char* input_file = argv[1];
    int use_topology_analysis = 0;
    int use_convex_decomposition = 0;
    int use_export = 0;
    topology_analysis_type_t topology_type = TOPO_ANALYSIS_COMPLETE;
    float convex_threshold = 0.8f;
    int convex_max_depth = 4;
    char* export_output_dir = NULL;

    
    // Parse command line arguments  
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--topology") == 0 && i + 1 < argc) {
            use_topology_analysis = 1;
            char* topo_str = argv[++i];
            if (strcmp(topo_str, "connectivity") == 0) topology_type = TOPO_ANALYSIS_CONNECTIVITY;
            else if (strcmp(topo_str, "curvature") == 0) topology_type = TOPO_ANALYSIS_CURVATURE;
            else if (strcmp(topo_str, "features") == 0) topology_type = TOPO_ANALYSIS_FEATURES;
            else if (strcmp(topo_str, "density") == 0) topology_type = TOPO_ANALYSIS_DENSITY;
            else if (strcmp(topo_str, "quality") == 0) topology_type = TOPO_ANALYSIS_QUALITY;
            else if (strcmp(topo_str, "complete") == 0) topology_type = TOPO_ANALYSIS_COMPLETE;
        }
        else if (strcmp(argv[i], "--convex") == 0 && i + 2 < argc) {
            use_convex_decomposition = 1;
            convex_threshold = atof(argv[++i]);
            convex_max_depth = atoi(argv[++i]);
            printf("Convex decomposition enabled: threshold=%.3f, max_depth=%d\n", convex_threshold, convex_max_depth);
        }
        else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            use_export = 1;
            export_output_dir = argv[++i];
            printf("Export enabled: output directory = %s\n", export_output_dir);
        }
    }
    
    printf("PSC Model Inspector - 3D Model Analysis Tool\n");
    printf("=============================================\n\n");
    
    // Load STL file
    printf("Loading STL file: %s\n", input_file);
    stl_file_t* stl = stl_load_file(input_file);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file\n");
        return 1;
    }
    
    // Print STL information
    stl_print_info(stl);
    printf("\n");

    
    
    // Topology analysis
    topology_evaluation_t* topology_eval = NULL;
    if (use_topology_analysis) {
        printf("Analyzing mesh topology...\n");
        topology_eval = evaluate_topology(stl, topology_type);
        
        if (topology_eval) {
            print_topology_summary(topology_eval);
            print_connectivity_analysis(topology_eval);
            print_curvature_analysis(topology_eval);
            print_feature_analysis(topology_eval);
            print_density_analysis(topology_eval);
            print_quality_analysis(topology_eval);
        } else {
            fprintf(stderr, "Warning: Failed to analyze topology\n");
        }
        printf("\n");
    }
    
    // CoACD convex decomposition visualization
    coacd_result_t* decomposition_result = NULL;
    if (use_convex_decomposition) {
        printf("Starting CoACD convex decomposition visualization...\n");
        display_convex_decomposition_results(stl, convex_threshold, convex_max_depth);
        
        // Store the decomposition results for export if needed
        if (use_export) {
            printf("Creating CoACD decomposition results for export...\n");
            if (coacd_init() == 0) {
                coacd_params_t params = COACD_DEFAULT_PARAMS;
                params.threshold = convex_threshold;
                params.max_convex_hull = convex_max_depth;
                decomposition_result = coacd_decompose(stl, &params);
                if (!decomposition_result) {
                    fprintf(stderr, "Warning: Failed to create CoACD decomposition results for export\n");
                }
            } else {
                fprintf(stderr, "Warning: Failed to initialize CoACD for export\n");
            }
        }
    }
    
    // Export functionality
    if (use_export && decomposition_result && export_output_dir) {
        printf("\nExporting CoACD decomposed meshes...\n");
        
        // Export individual meshes
        for (int i = 0; i < decomposition_result->count; i++) {
            char output_filename[256];
            snprintf(output_filename, sizeof(output_filename), "%s/part_%03d.stl", export_output_dir, i);
            
            if (write_stl_file(decomposition_result->meshes[i], output_filename) == 0) {
                printf("Exported: %s (%d triangles)\n", output_filename, decomposition_result->meshes[i]->num_triangles);
            } else {
                fprintf(stderr, "Warning: Failed to export %s\n", output_filename);
            }
        }
        
        printf("Successfully exported %d CoACD decomposed mesh parts\n", decomposition_result->count);
    }
    
    // Cleanup
    if (topology_eval) free_topology_evaluation(topology_eval);
    if (decomposition_result) coacd_free_result(decomposition_result);
    stl_free(stl);
    
    printf("\nModel analysis completed successfully!\n");
    
    return 0;
} 