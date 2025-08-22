#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "topology_evaluator.h"
#include "PSC_model_inspector.h"


void print_usage(const char* program_name) {
    printf("PSC Model Inspector - 3D Model Analysis Tool\n");
    printf("Usage: %s <input.stl> [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --topology <type>    Analyze mesh topology (connectivity, curvature, features, density, quality, complete)\n");
    printf("  --convex <threshold> <depth>  Perform convex decomposition and visualize with random colors\n");
    printf("                       threshold: 0.0-1.0 (0.8 recommended)\n");
    printf("                       depth: max recursion depth (3-5 recommended)\n");
    printf("  --help               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s model.stl --topology complete\n", program_name);
    printf("  %s model.stl --convex 0.8 4\n", program_name);
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
    topology_analysis_type_t topology_type = TOPO_ANALYSIS_COMPLETE;
    float convex_threshold = 0.8f;
    int convex_max_depth = 4;

    
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
    
    // Convex decomposition visualization
    if (use_convex_decomposition) {
        printf("Starting convex decomposition visualization...\n");
        display_convex_decomposition_tree(stl, convex_threshold, convex_max_depth, PLANE_METHOD_TWO_WORST_PLUS_CENTER, 0);
    }
    
    // Cleanup
    if (topology_eval) free_topology_evaluation(topology_eval);
    stl_free(stl);
    
    printf("\nModel analysis completed successfully!\n");
    
    return 0;
} 