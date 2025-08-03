#include <stdio.h>
#include <stdlib.h>
#include "stl_parser.h"
#include "bvh.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <stl_file> [num_partitions] [sort_axis]\n", argv[0]);
        printf("Example: %s test_cube.stl 4 xyz\n", argv[0]);
        return 1;
    }
    
    char* filename = argv[1];
    unsigned int num_partitions = (argc > 2) ? atoi(argv[2]) : 4;
    sort_axis_t sort_axis = (argc > 3) ? atoi(argv[3]) : SORT_XYZ;
    
    printf("BVH Test Program\n");
    printf("================\n\n");
    
    // Load STL file
    printf("Loading STL file: %s\n", filename);
    stl_file_t* stl = stl_load_file(filename);
    if (!stl) {
        fprintf(stderr, "Error: Failed to load STL file\n");
        return 1;
    }
    
    // Print STL information
    print_stl_info(stl);
    printf("\n");
    
    // Create BVH tree
    printf("Creating BVH tree...\n");
    bvh_tree_t* bvh = bvh_create(stl, 10);
    if (!bvh) {
        fprintf(stderr, "Error: Failed to create BVH tree\n");
        free_stl(stl);
        return 1;
    }
    
    // Print BVH tree structure
    printf("BVH Tree Structure:\n");
    print_bvh_tree(bvh, 0);
    printf("\n");
    
    // Create spatial partition
    printf("Creating spatial partition with %u partitions, sort axis: %d\n", num_partitions, sort_axis);
    spatial_partition_t* partition = spatial_partition_create(stl, num_partitions, sort_axis);
    if (!partition) {
        fprintf(stderr, "Error: Failed to create spatial partition\n");
        free_bvh(bvh);
        free_stl(stl);
        return 1;
    }
    
    // Print partition information
    print_spatial_partition_info(partition);
    printf("\n");
    
    // Print triangle distribution across partitions
    printf("Triangle distribution across partitions:\n");
    unsigned int* partition_counts = calloc(num_partitions, sizeof(unsigned int));
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        partition_counts[partition->partition_ids[i]]++;
    }
    
    for (unsigned int i = 0; i < num_partitions; i++) {
        printf("Partition %u: %u triangles\n", i, partition_counts[i]);
    }
    printf("\n");
    
    // Cleanup
    free(partition_counts);
    free_spatial_partition(partition);
    free_bvh(bvh);
    free_stl(stl);
    
    printf("BVH test completed successfully!\n");
    return 0;
} 