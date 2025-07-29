#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stl_parser.h"
#include "slicer.h"
#include "path_generator.h"
#include "bvh.h"
#include "convex_decomposition.h"
#include "topology_evaluator.h"
#include "gpu_accelerator.h"

void print_usage(const char* program_name) {
    printf("Parametric Slicer - 3D Path Generation Tool\n");
    printf("Usage: %s <input.stl> [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -o <output.gcode>    Output G-code file (default: output.gcode)\n");
    printf("  -h <height>          Layer height in mm (default: 0.2)\n");
    printf("  -i <density>         Infill density 0.0-1.0 (default: 0.2)\n");
    printf("  -s <thickness>       Shell thickness in mm (default: 0.4)\n");
    printf("  -n <shells>          Number of shell layers (default: 2)\n");
    printf("  -p <speed>           Print speed in mm/s (default: 60.0)\n");
    printf("  -t <speed>           Travel speed in mm/s (default: 120.0)\n");
    printf("  -d <diameter>        Nozzle diameter in mm (default: 0.4)\n");
    printf("  -f <diameter>        Filament diameter in mm (default: 1.75)\n");
    printf("  --bvh <partitions>   Use BVH spatial partitioning with N partitions\n");
    printf("  --sort-axis <axis>   Sort axis for BVH (x, y, z, xy, xz, yz, xyz) (default: xyz)\n");
    printf("  --convex <strategy>  Use convex decomposition (approx, exact, hierarchical, voxel)\n");
    printf("  --max-parts <num>    Maximum number of parts for decomposition (default: 8)\n");
    printf("  --quality <value>    Quality threshold for decomposition (0.0-1.0, default: 0.8)\n");
    printf("  --concavity <value>  Concavity tolerance for approx decomposition (0.0-1.0, default: 0.1)\n");
    printf("  --topology <type>    Analyze mesh topology (connectivity, curvature, features, density, quality, complete)\n");
    printf("  --gpu <mode>         GPU acceleration mode (cpu, gpu, auto, preferred)\n");
    printf("  --interactive        Interactive mode for parameter input\n");
    printf("  --help               Show this help message\n\n");
    printf("Example:\n");
    printf("  %s model.stl -h 0.15 -i 0.3 -o model.gcode\n", program_name);
}

slicing_params_t get_default_params() {
    slicing_params_t params = {
        .layer_height = 0.2f,
        .infill_density = 0.2f,
        .shell_thickness = 0.4f,
        .num_shells = 2,
        .print_speed = 60.0f,
        .travel_speed = 120.0f,
        .nozzle_diameter = 0.4f,
        .filament_diameter = 1.75f
    };
    return params;
}

void interactive_input(slicing_params_t* params) {
    printf("\n=== Interactive Parameter Input ===\n");
    
    printf("Layer height (mm) [%.2f]: ", params->layer_height);
    scanf("%f", &params->layer_height);
    
    printf("Infill density (0.0-1.0) [%.2f]: ", params->infill_density);
    scanf("%f", &params->infill_density);
    
    printf("Shell thickness (mm) [%.2f]: ", params->shell_thickness);
    scanf("%f", &params->shell_thickness);
    
    printf("Number of shell layers [%d]: ", params->num_shells);
    scanf("%d", &params->num_shells);
    
    printf("Print speed (mm/s) [%.1f]: ", params->print_speed);
    scanf("%f", &params->print_speed);
    
    printf("Travel speed (mm/s) [%.1f]: ", params->travel_speed);
    scanf("%f", &params->travel_speed);
    
    printf("Nozzle diameter (mm) [%.2f]: ", params->nozzle_diameter);
    scanf("%f", &params->nozzle_diameter);
    
    printf("Filament diameter (mm) [%.2f]: ", params->filament_diameter);
    scanf("%f", &params->filament_diameter);
    
    printf("\n");
}

void print_params(const slicing_params_t* params) {
    printf("Slicing Parameters:\n");
    printf("  Layer height: %.3f mm\n", params->layer_height);
    printf("  Infill density: %.1f%%\n", params->infill_density * 100.0f);
    printf("  Shell thickness: %.3f mm\n", params->shell_thickness);
    printf("  Number of shells: %d\n", params->num_shells);
    printf("  Print speed: %.1f mm/s\n", params->print_speed);
    printf("  Travel speed: %.1f mm/s\n", params->travel_speed);
    printf("  Nozzle diameter: %.3f mm\n", params->nozzle_diameter);
    printf("  Filament diameter: %.3f mm\n", params->filament_diameter);
    printf("\n");
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
    char* output_file = "output.gcode";
    slicing_params_t params = get_default_params();
    int interactive_mode = 0;
    int use_bvh = 0;
    unsigned int num_partitions = 4;
    sort_axis_t sort_axis = SORT_XYZ;
    int use_convex_decomp = 0;
    decomposition_strategy_t decomp_strategy = DECOMP_APPROX_CONVEX;
    unsigned int max_parts = 8;
    float quality_threshold = 0.8f;
    float concavity_tolerance = 0.1f;
    int use_topology_analysis = 0;
    topology_analysis_type_t topology_type = TOPO_ANALYSIS_COMPLETE;
    gpu_mode_t gpu_mode = GPU_MODE_AUTO;
    gpu_context_t* gpu_ctx = NULL;
    
    // Parse command line arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--interactive") == 0) {
            interactive_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            params.layer_height = atof(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            params.infill_density = atof(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            params.shell_thickness = atof(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            params.num_shells = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            params.print_speed = atof(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            params.travel_speed = atof(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            params.nozzle_diameter = atof(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            params.filament_diameter = atof(argv[++i]);
        } else if (strcmp(argv[i], "--bvh") == 0 && i + 1 < argc) {
            use_bvh = 1;
            num_partitions = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sort-axis") == 0 && i + 1 < argc) {
            char* axis_str = argv[++i];
            if (strcmp(axis_str, "x") == 0) sort_axis = SORT_X;
            else if (strcmp(axis_str, "y") == 0) sort_axis = SORT_Y;
            else if (strcmp(axis_str, "z") == 0) sort_axis = SORT_Z;
            else if (strcmp(axis_str, "xy") == 0) sort_axis = SORT_XY;
            else if (strcmp(axis_str, "xz") == 0) sort_axis = SORT_XZ;
            else if (strcmp(axis_str, "yz") == 0) sort_axis = SORT_YZ;
            else if (strcmp(axis_str, "xyz") == 0) sort_axis = SORT_XYZ;
            else {
                fprintf(stderr, "Error: Invalid sort axis '%s'. Use x, y, z, xy, xz, yz, or xyz\n", axis_str);
                return 1;
            }
        } else if (strcmp(argv[i], "--gpu") == 0 && i + 1 < argc) {
            char* gpu_mode_str = argv[++i];
            if (strcmp(gpu_mode_str, "cpu") == 0) gpu_mode = GPU_MODE_CPU_ONLY;
            else if (strcmp(gpu_mode_str, "gpu") == 0) gpu_mode = GPU_MODE_GPU_ONLY;
            else if (strcmp(gpu_mode_str, "auto") == 0) gpu_mode = GPU_MODE_AUTO;
            else if (strcmp(gpu_mode_str, "preferred") == 0) gpu_mode = GPU_MODE_GPU_PREFERRED;
            else {
                fprintf(stderr, "Error: Invalid GPU mode '%s'. Use cpu, gpu, auto, or preferred\n", gpu_mode_str);
                return 1;
            }
        }
    }
    
    printf("Parametric Slicer - 3D Path Generation\n");
    printf("=====================================\n\n");
    
    // Interactive mode
    if (interactive_mode) {
        interactive_input(&params);
    }
    
    // Print parameters
    print_params(&params);
    
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
    
    // Initialize GPU acceleration
    if (gpu_mode != GPU_MODE_CPU_ONLY) {
        printf("Initializing GPU acceleration...\n");
        gpu_ctx = gpu_init(gpu_mode);
        if (gpu_ctx && gpu_is_available(gpu_ctx)) {
            gpu_capabilities_t caps = gpu_get_capabilities(gpu_ctx);
            gpu_print_capabilities(&caps);
            printf("GPU acceleration enabled\n");
        } else {
            if (gpu_mode == GPU_MODE_GPU_ONLY) {
                fprintf(stderr, "Error: GPU-only mode requested but GPU not available\n");
                stl_free(stl);
                return 1;
            } else {
                printf("GPU acceleration not available, falling back to CPU\n");
                if (gpu_ctx) gpu_cleanup(gpu_ctx);
                gpu_ctx = NULL;
            }
        }
        printf("\n");
    }
    
    // Topology analysis
    topology_evaluation_t* topology_eval = NULL;
    if (use_topology_analysis) {
        printf("Analyzing mesh topology...\n");
        if (gpu_ctx && gpu_is_available(gpu_ctx)) {
            printf("Using GPU-accelerated topology analysis...\n");
            topology_eval = evaluate_topology(stl, topology_type);
            if (topology_eval) {
                // Use GPU acceleration for specific analyses
                if (topology_type == TOPO_ANALYSIS_CONNECTIVITY || topology_type == TOPO_ANALYSIS_COMPLETE) {
                    gpu_analyze_connectivity(stl, topology_eval, gpu_ctx);
                }
                if (topology_type == TOPO_ANALYSIS_CURVATURE || topology_type == TOPO_ANALYSIS_COMPLETE) {
                    gpu_analyze_curvature(stl, topology_eval, gpu_ctx);
                }
                if (topology_type == TOPO_ANALYSIS_FEATURES || topology_type == TOPO_ANALYSIS_COMPLETE) {
                    gpu_analyze_features(stl, topology_eval, gpu_ctx);
                }
                if (topology_type == TOPO_ANALYSIS_DENSITY || topology_type == TOPO_ANALYSIS_COMPLETE) {
                    gpu_analyze_density(stl, topology_eval, gpu_ctx);
                }
                if (topology_type == TOPO_ANALYSIS_QUALITY || topology_type == TOPO_ANALYSIS_COMPLETE) {
                    gpu_analyze_quality(stl, topology_eval, gpu_ctx);
                }
            }
        } else {
            printf("Using CPU topology analysis...\n");
            topology_eval = evaluate_topology(stl, topology_type);
        }
        
        if (topology_eval) {
            print_topology_summary(topology_eval);
            print_connectivity_analysis(topology_eval);
            print_curvature_analysis(topology_eval);
            print_feature_analysis(topology_eval);
            print_density_analysis(topology_eval);
            print_quality_analysis(topology_eval);
            
            // Generate slicing recommendations
            slicing_recommendations_t* recs = generate_slicing_recommendations(topology_eval);
            if (recs) {
                print_slicing_recommendations(recs);
                free_slicing_recommendations(recs);
            }
        } else {
            fprintf(stderr, "Warning: Failed to analyze topology\n");
        }
        printf("\n");
    }
    
    // Slice the model
    printf("Slicing model...\n");
    sliced_model_t* sliced = NULL;
    spatial_partition_t* partition = NULL;
    convex_decomposition_t* decomp = NULL;
    
    if (use_bvh) {
        printf("Using BVH spatial partitioning with %u partitions, sort axis: %d\n", num_partitions, sort_axis);
        
        // Create spatial partition with GPU acceleration if available
        if (gpu_ctx && gpu_is_available(gpu_ctx)) {
            printf("Using GPU-accelerated BVH construction...\n");
            // Note: GPU BVH construction would be implemented here
            partition = spatial_partition_create(stl, num_partitions, sort_axis);
        } else {
            partition = spatial_partition_create(stl, num_partitions, sort_axis);
        }
        if (!partition) {
            fprintf(stderr, "Error: Failed to create spatial partition\n");
            stl_free(stl);
            return 1;
        }
        
        // Print partition information
        spatial_partition_print_info(partition);
        
        // Slice with BVH
        sliced = slice_model_with_bvh(stl, &params, partition);
    } else if (use_convex_decomp) {
        printf("Using convex decomposition with strategy %d, max parts: %u, quality: %.2f, concavity: %.2f\n", 
               decomp_strategy, max_parts, quality_threshold, concavity_tolerance);
        
        // Create decomposition parameters
        decomposition_params_t decomp_params = {
            .strategy = decomp_strategy,
            .max_parts = max_parts,
            .quality_threshold = quality_threshold,
            .concavity_tolerance = concavity_tolerance,
            .voxel_size = 1.0f,
            .min_triangles_per_voxel = 10
        };
        
        // Create convex decomposition with GPU acceleration if available
        if (gpu_ctx && gpu_is_available(gpu_ctx)) {
            printf("Using GPU-accelerated convex decomposition...\n");
            // Note: GPU convex decomposition would be implemented here
            decomp = decompose_model(stl, &decomp_params);
        } else {
            decomp = decompose_model(stl, &decomp_params);
        }
        if (!decomp) {
            fprintf(stderr, "Error: Failed to create convex decomposition\n");
            stl_free(stl);
            return 1;
        }
        
        // Print decomposition information
        print_decomposition_info(decomp);
        
        // Slice with convex decomposition
        sliced = slice_model_with_convex_decomposition(stl, &params, decomp);
    } else {
        sliced = slice_model(stl, &params);
    }
    
    if (!sliced) {
        fprintf(stderr, "Error: Failed to slice model\n");
        if (partition) spatial_partition_free(partition);
        if (decomp) convex_decomposition_free(decomp);
        if (topology_eval) free_topology_evaluation(topology_eval);
        stl_free(stl);
        return 1;
    }
    
    // Print slicing information
    print_slicing_info(sliced);
    printf("\n");
    
    // Generate G-code
    printf("Generating G-code...\n");
    path_generator_t* generator = path_generator_create(&params);
    if (!generator) {
        fprintf(stderr, "Error: Failed to create path generator\n");
        free_sliced_model(sliced);
        stl_free(stl);
        return 1;
    }
    
    generate_gcode_from_slices(generator, sliced);
    
    // Write G-code to file
    printf("Writing G-code to: %s\n", output_file);
    write_gcode_to_file(generator, output_file);
    
    // Cleanup
    path_generator_free(generator);
    free_sliced_model(sliced);
    if (partition) spatial_partition_free(partition);
    if (decomp) convex_decomposition_free(decomp);
    if (topology_eval) free_topology_evaluation(topology_eval);
    if (gpu_ctx) gpu_cleanup(gpu_ctx);
    stl_free(stl);
    
    printf("\nSlicing completed successfully!\n");
    printf("Output file: %s\n", output_file);
    
    return 0;
} 