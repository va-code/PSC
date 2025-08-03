#include "stl_parser.h"
#include "topology_evaluator.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <stl_file>\n", argv[0]);
        return 1;
    }
    
    // Load STL file
    stl_file_t* stl = stl_load_file(argv[1]);
    if (!stl) {
        printf("Failed to load STL file: %s\n", argv[1]);
        return 1;
    }
    
    printf("Loaded STL file: %s\n", argv[1]);
    printf("Number of triangles: %u\n", stl->num_triangles);
    
    // Perform topology evaluation with hole detection
    topology_evaluation_t* eval = evaluate_topology(stl, TOPO_ANALYSIS_HOLES);
    if (!eval) {
        printf("Failed to evaluate topology\n");
        free_stl(stl);
        return 1;
    }
    
    // Print topology summary
    print_topology_summary(eval);
    
    // Print detailed hole analysis
    print_hole_analysis(&eval->holes);
    
    // Print connectivity analysis (includes boundary edges which are important for holes)
    print_connectivity_analysis(eval);
    
    // Cleanup
    free_topology_evaluation(eval);
    free_stl(stl);
    
    return 0;
} 