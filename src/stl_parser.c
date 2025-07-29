#include "stl_parser.h"

stl_file_t* stl_load_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }

    stl_file_t* stl = malloc(sizeof(stl_file_t));
    if (!stl) {
        fclose(file);
        return NULL;
    }

    // Read header to determine if ASCII or binary
    if (fread(stl->header, 1, 80, file) != 80) {
        fprintf(stderr, "Error: Cannot read STL header\n");
        free(stl);
        fclose(file);
        return NULL;
    }

    // Check if it's ASCII STL (starts with "solid")
    if (strncmp(stl->header, "solid", 5) == 0) {
        if (stl_parse_ascii(file, stl) != 0) {
            fprintf(stderr, "Error: Failed to parse ASCII STL\n");
            free(stl);
            fclose(file);
            return NULL;
        }
    } else {
        if (stl_parse_binary(file, stl) != 0) {
            fprintf(stderr, "Error: Failed to parse binary STL\n");
            free(stl);
            fclose(file);
            return NULL;
        }
    }

    fclose(file);
    stl_calculate_bounds(stl);
    return stl;
}

void stl_free(stl_file_t* stl) {
    if (stl) {
        if (stl->triangles) {
            free(stl->triangles);
        }
        free(stl);
    }
}

int stl_parse_ascii(FILE* file, stl_file_t* stl) {
    // Reset file position after header
    fseek(file, 0, SEEK_SET);
    
    char line[256];
    int triangle_count = 0;
    
    // Count triangles first
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "facet normal")) {
            triangle_count++;
        }
    }
    
    stl->num_triangles = triangle_count;
    stl->triangles = malloc(triangle_count * sizeof(stl_triangle_t));
    
    if (!stl->triangles) {
        return -1;
    }
    
    // Reset and parse
    fseek(file, 0, SEEK_SET);
    int current_triangle = 0;
    
    while (fgets(line, sizeof(line), file) && current_triangle < triangle_count) {
        if (strstr(line, "facet normal")) {
            sscanf(line, "facet normal %f %f %f", 
                   &stl->triangles[current_triangle].normal[0],
                   &stl->triangles[current_triangle].normal[1],
                   &stl->triangles[current_triangle].normal[2]);
        } else if (strstr(line, "vertex")) {
            int vertex_idx = 0;
            if (strstr(line, "outer loop")) {
                // Read three vertices
                for (int i = 0; i < 3; i++) {
                    fgets(line, sizeof(line), file);
                    sscanf(line, "vertex %f %f %f",
                           &stl->triangles[current_triangle].vertices[i][0],
                           &stl->triangles[current_triangle].vertices[i][1],
                           &stl->triangles[current_triangle].vertices[i][2]);
                }
                current_triangle++;
            }
        }
    }
    
    return 0;
}

int stl_parse_binary(FILE* file, stl_file_t* stl) {
    // Skip header (already read)
    fseek(file, 80, SEEK_SET);
    
    // Read triangle count
    if (fread(&stl->num_triangles, sizeof(unsigned int), 1, file) != 1) {
        return -1;
    }
    
    // Allocate memory for triangles
    stl->triangles = malloc(stl->num_triangles * sizeof(stl_triangle_t));
    if (!stl->triangles) {
        return -1;
    }
    
    // Read all triangles
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        // Read normal vector
        if (fread(stl->triangles[i].normal, sizeof(float), 3, file) != 3) {
            return -1;
        }
        
        // Read three vertices
        for (int j = 0; j < 3; j++) {
            if (fread(stl->triangles[i].vertices[j], sizeof(float), 3, file) != 3) {
                return -1;
            }
        }
        
        // Skip attribute byte count (2 bytes)
        unsigned short attr;
        if (fread(&attr, sizeof(unsigned short), 1, file) != 1) {
            return -1;
        }
    }
    
    return 0;
}

void stl_calculate_bounds(stl_file_t* stl) {
    if (stl->num_triangles == 0) return;
    
    // Initialize bounds with first triangle
    stl->bounds[0] = stl->bounds[1] = stl->bounds[2] = FLT_MAX;  // min
    stl->bounds[3] = stl->bounds[4] = stl->bounds[5] = -FLT_MAX; // max
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                float val = stl->triangles[i].vertices[j][k];
                if (val < stl->bounds[k]) stl->bounds[k] = val;     // min
                if (val > stl->bounds[k+3]) stl->bounds[k+3] = val; // max
            }
        }
    }
}

void stl_print_info(const stl_file_t* stl) {
    printf("STL File Information:\n");
    printf("Header: %.80s\n", stl->header);
    printf("Number of triangles: %u\n", stl->num_triangles);
    printf("Bounding box:\n");
    printf("  X: %.3f to %.3f (width: %.3f)\n", 
           stl->bounds[0], stl->bounds[3], stl->bounds[3] - stl->bounds[0]);
    printf("  Y: %.3f to %.3f (depth: %.3f)\n", 
           stl->bounds[1], stl->bounds[4], stl->bounds[4] - stl->bounds[1]);
    printf("  Z: %.3f to %.3f (height: %.3f)\n", 
           stl->bounds[2], stl->bounds[5], stl->bounds[5] - stl->bounds[2]);
} 