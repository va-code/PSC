#include <stdio.h>
#include <stdlib.h>
#include "stl_parser.h"
#include "topology_evaluator.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <stl_file> [analysis_type]\n", argv[0]);
        printf("Analysis types: 0=connectivity, 1=curvature, 2=features, 3=density, 4=quality, 5=complete\n");
        printf("Example: %s test_cube.stl 5\n", argv[0]);
        return 1;
    }
    
    char* filename = argv[1];
    topology_analysis_type_t analysis_type = (argc > 2) ? atoi(argv[2]) : TOPO_ANALYSIS_COMPLETE;
    
    printf("Topology Evaluation Test Program\n");
    printf("================================\n\n");
    
    // Load STL file
    printf("Loading STL file: %s\n", filename);
    stl_file_t* stl = stl_load_file(filename);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file\n");
        return 1;
    }
    
    // Print STL information
    stl_print_info(stl);
    printf("\n");
    
    // Perform topology analysis
    printf("Performing topology analysis...\n");
    topology_evaluation_t* eval = evaluate_topology(stl, analysis_type);
    if (!eval) {
        fprintf(stderr, "Error: Failed to evaluate topology\n");
        stl_free(stl);
        return 1;
    }
    
    // Print analysis results
    printf("Topology Analysis Results\n");
    printf("=========================\n\n");
    
    print_topology_summary(eval);
    print_connectivity_analysis(eval);
    print_curvature_analysis(eval);
    print_feature_analysis(eval);
    print_density_analysis(eval);
    print_quality_analysis(eval);
    
    // Generate and print slicing recommendations
    printf("Slicing Recommendations\n");
    printf("=======================\n");
    slicing_recommendations_t* recs = generate_slicing_recommendations(eval);
    if (recs) {
        print_slicing_recommendations(recs);
        free_slicing_recommendations(recs);
    }
    
    // Test different analysis types
    printf("Testing Different Analysis Types\n");
    printf("================================\n");
    
    const char* analysis_names[] = {"Connectivity", "Curvature", "Features", "Density", "Quality", "Complete"};
    for (int i = 0; i < 6; i++) {
        printf("\n%s Analysis:\n", analysis_names[i]);
        printf("------------\n");
        
        topology_evaluation_t* test_eval = evaluate_topology(stl, i);
        if (test_eval) {
            print_topology_summary(test_eval);
            free_topology_evaluation(test_eval);
        } else {
            printf("Failed to perform analysis\n");
        }
    }
    
    // Cleanup
    free_topology_evaluation(eval);
    stl_free(stl);
    
    printf("\nTopology evaluation test completed successfully!\n");
    return 0;
} 