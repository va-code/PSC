#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "../src/stl_parser.h"
#include "../src/topology_evaluator.h"
#include "../src/PSC_model_inspector.h"

// Helper function to validate vertex data
void validate_vertex_data(FILE* log_file, const topology_vertex_t* vertex, unsigned int vertex_idx) {
    fprintf(log_file, "  Vertex %u:\n", vertex_idx);
    fprintf(log_file, "    Position: (%.3f, %.3f, %.3f)\n", 
            vertex->position[0], vertex->position[1], vertex->position[2]);
    fprintf(log_file, "    Connections: %u/%u\n", vertex->num_connections, vertex->capacity);
    fprintf(log_file, "    Connected vertices: ");
    
    assert(vertex->connected_vertices != NULL && "Vertex connected_vertices array is NULL");
    assert(vertex->num_connections <= vertex->capacity && "Number of connections exceeds capacity");
    
    for (unsigned int i = 0; i < vertex->num_connections; i++) {
        fprintf(log_file, "%u ", vertex->connected_vertices[i]);
    }
    fprintf(log_file, "\n    Valence: %u\n", vertex->valence);
    fprintf(log_file, "    Curvature: %.6f\n", vertex->curvature);
}

// Helper function to validate edge data
void validate_edge_data(FILE* log_file, const topology_edge_t* edge, unsigned int edge_idx) {
    fprintf(log_file, "  Edge %u:\n", edge_idx);
    fprintf(log_file, "    Vertices: %u -> %u\n", edge->vertex1, edge->vertex2);
    fprintf(log_file, "    Triangles: %u -> %d\n", edge->triangle1, edge->triangle2);
    fprintf(log_file, "    Length: %.6f\n", edge->length);
    fprintf(log_file, "    Dihedral angle: %.6f\n", edge->dihedral_angle);
    fprintf(log_file, "    Is boundary: %s\n", edge->is_boundary ? "Yes" : "No");
    
    assert(edge->length >= 0.0f && "Edge length is negative");
    assert(edge->dihedral_angle >= 0.0f && "Dihedral angle is negative");
}

// Helper function to validate triangle data
void validate_triangle_data(FILE* log_file, const topology_triangle_t* triangle, unsigned int tri_idx) {
    fprintf(log_file, "  Triangle %u:\n", tri_idx);
    fprintf(log_file, "    Vertices: %u, %u, %u\n", 
            triangle->vertices[0], triangle->vertices[1], triangle->vertices[2]);
    fprintf(log_file, "    Edges: %u, %u, %u\n", 
            triangle->edges[0], triangle->edges[1], triangle->edges[2]);
    fprintf(log_file, "    Normal: (%.3f, %.3f, %.3f)\n", 
            triangle->normal[0], triangle->normal[1], triangle->normal[2]);
    fprintf(log_file, "    Area: %.6f\n", triangle->area);
    fprintf(log_file, "    Curvature: %.6f\n", triangle->curvature);
    fprintf(log_file, "    Aspect ratio: %.6f\n", triangle->aspect_ratio);
    
    assert(triangle->area >= 0.0f && "Triangle area is negative");
    assert(triangle->aspect_ratio >= 0.0f && triangle->aspect_ratio <= 1.0f && "Aspect ratio out of range [0,1]");
}

// Helper function to validate topology evaluation data
void validate_topology_data(FILE* log_file, const topology_evaluation_t* eval) {
    assert(eval != NULL && "Topology evaluation is NULL");
    assert(eval->vertices != NULL && "Vertex array is NULL");
    assert(eval->edges != NULL && "Edge array is NULL");
    assert(eval->triangles != NULL && "Triangle array is NULL");
    
    fprintf(log_file, "\nTopology Data Validation:\n");
    fprintf(log_file, "========================\n");
    
    // Validate basic counts
    fprintf(log_file, "Counts:\n");
    fprintf(log_file, "  Vertices: %u\n", eval->num_vertices);
    fprintf(log_file, "  Edges: %u\n", eval->num_edges);
    fprintf(log_file, "  Triangles: %u\n", eval->num_triangles);
    fprintf(log_file, "  Boundary edges: %u\n", eval->num_boundary_edges);
    fprintf(log_file, "  Non-manifold vertices: %u\n", eval->num_non_manifold_vertices);
    fprintf(log_file, "  Isolated vertices: %u\n", eval->num_isolated_vertices);
    
    // Validate vertex data
    fprintf(log_file, "\nVertex Data:\n");
    for (unsigned int i = 0; i < eval->num_vertices; i++) {
        validate_vertex_data(log_file, &eval->vertices[i], i);
    }
    
    // Validate edge data
    fprintf(log_file, "\nEdge Data:\n");
    for (unsigned int i = 0; i < eval->num_edges; i++) {
        validate_edge_data(log_file, &eval->edges[i], i);
    }
    
    // Validate triangle data
    fprintf(log_file, "\nTriangle Data:\n");
    for (unsigned int i = 0; i < eval->num_triangles; i++) {
        validate_triangle_data(log_file, &eval->triangles[i], i);
    }
    
    // Validate connectivity
    fprintf(log_file, "\nConnectivity Validation:\n");
    for (unsigned int i = 0; i < eval->num_edges; i++) {
        const topology_edge_t* edge = &eval->edges[i];
        assert(edge->vertex1 < eval->num_vertices && "Edge vertex1 index out of bounds");
        assert(edge->vertex2 < eval->num_vertices && "Edge vertex2 index out of bounds");
        assert(edge->triangle1 < eval->num_triangles && "Edge triangle1 index out of bounds");
        if (!edge->is_boundary) {
            assert(edge->triangle2 < eval->num_triangles && "Edge triangle2 index out of bounds");
        } else {
            assert(edge->triangle2 == UINT_MAX && "Boundary edge triangle2 should be UINT_MAX");
        }
    }
    
    fprintf(log_file, "  All edge indices are valid\n");
    
    // Validate metrics
    fprintf(log_file, "\nMetrics:\n");
    fprintf(log_file, "  Connectivity score: %.6f\n", eval->connectivity_score);
    fprintf(log_file, "  Complexity score: %.6f\n", eval->complexity_score);
    fprintf(log_file, "  Feature richness: %.6f\n", eval->feature_richness);
    
    assert(eval->connectivity_score >= 0.0f && eval->connectivity_score <= 1.0f && "Connectivity score out of range [0,1]");
    assert(eval->complexity_score >= 0.0f && "Complexity score is negative");
    assert(eval->feature_richness >= 0.0f && "Feature richness is negative");
}

// Helper function to repeat a character n times
void repeat_char(char* buffer, char c, int n) {
    for (int i = 0; i < n; i++) {
        buffer[i] = c;
    }
    buffer[n] = '\0';
}

// Function declarations for test suites
int run_topology_tests(const char* stl_file, FILE* log_file);
void run_model_inspector(const char* stl_file);

// Helper function to get current timestamp
void get_timestamp(char* buffer, size_t size) {
    time_t now;
    struct tm* tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Helper function to log section headers
void log_section_header(FILE* log_file, const char* section_name) {
    char timestamp[26];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char separator[81];
    repeat_char(separator, '=', 80);
    fprintf(log_file, "\n%s\n", separator);
    fprintf(log_file, "TEST SECTION: %s (Started at %s)\n", section_name, timestamp);
    repeat_char(separator, '=', 80);
    fprintf(log_file, "%s\n\n", separator);
    fflush(log_file);
}

// Helper function to log test steps
void log_test_step(FILE* log_file, const char* test_name, const char* step_description) {
    char timestamp[26];
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(log_file, "[%s] %s: %s\n", timestamp, test_name, step_description);
    fflush(log_file);
}

// Helper function to log test results
void log_test_result(FILE* log_file, const char* test_name, int success) {
    char timestamp[26];
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(log_file, "\nTEST RESULT: %s - %s (at %s)\n", 
            test_name, success ? "PASSED" : "FAILED", timestamp);
    fflush(log_file);
}

int main(int argc, char* argv[]) {
    char separator[81];  // Used for formatting output
    int show_inspector = 1;  // Default: show inspector after tests
    
    if (argc < 2) {
        printf("Usage: %s <stl_file> [--no-inspector]\n", argv[0]);
        printf("Example: %s A.stl\n", argv[0]);
        printf("Options:\n");
        printf("  --no-inspector    Run tests only, skip model inspector\n");
        return 1;
    }
    
    // Check for --no-inspector flag
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-inspector") == 0) {
            show_inspector = 0;
            break;
        }
    }
    
    char* stl_file = argv[1];
    FILE* log_file = fopen("PSC_model_inspector_log.txt", "w");
    if (!log_file) {
        printf("Error: Could not open log file for writing\n");
        return 1;
    }
    
    // Write test header
    char timestamp[26];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_file, "PSC Test Suite Execution\n");
    fprintf(log_file, "Started at: %s\n", timestamp);
    fprintf(log_file, "Test file: %s\n\n", stl_file);
    
    int total_tests = 1;
    int passed_tests = 0;
    
    // Run Topology tests only
    log_section_header(log_file, "Topology Tests");
    int topology_result = run_topology_tests(stl_file, log_file);
    log_test_result(log_file, "Topology Tests", topology_result);
    if (topology_result) passed_tests++;
    
    // Write summary
    get_timestamp(timestamp, sizeof(timestamp));
    repeat_char(separator, '=', 80);
    fprintf(log_file, "\n%s\n", separator);
    fprintf(log_file, "TEST SUMMARY (Completed at %s)\n", timestamp);
    fprintf(log_file, "Total Tests: %d\n", total_tests);
    fprintf(log_file, "Passed: %d\n", passed_tests);
    fprintf(log_file, "Failed: %d\n", total_tests - passed_tests);
    fprintf(log_file, "Success Rate: %.1f%%\n", (float)passed_tests / total_tests * 100);
    repeat_char(separator, '=', 80);
    fprintf(log_file, "%s\n", separator);
    
    // Also print summary to console
    printf("\nTest Summary:\n");
    printf("Total Tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    printf("Success Rate: %.1f%%\n", (float)passed_tests / total_tests * 100);
    printf("\nDetailed results have been written to PSC_model_inspector_log.txt\n");
    
    fclose(log_file);
    
    // Run model inspector after all tests are complete (if requested)
    if (show_inspector) {
        printf("\nAll tests completed. Starting model inspector...\n");
        run_model_inspector(stl_file);
    } else {
        printf("\nAll tests completed. Skipping model inspector.\n");
    }
    
    return (passed_tests == total_tests) ? 0 : 1;
}

// Simplified topology test function
int run_topology_tests(const char* stl_file, FILE* log_file) {
    log_test_step(log_file, "Topology", "Loading STL file");
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        fprintf(log_file, "Failed to load STL file for topology tests\n");
        return 0;
    }
    
    // Print STL information
    log_test_step(log_file, "Topology", "Analyzing STL file");
    fprintf(log_file, "STL File Information:\n");
    fprintf(log_file, "Number of triangles: %u\n", stl->num_triangles);
    fprintf(log_file, "Bounding box:\n");
    fprintf(log_file, "  X: %.3f to %.3f (width: %.3f)\n",
            stl->bounds[0], stl->bounds[3], stl->bounds[3] - stl->bounds[0]);
    fprintf(log_file, "  Y: %.3f to %.3f (depth: %.3f)\n",
            stl->bounds[1], stl->bounds[4], stl->bounds[4] - stl->bounds[1]);
    fprintf(log_file, "  Z: %.3f to %.3f (height: %.3f)\n\n",
            stl->bounds[2], stl->bounds[5], stl->bounds[5] - stl->bounds[2]);
            
    // Perform topology evaluation
    log_test_step(log_file, "Topology", "Running topology analysis");
    topology_evaluation_t* eval = evaluate_topology(stl, TOPO_ANALYSIS_COMPLETE);
    if (!eval) {
        fprintf(log_file, "Failed to evaluate topology\n");
        free_stl(stl);
        return 0;
    }
    
    // Validate topology data structure
    log_test_step(log_file, "Topology", "Validating topology data");
    validate_topology_data(log_file, eval);
    
    // Print topology summary
    log_test_step(log_file, "Topology", "Printing topology summary");
    print_topology_summary(eval);
    
    // Print detailed analyses
    log_test_step(log_file, "Topology", "Printing detailed analyses");
    print_connectivity_analysis(eval);
    print_curvature_analysis(eval);
    print_feature_analysis(eval);
    print_density_analysis(eval);
    print_quality_analysis(eval);
    
    // Cleanup
    log_test_step(log_file, "Topology", "Cleaning up resources");
    free_topology_evaluation(eval);
    free_stl(stl);
    
    return 1;
}







// Model inspector function to run after all tests
void run_model_inspector(const char* stl_file) {
    printf("Loading STL file for visualization...\n");
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        printf("Failed to load STL file for model inspector\n");
        return;
    }
    
    // Initialize and show STL viewer
    printf("Initializing model inspector...\n");
    stl_viewer_t* viewer = viewer_init(800, 600);
    if (!viewer) {
        printf("Failed to initialize model inspector\n");
        free_stl(stl);
        return;
    }
    
    if (!viewer_load_stl(viewer, stl)) {
        printf("Failed to load STL into viewer\n");
        viewer_cleanup(viewer);
        free_stl(stl);
        return;
    }
    
    printf("Model inspector ready. Use mouse to rotate, ESC to exit.\n");
    printf("Controls:\n");
    printf("  - Left mouse button + drag: Rotate view\n");
    printf("  - ESC: Exit inspector\n");
    
    // Main display loop
    while (!glfwWindowShouldClose(viewer->window)) {
        viewer_display(viewer);
        glfwSwapBuffers(viewer->window);
        glfwPollEvents();
        
        // Check for ESC key
        if (glfwGetKey(viewer->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(viewer->window, GLFW_TRUE);
        }
    }
    
    // Cleanup
    viewer_cleanup(viewer);
    free_stl(stl);
    printf("Model inspector closed.\n");
}


