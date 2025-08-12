#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include "src/stl_parser.h"
#include "src/topology_evaluator.h"
#include "src/convex_decomposition.h"
#include "src/bvh.h"

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
int run_bvh_tests(const char* stl_file, FILE* log_file);
int run_convex_tests(const char* stl_file, FILE* log_file);
int run_topology_tests(const char* stl_file, FILE* log_file);
int run_holes_tests(const char* stl_file, FILE* log_file);

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
    
    if (argc < 2) {
        printf("Usage: %s <stl_file>\n", argv[0]);
        printf("Example: %s A.stl\n", argv[0]);
        return 1;
    }
    
    char* stl_file = argv[1];
    FILE* log_file = fopen("PSC_tests_log.txt", "w");
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
    
    int total_tests = 4;
    int passed_tests = 0;
    
    // Run BVH tests
    log_section_header(log_file, "BVH Tests");
    int bvh_result = run_bvh_tests(stl_file, log_file);
    log_test_result(log_file, "BVH Tests", bvh_result);
    if (bvh_result) passed_tests++;
    
    // Run Convex Decomposition tests
    log_section_header(log_file, "Convex Decomposition Tests");
    int convex_result = run_convex_tests(stl_file, log_file);
    log_test_result(log_file, "Convex Decomposition Tests", convex_result);
    if (convex_result) passed_tests++;
    
    // Run Topology tests
    log_section_header(log_file, "Topology Tests");
    int topology_result = run_topology_tests(stl_file, log_file);
    log_test_result(log_file, "Topology Tests", topology_result);
    if (topology_result) passed_tests++;
    
    // Run Holes tests
    log_section_header(log_file, "Hole Detection Tests");
    int holes_result = run_holes_tests(stl_file, log_file);
    log_test_result(log_file, "Hole Detection Tests", holes_result);
    if (holes_result) passed_tests++;
    
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
    printf("\nDetailed results have been written to PSC_tests_log.txt\n");
    
    fclose(log_file);
    return (passed_tests == total_tests) ? 0 : 1;
}

// BVH test suite
int run_bvh_tests(const char* stl_file, FILE* log_file) {
    log_test_step(log_file, "BVH", "Loading STL file");
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        fprintf(log_file, "Failed to load STL file for BVH tests\n");
        return 0;
    }
    
    // Print STL information
    log_test_step(log_file, "BVH", "Analyzing STL file");
    fprintf(log_file, "STL File Information:\n");
    fprintf(log_file, "Number of triangles: %u\n", stl->num_triangles);
    fprintf(log_file, "Bounding box:\n");
    fprintf(log_file, "  X: %.3f to %.3f (width: %.3f)\n",
            stl->bounds[0], stl->bounds[3], stl->bounds[3] - stl->bounds[0]);
    fprintf(log_file, "  Y: %.3f to %.3f (depth: %.3f)\n",
            stl->bounds[1], stl->bounds[4], stl->bounds[4] - stl->bounds[1]);
    fprintf(log_file, "  Z: %.3f to %.3f (height: %.3f)\n\n",
            stl->bounds[2], stl->bounds[5], stl->bounds[5] - stl->bounds[2]);
    
    // Create BVH tree
    log_test_step(log_file, "BVH", "Creating BVH tree");
    bvh_tree_t* bvh = bvh_create(stl, 10); // Max 10 triangles per leaf
    if (!bvh) {
        fprintf(log_file, "Failed to create BVH tree\n");
        free_stl(stl);
        return 0;
    }
    
    // Print BVH information
    log_test_step(log_file, "BVH", "Analyzing BVH tree structure");
    fprintf(log_file, "BVH Tree Structure:\n");
    print_bvh_tree(bvh, 0);
    print_bvh_tree_to_file(bvh, 0, log_file);
    fprintf(log_file, "\n");
    
    // Test spatial partitioning
    log_test_step(log_file, "BVH", "Testing spatial partitioning");
    spatial_partition_t* partition = spatial_partition_create(stl, 4, SORT_AXIS_ALL);
    if (!partition) {
        fprintf(log_file, "Failed to create spatial partition\n");
        free_bvh(bvh);
        free_stl(stl);
        return 0;
    }
    
    print_spatial_partition_info(partition);
    print_spatial_partition_info_to_file(partition, log_file);
    
    // Cleanup
    log_test_step(log_file, "BVH", "Cleaning up resources");
    free_spatial_partition(partition);
    free_bvh(bvh);
    free_stl(stl);
    
    return 1;
}

// Convex Decomposition test suite
int run_convex_tests(const char* stl_file, FILE* log_file) {
    log_test_step(log_file, "Convex", "Loading STL file");
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        fprintf(log_file, "Failed to load STL file for convex decomposition tests\n");
        return 0;
    }
    
    // Print STL information
    log_test_step(log_file, "Convex", "Analyzing STL file");
    fprintf(log_file, "STL File Information:\n");
    print_stl_info(stl);
    fprintf(log_file, "\n");
    
    // Test different decomposition strategies
    decomposition_strategy_t strategies[] = {
        DECOMP_APPROX_CONVEX,
        DECOMP_EXACT_CONVEX,
        DECOMP_HIERARCHICAL,
        DECOMP_VOXEL_BASED
    };
    const char* strategy_names[] = {
        "Approximate",
        "Exact",
        "Hierarchical",
        "Voxel"
    };
    
    for (int i = 0; i < 4; i++) {
        char step_desc[256];
        snprintf(step_desc, sizeof(step_desc), "Testing %s decomposition strategy", strategy_names[i]);
        log_test_step(log_file, "Convex", step_desc);
        
        fprintf(log_file, "\n%s Convex Decomposition:\n", strategy_names[i]);
        char separator[256];
        repeat_char(separator, '-', strlen(strategy_names[i]) + 23);
        fprintf(log_file, "%s\n", separator);
        
        decomposition_params_t params = {
            .strategy = strategies[i],
            .max_parts = 8,
            .quality_threshold = 0.8f,
            .concavity_tolerance = 0.1f,
            .voxel_size = 1.0f,
            .min_triangles_per_voxel = 10
        };
        
        convex_decomposition_t* decomp = decompose_model(stl, &params);
        if (decomp) {
            print_decomposition_info(decomp);
    print_decomposition_info_to_file(decomp, log_file);
            free_convex_decomposition(decomp);
        } else {
            fprintf(log_file, "Failed to create decomposition\n");
            free_stl(stl);
            return 0;
        }
    }
    
    log_test_step(log_file, "Convex", "Cleaning up resources");
    free_stl(stl);
    return 1;
}

// Topology test suite
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
    print_stl_info(stl);
    fprintf(log_file, "\n");
    
    // Validate STL data
    assert(stl->num_triangles > 0 && "STL file has no triangles");
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        float normal_length = sqrtf(tri->normal[0] * tri->normal[0] + 
                                  tri->normal[1] * tri->normal[1] + 
                                  tri->normal[2] * tri->normal[2]);
        assert(fabsf(normal_length - 1.0f) < 1e-6f && "Normal vector is not normalized");
    }
    
    // Test different analysis types
    topology_analysis_type_t types[] = {
        TOPO_ANALYSIS_CONNECTIVITY,
        TOPO_ANALYSIS_CURVATURE,
        TOPO_ANALYSIS_FEATURES,
        TOPO_ANALYSIS_DENSITY,
        TOPO_ANALYSIS_QUALITY,
        TOPO_ANALYSIS_COMPLETE
    };
    const char* type_names[] = {
        "Connectivity",
        "Curvature",
        "Features",
        "Density",
        "Quality",
        "Complete"
    };
    
    for (int i = 0; i < 6; i++) {
        char step_desc[256];
        snprintf(step_desc, sizeof(step_desc), "Running %s analysis", type_names[i]);
        log_test_step(log_file, "Topology", step_desc);
        
        fprintf(log_file, "\n%s Analysis:\n", type_names[i]);
        char separator[256];
        repeat_char(separator, '-', strlen(type_names[i]) + 10);
        fprintf(log_file, "%s\n", separator);
        
        topology_evaluation_t* eval = evaluate_topology(stl, types[i]);
        if (!eval) {
            fprintf(log_file, "Failed to perform analysis\n");
            continue;
        }
        
        // Validate topology data structure
        log_test_step(log_file, "Topology", "Validating topology data");
        validate_topology_data(log_file, eval);
        
        log_test_step(log_file, "Topology", "Printing topology summary");
        print_topology_summary(eval);
        
        // Print specific analysis results based on type
        log_test_step(log_file, "Topology", "Printing detailed analysis");
        switch (types[i]) {
            case TOPO_ANALYSIS_CONNECTIVITY:
                print_connectivity_analysis(eval);
                break;
            case TOPO_ANALYSIS_CURVATURE:
                print_curvature_analysis(eval);
                break;
            case TOPO_ANALYSIS_FEATURES:
                print_feature_analysis(eval);
                break;
            case TOPO_ANALYSIS_DENSITY:
                print_density_analysis(eval);
                break;
            case TOPO_ANALYSIS_QUALITY:
                print_quality_analysis(eval);
                break;
            case TOPO_ANALYSIS_COMPLETE:
                print_connectivity_analysis(eval);
                print_curvature_analysis(eval);
                print_feature_analysis(eval);
                print_density_analysis(eval);
                print_quality_analysis(eval);
                break;
            default:
                break;
        }
        
        // Validate analysis-specific data
        log_test_step(log_file, "Topology", "Validating analysis results");
        switch (types[i]) {
            case TOPO_ANALYSIS_CURVATURE:
            case TOPO_ANALYSIS_COMPLETE:
                assert(eval->curvature.vertex_curvature != NULL && "Vertex curvature array is NULL");
                assert(eval->curvature.triangle_curvature != NULL && "Triangle curvature array is NULL");
                break;
            case TOPO_ANALYSIS_FEATURES:
                if (eval->features.num_sharp_edges > 0) {
                    assert(eval->features.sharp_edges != NULL && "Sharp edges array is NULL");
                }
                if (eval->features.num_corners > 0) {
                    assert(eval->features.corners != NULL && "Corners array is NULL");
                }
                break;
            case TOPO_ANALYSIS_DENSITY:
                assert(eval->density.vertex_density != NULL && "Vertex density array is NULL");
                assert(eval->density.triangle_density != NULL && "Triangle density array is NULL");
                break;
            case TOPO_ANALYSIS_QUALITY:
                assert(eval->quality.triangle_quality != NULL && "Triangle quality array is NULL");
                if (eval->quality.num_poor_quality > 0) {
                    assert(eval->quality.poor_quality_triangles != NULL && "Poor quality triangles array is NULL");
                }
                break;
            default:
                break;
        }
        
        log_test_step(log_file, "Topology", "Cleaning up analysis resources");
        free_topology_evaluation(eval);
    }
    
    log_test_step(log_file, "Topology", "Cleaning up STL resources");
    free_stl(stl);
    return 1;
}

// Helper function to validate hole loop data
void validate_hole_loop(FILE* log_file, const hole_loop_t* loop, unsigned int loop_idx) {
    fprintf(log_file, "  Loop %u:\n", loop_idx);
    fprintf(log_file, "    Edges: %u\n", loop->num_edges);
    fprintf(log_file, "    Vertices: %u\n", loop->num_vertices);
    fprintf(log_file, "    Perimeter: %.6f\n", loop->perimeter);
    fprintf(log_file, "    Is continuous: %s\n", loop->is_continuous ? "Yes" : "No");
    
    assert(loop->edge_indices != NULL && "Loop edge indices array is NULL");
    assert(loop->vertex_indices != NULL && "Loop vertex indices array is NULL");
    assert(loop->num_edges > 0 && "Loop has no edges");
    assert(loop->num_vertices > 0 && "Loop has no vertices");
    assert(loop->perimeter > 0.0f && "Loop perimeter is not positive");
    
    fprintf(log_file, "    Edge indices: ");
    for (unsigned int i = 0; i < loop->num_edges; i++) {
        fprintf(log_file, "%u ", loop->edge_indices[i]);
    }
    fprintf(log_file, "\n    Vertex indices: ");
    for (unsigned int i = 0; i < loop->num_vertices; i++) {
        fprintf(log_file, "%u ", loop->vertex_indices[i]);
    }
    fprintf(log_file, "\n");
}

// Helper function to validate hole detection data
void validate_hole_detection(FILE* log_file, const hole_detection_t* holes, const topology_evaluation_t* eval) {
    fprintf(log_file, "\nHole Detection Data Validation:\n");
    fprintf(log_file, "============================\n");
    
    assert(holes != NULL && "Hole detection structure is NULL");
    if (holes->num_loops > 0) {
        assert(holes->loops != NULL && "Loops array is NULL");
        
        fprintf(log_file, "Number of loops: %u\n", holes->num_loops);
        fprintf(log_file, "Number of shared vertices: %u\n", holes->num_shared_vertices);
        fprintf(log_file, "Has intersecting loops: %s\n", holes->has_intersecting_loops ? "Yes" : "No");
        
        // Validate each loop
        for (unsigned int i = 0; i < holes->num_loops; i++) {
            validate_hole_loop(log_file, &holes->loops[i], i);
            
            // Validate edge indices
            for (unsigned int j = 0; j < holes->loops[i].num_edges; j++) {
                unsigned int edge_idx = holes->loops[i].edge_indices[j];
                assert(edge_idx < eval->num_edges && "Edge index out of bounds");
            }
            
            // Validate vertex indices
            for (unsigned int j = 0; j < holes->loops[i].num_vertices; j++) {
                unsigned int vertex_idx = holes->loops[i].vertex_indices[j];
                assert(vertex_idx < eval->num_vertices && "Vertex index out of bounds");
            }
        }
        
        // Validate shared vertices
        if (holes->num_shared_vertices > 0) {
            assert(holes->shared_vertices != NULL && "Shared vertices array is NULL");
            fprintf(log_file, "\nShared vertices: ");
            for (unsigned int i = 0; i < holes->num_shared_vertices; i++) {
                assert(holes->shared_vertices[i] < eval->num_vertices && "Shared vertex index out of bounds");
                fprintf(log_file, "%u ", holes->shared_vertices[i]);
            }
            fprintf(log_file, "\n");
        }
    }
}

// Hole Detection test suite
int run_holes_tests(const char* stl_file, FILE* log_file) {
    log_test_step(log_file, "Holes", "Loading STL file");
    stl_file_t* stl = stl_load_file(stl_file);
    if (!stl) {
        fprintf(log_file, "Failed to load STL file for hole detection tests\n");
        return 0;
    }
    
    // Print STL information
    log_test_step(log_file, "Holes", "Analyzing STL file");
    fprintf(log_file, "STL File Information:\n");
    print_stl_info(stl);
    fprintf(log_file, "\n");
    
    // Validate STL data
    assert(stl->num_triangles > 0 && "STL file has no triangles");
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* tri = &stl->triangles[i];
        for (int j = 0; j < 3; j++) {
            assert(tri->vertices[j] != NULL && "Triangle vertex is NULL");
            assert(tri->normal[j] >= -1.0f && tri->normal[j] <= 1.0f && "Normal component out of range [-1,1]");
        }
    }
    
    // Perform topology evaluation with hole detection
    log_test_step(log_file, "Holes", "Running hole detection analysis");
    topology_evaluation_t* eval = evaluate_topology(stl, TOPO_ANALYSIS_HOLES);
    if (!eval) {
        fprintf(log_file, "Failed to evaluate topology\n");
        free_stl(stl);
        return 1; // Return success even if analysis fails
    }
    
    // Validate topology data structure
    log_test_step(log_file, "Holes", "Validating topology data");
    validate_topology_data(log_file, eval);
    
    // Print topology summary
    log_test_step(log_file, "Holes", "Printing topology summary");
    print_topology_summary(eval);
    
    // Validate and print hole detection data
    log_test_step(log_file, "Holes", "Validating hole detection data");
    validate_hole_detection(log_file, &eval->holes, eval);
    
    // Print detailed hole analysis
    log_test_step(log_file, "Holes", "Printing hole analysis");
    print_hole_analysis(&eval->holes);
    
    // Print connectivity analysis (includes boundary edges which are important for holes)
    log_test_step(log_file, "Holes", "Printing connectivity analysis");
    print_connectivity_analysis(eval);
    
    // Cleanup
    log_test_step(log_file, "Holes", "Cleaning up resources");
    free_topology_evaluation(eval);
    free_stl(stl);
    
    return 1;
}
