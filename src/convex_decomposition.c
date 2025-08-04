#include "convex_decomposition.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>

// Convex hull operations
convex_hull_t* convex_hull_create(unsigned int initial_capacity) {
    convex_hull_t* hull = malloc(sizeof(convex_hull_t));
    if (!hull) return NULL;
    
    hull->vertices = malloc(initial_capacity * sizeof(point3d_t));
    if (!hull->vertices) {
        free(hull);
        return NULL;
    }
    
    hull->faces = malloc(initial_capacity * sizeof(face_t));
    if (!hull->faces) {
        free(hull->vertices);
        free(hull);
        return NULL;
    }
    
    hull->num_vertices = 0;
    hull->capacity = initial_capacity;
    hull->num_faces = 0;
    hull->face_capacity = initial_capacity;
    
    // Initialize bounds
    hull->bounds[0] = hull->bounds[1] = hull->bounds[2] = FLT_MAX;
    hull->bounds[3] = hull->bounds[4] = hull->bounds[5] = -FLT_MAX;
    
    return hull;
}

void convex_hull_free(convex_hull_t* hull) {
    if (!hull) return;
    
    if (hull->vertices) {
        free(hull->vertices);
    }
    if (hull->faces) {
        free(hull->faces);
    }
    free(hull);
}

void convex_hull_add_vertex(convex_hull_t* hull, float x, float y, float z) {
    if (!hull) return;
    
    // Expand capacity if needed
    if (hull->num_vertices >= hull->capacity) {
        hull->capacity *= 2;
        hull->vertices = realloc(hull->vertices, hull->capacity * sizeof(point3d_t));
        if (!hull->vertices) return;
    }
    
    hull->vertices[hull->num_vertices].x = x;
    hull->vertices[hull->num_vertices].y = y;
    hull->vertices[hull->num_vertices].z = z;
    hull->num_vertices++;
    
    // Update bounds
    if (x < hull->bounds[0]) hull->bounds[0] = x;
    if (y < hull->bounds[1]) hull->bounds[1] = y;
    if (z < hull->bounds[2]) hull->bounds[2] = z;
    if (x > hull->bounds[3]) hull->bounds[3] = x;
    if (y > hull->bounds[4]) hull->bounds[4] = y;
    if (z > hull->bounds[5]) hull->bounds[5] = z;
}

void convex_hull_add_face(convex_hull_t* hull, unsigned int v1, unsigned int v2, unsigned int v3) {
    if (!hull) return;
    
    // Expand face capacity if needed
    if (hull->num_faces >= hull->face_capacity) {
        hull->face_capacity *= 2;
        hull->faces = realloc(hull->faces, hull->face_capacity * sizeof(face_t));
        if (!hull->faces) return;
    }
    
    hull->faces[hull->num_faces].vertices[0] = v1;
    hull->faces[hull->num_faces].vertices[1] = v2;
    hull->faces[hull->num_faces].vertices[2] = v3;
    
    // Compute face normal and distance
    point3d_t* p1 = &hull->vertices[v1];
    point3d_t* p2 = &hull->vertices[v2];
    point3d_t* p3 = &hull->vertices[v3];
    
    // Compute two edge vectors
    float ux = p2->x - p1->x;
    float uy = p2->y - p1->y;
    float uz = p2->z - p1->z;
    
    float vx = p3->x - p1->x;
    float vy = p3->y - p1->y;
    float vz = p3->z - p1->z;
    
    // Cross product to get normal
    hull->faces[hull->num_faces].normal.x = uy * vz - uz * vy;
    hull->faces[hull->num_faces].normal.y = uz * vx - ux * vz;
    hull->faces[hull->num_faces].normal.z = ux * vy - uy * vx;
    
    // Normalize the normal vector
    float length = sqrtf(hull->faces[hull->num_faces].normal.x * hull->faces[hull->num_faces].normal.x +
                        hull->faces[hull->num_faces].normal.y * hull->faces[hull->num_faces].normal.y +
                        hull->faces[hull->num_faces].normal.z * hull->faces[hull->num_faces].normal.z);
    
    if (length > 0.0f) {
        hull->faces[hull->num_faces].normal.x /= length;
        hull->faces[hull->num_faces].normal.y /= length;
        hull->faces[hull->num_faces].normal.z /= length;
    }
    
    // Compute distance from origin to plane
    hull->faces[hull->num_faces].d = -(hull->faces[hull->num_faces].normal.x * p1->x +
                                      hull->faces[hull->num_faces].normal.y * p1->y +
                                      hull->faces[hull->num_faces].normal.z * p1->z);
    
    hull->num_faces++;
}

// QuickHull algorithm for 3D convex hull
convex_hull_t* compute_convex_hull_3d(const point3d_t* points, unsigned int num_points, 
                                      convex_hull_algorithm_t algorithm) {
    if (!points || num_points < 4) return NULL;
    
    convex_hull_t* hull = convex_hull_create(num_points);
    if (!hull) return NULL;
    
    // Find extreme points along each axis
    unsigned int min_x = 0, max_x = 0;
    unsigned int min_y = 0, max_y = 0;
    unsigned int min_z = 0, max_z = 0;
    
    for (unsigned int i = 1; i < num_points; i++) {
        if (points[i].x < points[min_x].x) min_x = i;
        if (points[i].x > points[max_x].x) max_x = i;
        if (points[i].y < points[min_y].y) min_y = i;
        if (points[i].y > points[max_y].y) max_y = i;
        if (points[i].z < points[min_z].z) min_z = i;
        if (points[i].z > points[max_z].z) max_z = i;
    }
    
    // Find the pair with maximum distance
    float max_dist = 0.0f;
    unsigned int p1 = 0, p2 = 0;
    
    // Check all extreme point pairs
    unsigned int extreme_points[] = {min_x, max_x, min_y, max_y, min_z, max_z};
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            float dx = points[extreme_points[i]].x - points[extreme_points[j]].x;
            float dy = points[extreme_points[i]].y - points[extreme_points[j]].y;
            float dz = points[extreme_points[i]].z - points[extreme_points[j]].z;
            float dist = dx * dx + dy * dy + dz * dz;
            if (dist > max_dist) {
                max_dist = dist;
                p1 = extreme_points[i];
                p2 = extreme_points[j];
            }
        }
    }
    
    // Add the two extreme points to hull
    convex_hull_add_vertex(hull, points[p1].x, points[p1].y, points[p1].z);
    convex_hull_add_vertex(hull, points[p2].x, points[p2].y, points[p2].z);
    
    // Find the point with maximum distance from the line p1-p2
    max_dist = 0.0f;
    unsigned int p3 = 0;
    for (unsigned int i = 0; i < num_points; i++) {
        if (i == p1 || i == p2) continue;
        
        // Vector from p1 to p2
        float vx = points[p2].x - points[p1].x;
        float vy = points[p2].y - points[p1].y;
        float vz = points[p2].z - points[p1].z;
        
        // Vector from p1 to current point
        float wx = points[i].x - points[p1].x;
        float wy = points[i].y - points[p1].y;
        float wz = points[i].z - points[p1].z;
        
        // Cross product magnitude gives distance
        float cross_x = vy * wz - vz * wy;
        float cross_y = vz * wx - vx * wz;
        float cross_z = vx * wy - vy * wx;
        float dist = sqrtf(cross_x * cross_x + cross_y * cross_y + cross_z * cross_z);
        
        if (dist > max_dist) {
            max_dist = dist;
            p3 = i;
        }
    }
    
    // Add the third point
    convex_hull_add_vertex(hull, points[p3].x, points[p3].y, points[p3].z);
    
    // Create initial tetrahedron faces
    convex_hull_add_face(hull, 0, 1, 2); // Front face
    convex_hull_add_face(hull, 0, 2, 1); // Back face (opposite orientation)
    
    // Find the fourth point to complete the tetrahedron
    max_dist = 0.0f;
    unsigned int p4 = 0;
    for (unsigned int i = 0; i < num_points; i++) {
        if (i == p1 || i == p2 || i == p3) continue;
        
        // Check distance to the plane formed by the first three points
        float vx = points[p2].x - points[p1].x;
        float vy = points[p2].y - points[p1].y;
        float vz = points[p2].z - points[p1].z;
        
        float wx = points[p3].x - points[p1].x;
        float wy = points[p3].y - points[p1].y;
        float wz = points[p3].z - points[p1].z;
        
        // Normal vector
        float nx = vy * wz - vz * wy;
        float ny = vz * wx - vx * wz;
        float nz = vx * wy - vy * wx;
        
        // Distance from point to plane
        float dx = points[i].x - points[p1].x;
        float dy = points[i].y - points[p1].y;
        float dz = points[i].z - points[p1].z;
        
        float dist = fabsf(nx * dx + ny * dy + nz * dz);
        if (dist > max_dist) {
            max_dist = dist;
            p4 = i;
        }
    }
    
    // Add the fourth point
    convex_hull_add_vertex(hull, points[p4].x, points[p4].y, points[p4].z);
    
    // Create the four faces of the tetrahedron
    hull->num_faces = 0; // Reset faces
    convex_hull_add_face(hull, 0, 1, 2);
    convex_hull_add_face(hull, 0, 3, 1);
    convex_hull_add_face(hull, 0, 2, 3);
    convex_hull_add_face(hull, 1, 3, 2);
    
    // Now expand the hull by adding remaining points
    for (unsigned int i = 0; i < num_points; i++) {
        if (i == p1 || i == p2 || i == p3 || i == p4) continue;
        
        // Add this point to the hull
        convex_hull_add_vertex(hull, points[i].x, points[i].y, points[i].z);
        unsigned int new_vertex = hull->num_vertices - 1;
        
        // Find visible faces and create new faces
        unsigned int* visible_faces = malloc(hull->num_faces * sizeof(unsigned int));
        unsigned int num_visible = 0;
        
        for (unsigned int f = 0; f < hull->num_faces; f++) {
            float dist = hull->faces[f].normal.x * points[i].x +
                        hull->faces[f].normal.y * points[i].y +
                        hull->faces[f].normal.z * points[i].z +
                        hull->faces[f].d;
            
            if (dist > 0.001f) { // Point is outside this face
                visible_faces[num_visible++] = f;
            }
        }
        
        if (num_visible > 0) {
            // Remove visible faces and create new ones
            // This is a simplified version - in practice, you'd need to handle edge cases
            // and ensure proper face connectivity
            
            // For now, we'll just add the point and let the hull grow
            // A full implementation would remove visible faces and create new ones
            // connecting the new point to the boundary edges of visible faces
        }
        
        free(visible_faces);
    }
    
    return hull;
}

// Convex part operations
convex_part_t* convex_part_create(unsigned int initial_capacity) {
    convex_part_t* part = malloc(sizeof(convex_part_t));
    if (!part) return NULL;
    
    part->triangle_indices = malloc(initial_capacity * sizeof(unsigned int));
    if (!part->triangle_indices) {
        free(part);
        return NULL;
    }
    
    part->num_triangles = 0;
    part->capacity = initial_capacity;
    part->volume = 0.0f;
    part->center[0] = part->center[1] = part->center[2] = 0.0f;
    
    // Initialize hull
    part->hull.vertices = NULL;
    part->hull.num_vertices = 0;
    part->hull.capacity = 0;
    part->hull.faces = NULL;
    part->hull.num_faces = 0;
    part->hull.face_capacity = 0;
    
    return part;
}

void convex_part_free(convex_part_t* part) {
    if (!part) return;
    
    if (part->triangle_indices) {
        free(part->triangle_indices);
    }
    if (part->hull.vertices) {
        free(part->hull.vertices);
    }
    if (part->hull.faces) {
        free(part->hull.faces);
    }
    free(part);
}

void convex_part_add_triangle(convex_part_t* part, unsigned int triangle_index) {
    if (!part) return;
    
    // Expand capacity if needed
    if (part->num_triangles >= part->capacity) {
        part->capacity *= 2;
        part->triangle_indices = realloc(part->triangle_indices, part->capacity * sizeof(unsigned int));
        if (!part->triangle_indices) return;
    }
    
    part->triangle_indices[part->num_triangles] = triangle_index;
    part->num_triangles++;
}

void convex_part_compute_properties(convex_part_t* part, const stl_file_t* stl) {
    if (!part || !stl || part->num_triangles == 0) return;
    
    // Compute centroid
    float total_x = 0.0f, total_y = 0.0f, total_z = 0.0f;
    unsigned int total_vertices = 0;
    
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        for (int j = 0; j < 3; j++) {
            total_x += triangle->vertices[j][0];
            total_y += triangle->vertices[j][1];
            total_z += triangle->vertices[j][2];
            total_vertices++;
        }
    }
    
    if (total_vertices > 0) {
        part->center[0] = total_x / total_vertices;
        part->center[1] = total_y / total_vertices;
        part->center[2] = total_z / total_vertices;
    }
    
    // Compute convex hull from all vertices
    point3d_t* vertices = malloc(part->num_triangles * 3 * sizeof(point3d_t));
    if (!vertices) return;
    
    unsigned int num_vertices = 0;
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        for (int j = 0; j < 3; j++) {
            vertices[num_vertices].x = triangle->vertices[j][0];
            vertices[num_vertices].y = triangle->vertices[j][1];
            vertices[num_vertices].z = triangle->vertices[j][2];
            num_vertices++;
        }
    }
    
    // Compute convex hull
    convex_hull_t* hull = compute_convex_hull_3d(vertices, num_vertices, CONVEX_HULL_QUICKHULL);
    if (hull) {
        // Copy hull data to part
        if (part->hull.vertices) free(part->hull.vertices);
        if (part->hull.faces) free(part->hull.faces);
        
        part->hull.vertices = malloc(hull->num_vertices * sizeof(point3d_t));
        part->hull.faces = malloc(hull->num_faces * sizeof(face_t));
        
        if (part->hull.vertices && part->hull.faces) {
            memcpy(part->hull.vertices, hull->vertices, hull->num_vertices * sizeof(point3d_t));
            memcpy(part->hull.faces, hull->faces, hull->num_faces * sizeof(face_t));
            part->hull.num_vertices = hull->num_vertices;
            part->hull.num_faces = hull->num_faces;
            part->hull.capacity = hull->num_vertices;
            part->hull.face_capacity = hull->num_faces;
            
            // Copy bounds
            memcpy(part->hull.bounds, hull->bounds, 6 * sizeof(float));
        }
        
        convex_hull_free(hull);
    }
    
    free(vertices);
    
    // Compute volume using convex hull
    part->volume = compute_volume(&part->hull);
}

// Convex node operations
convex_node_t* convex_node_create_leaf(unsigned int node_id, convex_part_t* part) {
    convex_node_t* node = malloc(sizeof(convex_node_t));
    if (!node) return NULL;
    
    node->type = CONVEX_LEAF;
    node->node_id = node_id;
    node->data.leaf.part = part;
    
    // Initialize bounds from part
    if (part) {
        memcpy(node->bounds, part->hull.bounds, 6 * sizeof(float));
    } else {
        node->bounds[0] = node->bounds[1] = node->bounds[2] = FLT_MAX;
        node->bounds[3] = node->bounds[4] = node->bounds[5] = -FLT_MAX;
    }
    
    node->concavity = 0.0f;
    return node;
}

convex_node_t* convex_node_create_internal(unsigned int node_id, convex_node_t* left, convex_node_t* right) {
    convex_node_t* node = malloc(sizeof(convex_node_t));
    if (!node) return NULL;
    
    node->type = CONVEX_INTERNAL;
    node->node_id = node_id;
    node->data.internal.left = left;
    node->data.internal.right = right;
    
    // Compute bounds from children
    compute_convex_node_bounds(node);
    
    node->concavity = 0.0f;
    return node;
}

void convex_node_free(convex_node_t* node) {
    if (!node) return;
    
    if (node->type == CONVEX_LEAF) {
        if (node->data.leaf.part) {
            convex_part_free(node->data.leaf.part);
        }
    } else {
        convex_node_free(node->data.internal.left);
        convex_node_free(node->data.internal.right);
    }
    
    free(node);
}

void compute_convex_node_bounds(convex_node_t* node) {
    if (!node || node->type != CONVEX_INTERNAL) return;
    
    convex_node_t* left = node->data.internal.left;
    convex_node_t* right = node->data.internal.right;
    
    if (!left || !right) return;
    
    // Compute union of children bounds
    node->bounds[0] = fminf(left->bounds[0], right->bounds[0]);
    node->bounds[1] = fminf(left->bounds[1], right->bounds[1]);
    node->bounds[2] = fminf(left->bounds[2], right->bounds[2]);
    node->bounds[3] = fmaxf(left->bounds[3], right->bounds[3]);
    node->bounds[4] = fmaxf(left->bounds[4], right->bounds[4]);
    node->bounds[5] = fmaxf(left->bounds[5], right->bounds[5]);
}

void convex_node_compute_concavity(convex_node_t* node, const stl_file_t* stl) {
    if (!node) return;
    
    if (node->type == CONVEX_LEAF) {
        if (node->data.leaf.part) {
            node->concavity = compute_part_concavity(node->data.leaf.part, stl);
        }
    } else {
        // For internal nodes, use average of children concavity
        convex_node_compute_concavity(node->data.internal.left, stl);
        convex_node_compute_concavity(node->data.internal.right, stl);
        node->concavity = (node->data.internal.left->concavity + node->data.internal.right->concavity) / 2.0f;
    }
}

// Adjacency operations
adjacency_list_t* adjacency_list_create(unsigned int node_id, unsigned int initial_capacity) {
    adjacency_list_t* list = malloc(sizeof(adjacency_list_t));
    if (!list) return NULL;
    
    list->entries = malloc(initial_capacity * sizeof(adjacency_entry_t));
    if (!list->entries) {
        free(list);
        return NULL;
    }
    
    list->node_id = node_id;
    list->num_adjacent = 0;
    list->capacity = initial_capacity;
    
    return list;
}

void adjacency_list_free(adjacency_list_t* list) {
    if (!list) return;
    
    if (list->entries) {
        free(list->entries);
    }
    free(list);
}

void adjacency_list_add_entry(adjacency_list_t* list, unsigned int adjacent_node_id, 
                             float overlap_volume, float distance) {
    if (!list) return;
    
    // Expand capacity if needed
    if (list->num_adjacent >= list->capacity) {
        list->capacity *= 2;
        list->entries = realloc(list->entries, list->capacity * sizeof(adjacency_entry_t));
        if (!list->entries) return;
    }
    
    list->entries[list->num_adjacent].node_id = adjacent_node_id;
    list->entries[list->num_adjacent].overlap_volume = overlap_volume;
    list->entries[list->num_adjacent].distance = distance;
    list->num_adjacent++;
}

// Decomposition operations
convex_decomposition_t* convex_decomposition_create(unsigned int initial_capacity) {
    convex_decomposition_t* decomp = malloc(sizeof(convex_decomposition_t));
    if (!decomp) return NULL;
    
    decomp->root = NULL;
    decomp->num_nodes = 0;
    decomp->num_leaf_nodes = 0;
    decomp->max_depth = 0;
    decomp->strategy = DECOMP_APPROX_CONVEX;
    decomp->total_volume = 0.0f;
    decomp->decomposition_quality = 0.0f;
    
    decomp->adjacency_lists = malloc(initial_capacity * sizeof(adjacency_list_t*));
    if (!decomp->adjacency_lists) {
        free(decomp);
        return NULL;
    }
    
    decomp->num_adjacency_lists = 0;
    
    return decomp;
}

void convex_decomposition_free(convex_decomposition_t* decomp) {
    if (!decomp) return;
    
    if (decomp->root) {
        convex_node_free(decomp->root);
    }
    
    for (unsigned int i = 0; i < decomp->num_adjacency_lists; i++) {
        if (decomp->adjacency_lists[i]) {
            adjacency_list_free(decomp->adjacency_lists[i]);
        }
    }
    
    if (decomp->adjacency_lists) {
        free(decomp->adjacency_lists);
    }
    
    free(decomp);
}

// Main decomposition functions
convex_decomposition_t* decompose_model(const stl_file_t* stl, const decomposition_params_t* params) {
    if (!stl || !params) return NULL;
    
    switch (params->strategy) {
        case DECOMP_APPROX_CONVEX:
            return approximate_convex_decomposition(stl, params->max_parts, params->quality_threshold, params->concavity_tolerance);
        case DECOMP_HIERARCHICAL:
            return hierarchical_decomposition(stl, params->max_parts, params->quality_threshold);
        case DECOMP_VOXEL_BASED:
            return voxel_based_decomposition(stl, params->voxel_size, params->min_triangles_per_voxel);
        default:
            return approximate_convex_decomposition(stl, params->max_parts, params->quality_threshold, params->concavity_tolerance);
    }
}

convex_decomposition_t* decompose_model_simple(const stl_file_t* stl, decomposition_strategy_t strategy,
                                             unsigned int max_parts, float quality_threshold) {
    if (!stl) return NULL;
    
    decomposition_params_t params = {
        .strategy = strategy,
        .max_parts = max_parts,
        .quality_threshold = quality_threshold,
        .concavity_tolerance = 0.1f,  // Default 10% concavity tolerance
        .voxel_size = 1.0f,
        .min_triangles_per_voxel = 10
    };
    
    return decompose_model(stl, &params);
}

convex_decomposition_t* approximate_convex_decomposition(const stl_file_t* stl, 
                                                        unsigned int max_parts, 
                                                        float quality_threshold,
                                                        float concavity_tolerance) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(max_parts);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_APPROX_CONVEX;
    
    // Start with all triangles in one part
    convex_part_t* initial_part = convex_part_create(stl->num_triangles);
    if (!initial_part) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    // Add all triangles to the initial part
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        convex_part_add_triangle(initial_part, i);
    }
    convex_part_compute_properties(initial_part, stl);
    
    // Build hierarchical tree
    unsigned int next_node_id = 0;
    decomp->root = hierarchical_decompose_part(initial_part, stl, next_node_id++, max_parts, concavity_tolerance, &next_node_id);
    
    if (decomp->root) {
        // Count nodes and compute properties
        count_nodes_and_compute_properties(decomp->root, decomp);
        
        // Build adjacency lists
        build_adjacency_lists(decomp);
        
        decomp->decomposition_quality = compute_decomposition_quality(decomp);
    }
    
    return decomp;
}

// Hierarchical decomposition functions
convex_node_t* hierarchical_decompose_part(convex_part_t* part, const stl_file_t* stl,
                                          unsigned int node_id, unsigned int max_parts,
                                          float concavity_tolerance, unsigned int* next_node_id) {
    if (!part || !stl || !next_node_id) return NULL;
    
    // Compute concavity of current part
    float concavity = compute_part_concavity(part, stl);
    
    // If concavity is within tolerance or we've reached max parts, create leaf node
    if (concavity <= concavity_tolerance || *next_node_id >= max_parts) {
        return convex_node_create_leaf(node_id, part);
    }
    
    // Split the part along its longest axis
    float dx = part->hull.bounds[3] - part->hull.bounds[0];
    float dy = part->hull.bounds[4] - part->hull.bounds[1];
    float dz = part->hull.bounds[5] - part->hull.bounds[2];
    
    int split_axis = 0; // X
    if (dy > dx && dy > dz) split_axis = 1; // Y
    else if (dz > dx && dz > dy) split_axis = 2; // Z
    
    float split_value = (part->hull.bounds[split_axis] + part->hull.bounds[split_axis + 3]) / 2.0f;
    
    // Create two new parts
    convex_part_t* left_part = convex_part_create(part->num_triangles / 2);
    convex_part_t* right_part = convex_part_create(part->num_triangles / 2);
    
    if (!left_part || !right_part) {
        if (left_part) convex_part_free(left_part);
        if (right_part) convex_part_free(right_part);
        return convex_node_create_leaf(node_id, part);
    }
    
    // Distribute triangles based on their centers
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        // Calculate triangle center
        float center = (triangle->vertices[0][split_axis] + 
                       triangle->vertices[1][split_axis] + 
                       triangle->vertices[2][split_axis]) / 3.0f;
        
        if (center < split_value) {
            convex_part_add_triangle(left_part, triangle_idx);
        } else {
            convex_part_add_triangle(right_part, triangle_idx);
        }
    }
    
    // Recursively decompose the split parts
    convex_node_t* left_node = NULL;
    convex_node_t* right_node = NULL;
    
    if (left_part->num_triangles > 0) {
        convex_part_compute_properties(left_part, stl);
        left_node = hierarchical_decompose_part(left_part, stl, (*next_node_id)++, max_parts, concavity_tolerance, next_node_id);
    }
    
    if (right_part->num_triangles > 0) {
        convex_part_compute_properties(right_part, stl);
        right_node = hierarchical_decompose_part(right_part, stl, (*next_node_id)++, max_parts, concavity_tolerance, next_node_id);
    }
    
    // If either child failed, return leaf node with original part
    if (!left_node || !right_node) {
        if (left_node) convex_node_free(left_node);
        if (right_node) convex_node_free(right_node);
        if (left_part) convex_part_free(left_part);
        if (right_part) convex_part_free(right_part);
        return convex_node_create_leaf(node_id, part);
    }
    
    // Create internal node
    convex_node_t* internal_node = convex_node_create_internal(node_id, left_node, right_node);
    
    // Free the original part since it's been split
    convex_part_free(part);
    
    return internal_node;
}

void count_nodes_and_compute_properties(convex_node_t* node, convex_decomposition_t* decomp) {
    if (!node || !decomp) return;
    
    decomp->num_nodes++;
    
    if (node->type == CONVEX_LEAF) {
        decomp->num_leaf_nodes++;
        if (node->data.leaf.part) {
            decomp->total_volume += node->data.leaf.part->volume;
        }
    } else {
        count_nodes_and_compute_properties(node->data.internal.left, decomp);
        count_nodes_and_compute_properties(node->data.internal.right, decomp);
    }
}

void build_adjacency_lists(convex_decomposition_t* decomp) {
    if (!decomp || !decomp->root) return;
    
    // Allocate adjacency lists for all nodes
    decomp->adjacency_lists = realloc(decomp->adjacency_lists, decomp->num_nodes * sizeof(adjacency_list_t*));
    if (!decomp->adjacency_lists) return;
    
    // Initialize adjacency lists for all nodes
    for (unsigned int i = 0; i < decomp->num_nodes; i++) {
        decomp->adjacency_lists[i] = NULL;
    }
    
    // Build adjacency lists for leaf nodes only
    build_adjacency_for_leaf_nodes(decomp->root, decomp);
}

void build_adjacency_for_leaf_nodes(convex_node_t* node, convex_decomposition_t* decomp) {
    if (!node || !decomp) return;
    
    if (node->type == CONVEX_LEAF) {
        // Create adjacency list for this leaf node
        adjacency_list_t* list = adjacency_list_create(node->node_id, 10);
        if (list) {
            decomp->adjacency_lists[decomp->num_adjacency_lists++] = list;
            
            // Check adjacency with all other leaf nodes
            check_adjacency_with_other_leaves(node, decomp->root, list, decomp);
        }
    } else {
        build_adjacency_for_leaf_nodes(node->data.internal.left, decomp);
        build_adjacency_for_leaf_nodes(node->data.internal.right, decomp);
    }
}

void check_adjacency_with_other_leaves(convex_node_t* current_node, convex_node_t* other_node, 
                                      adjacency_list_t* list, convex_decomposition_t* decomp) {
    if (!current_node || !other_node || !list || current_node == other_node) return;
    
    if (other_node->type == CONVEX_LEAF) {
        // Check if these leaf nodes are adjacent
        if (hulls_intersect(&current_node->data.leaf.part->hull, &other_node->data.leaf.part->hull)) {
            float overlap_volume = compute_overlap_volume(&current_node->data.leaf.part->hull, &other_node->data.leaf.part->hull);
            float distance = compute_hull_distance(&current_node->data.leaf.part->hull, &other_node->data.leaf.part->hull);
            
            adjacency_list_add_entry(list, other_node->node_id, overlap_volume, distance);
        }
    } else {
        // Recursively check children
        check_adjacency_with_other_leaves(current_node, other_node->data.internal.left, list, decomp);
        check_adjacency_with_other_leaves(current_node, other_node->data.internal.right, list, decomp);
    }
}

// Utility functions
float compute_volume(const convex_hull_t* hull) {
    if (!hull || hull->num_vertices < 4 || hull->num_faces == 0) return 0.0f;
    
    // Compute volume using the convex hull faces
    // Volume = (1/6) * sum of signed volumes of tetrahedra formed by origin and each face
    float volume = 0.0f;
    
    for (unsigned int i = 0; i < hull->num_faces; i++) {
        const face_t* face = &hull->faces[i];
        const point3d_t* v1 = &hull->vertices[face->vertices[0]];
        const point3d_t* v2 = &hull->vertices[face->vertices[1]];
        const point3d_t* v3 = &hull->vertices[face->vertices[2]];
        
        // Compute signed volume of tetrahedron (0, v1, v2, v3)
        // Volume = (1/6) * dot(v1, cross(v2, v3))
        float ux = v2->x - v1->x;
        float uy = v2->y - v1->y;
        float uz = v2->z - v1->z;
        
        float vx = v3->x - v1->x;
        float vy = v3->y - v1->y;
        float vz = v3->z - v1->z;
        
        float cross_x = uy * vz - uz * vy;
        float cross_y = uz * vx - ux * vz;
        float cross_z = ux * vy - uy * vx;
        
        float signed_volume = (v1->x * cross_x + v1->y * cross_y + v1->z * cross_z) / 6.0f;
        volume += signed_volume;
    }
    
    return fabsf(volume);
}

float compute_centroid(const convex_hull_t* hull, float* center) {
    if (!hull || !center || hull->num_vertices == 0) return 0.0f;
    
    center[0] = (hull->bounds[0] + hull->bounds[3]) / 2.0f;
    center[1] = (hull->bounds[1] + hull->bounds[4]) / 2.0f;
    center[2] = (hull->bounds[2] + hull->bounds[5]) / 2.0f;
    
    return 1.0f;
}

float compute_decomposition_quality(const convex_decomposition_t* decomp) {
    if (!decomp || decomp->num_leaf_nodes == 0) return 0.0f;
    
    // Simple quality metric based on part distribution
    float avg_volume = decomp->total_volume / decomp->num_leaf_nodes;
    float variance = 0.0f;
    
    // Collect volumes from leaf nodes
    float* volumes = malloc(decomp->num_leaf_nodes * sizeof(float));
    if (!volumes) return 0.0f;
    
    unsigned int leaf_index = 0;
    collect_leaf_volumes(decomp->root, volumes, &leaf_index);
    
    for (unsigned int i = 0; i < decomp->num_leaf_nodes; i++) {
        float diff = volumes[i] - avg_volume;
        variance += diff * diff;
    }
    variance /= decomp->num_leaf_nodes;
    
    free(volumes);
    
    // Quality is inversely proportional to variance
    float quality = 1.0f / (1.0f + variance);
    return quality;
}

void collect_leaf_volumes(convex_node_t* node, float* volumes, unsigned int* index) {
    if (!node || !volumes || !index) return;
    
    if (node->type == CONVEX_LEAF) {
        if (node->data.leaf.part) {
            volumes[*index] = node->data.leaf.part->volume;
            (*index)++;
        }
    } else {
        collect_leaf_volumes(node->data.internal.left, volumes, index);
        collect_leaf_volumes(node->data.internal.right, volumes, index);
    }
}

// Analysis and visualization
void print_decomposition_info(const convex_decomposition_t* decomp) {
    if (!decomp) return;
    
    printf("Convex Decomposition Information:\n");
    printf("Strategy: %d\n", decomp->strategy);
    printf("Number of nodes: %u\n", decomp->num_nodes);
    printf("Number of leaf nodes: %u\n", decomp->num_leaf_nodes);
    printf("Total volume: %.3f\n", decomp->total_volume);
    printf("Decomposition quality: %.3f\n", decomp->decomposition_quality);
    printf("\n");
    
    if (decomp->root) {
        print_convex_node_info(decomp->root, 0);
    }
    
    // Print adjacency information
    printf("Adjacency Information:\n");
    for (unsigned int i = 0; i < decomp->num_adjacency_lists; i++) {
        if (decomp->adjacency_lists[i]) {
            printf("Node %u has %u adjacent nodes:\n", 
                   decomp->adjacency_lists[i]->node_id, 
                   decomp->adjacency_lists[i]->num_adjacent);
            
            for (unsigned int j = 0; j < decomp->adjacency_lists[i]->num_adjacent; j++) {
                adjacency_entry_t* entry = &decomp->adjacency_lists[i]->entries[j];
                printf("  -> Node %u (overlap: %.3f, distance: %.3f)\n", 
                       entry->node_id, entry->overlap_volume, entry->distance);
            }
            printf("\n");
        }
    }
}

void print_convex_node_info(const convex_node_t* node, unsigned int depth) {
    if (!node) return;
    
    // Print indentation
    for (unsigned int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    printf("Node %u (", node->node_id);
    
    if (node->type == CONVEX_LEAF) {
        printf("LEAF");
        if (node->data.leaf.part) {
            printf(", triangles: %u, volume: %.3f", 
                   node->data.leaf.part->num_triangles, 
                   node->data.leaf.part->volume);
        }
    } else {
        printf("INTERNAL");
    }
    
    printf(", concavity: %.3f", node->concavity);
    printf(")\n");
    
    if (node->type == CONVEX_INTERNAL) {
        print_convex_node_info(node->data.internal.left, depth + 1);
        print_convex_node_info(node->data.internal.right, depth + 1);
    }
}

void print_part_info(const convex_part_t* part, unsigned int part_index) {
    if (!part) return;
    
    printf("Part %u:\n", part_index);
    printf("  Triangles: %u\n", part->num_triangles);
    printf("  Volume: %.3f\n", part->volume);
    printf("  Center: (%.3f, %.3f, %.3f)\n", part->center[0], part->center[1], part->center[2]);
    printf("  Bounds: X[%.3f, %.3f] Y[%.3f, %.3f] Z[%.3f, %.3f]\n",
           part->hull.bounds[0], part->hull.bounds[3],
           part->hull.bounds[1], part->hull.bounds[4],
           part->hull.bounds[2], part->hull.bounds[5]);
    printf("\n");
}

// Geometry utilities
float cross_product_2d(float x1, float y1, float x2, float y2) {
    return x1 * y2 - x2 * y1;
}

float dot_product_3d(float x1, float y1, float z1, float x2, float y2, float z2) {
    return x1 * x2 + y1 * y2 + z1 * z2;
}

float distance_3d(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

int orientation_2d(float x1, float y1, float x2, float y2, float x3, float y3) {
    float val = (y2 - y1) * (x3 - x2) - (x2 - x1) * (y3 - y2);
    if (val > 0) return 1;   // Clockwise
    if (val < 0) return -1;  // Counter-clockwise
    return 0;                // Collinear
}

convex_decomposition_t* hierarchical_decomposition(const stl_file_t* stl, 
                                                  unsigned int max_depth,
                                                  float split_threshold) {
    if (!stl || stl->num_triangles == 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(1 << max_depth);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_HIERARCHICAL;
    
    // Start with all triangles in one part
    convex_part_t* initial_part = convex_part_create(stl->num_triangles);
    if (!initial_part) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        convex_part_add_triangle(initial_part, i);
    }
    convex_part_compute_properties(initial_part, stl);
    
    // Build hierarchical tree
    unsigned int next_node_id = 0;
    decomp->root = hierarchical_decompose_part(initial_part, stl, next_node_id++, 1 << max_depth, split_threshold, &next_node_id);
    
    if (decomp->root) {
        // Count nodes and compute properties
        count_nodes_and_compute_properties(decomp->root, decomp);
        
        // Build adjacency lists
        build_adjacency_lists(decomp);
        
        decomp->decomposition_quality = compute_decomposition_quality(decomp);
    }
    
    return decomp;
}

convex_decomposition_t* voxel_based_decomposition(const stl_file_t* stl, 
                                                 float voxel_size,
                                                 unsigned int min_triangles_per_voxel) {
    if (!stl || stl->num_triangles == 0 || voxel_size <= 0) return NULL;
    
    convex_decomposition_t* decomp = convex_decomposition_create(100);
    if (!decomp) return NULL;
    
    decomp->strategy = DECOMP_VOXEL_BASED;
    
    // Calculate voxel grid dimensions
    float min_x = stl->bounds[0], max_x = stl->bounds[3];
    float min_y = stl->bounds[1], max_y = stl->bounds[4];
    float min_z = stl->bounds[2], max_z = stl->bounds[5];
    
    int num_voxels_x = (int)((max_x - min_x) / voxel_size) + 1;
    int num_voxels_y = (int)((max_y - min_y) / voxel_size) + 1;
    int num_voxels_z = (int)((max_z - min_z) / voxel_size) + 1;
    
    // Create voxel grid
    unsigned int*** voxel_grid = malloc(num_voxels_x * sizeof(unsigned int**));
    if (!voxel_grid) {
        convex_decomposition_free(decomp);
        return NULL;
    }
    
    for (int x = 0; x < num_voxels_x; x++) {
        voxel_grid[x] = malloc(num_voxels_y * sizeof(unsigned int*));
        if (!voxel_grid[x]) {
            // Cleanup
            for (int i = 0; i < x; i++) {
                for (int y = 0; y < num_voxels_y; y++) {
                    if (voxel_grid[i][y]) free(voxel_grid[i][y]);
                }
                free(voxel_grid[i]);
            }
            free(voxel_grid);
            convex_decomposition_free(decomp);
            return NULL;
        }
        
        for (int y = 0; y < num_voxels_y; y++) {
            voxel_grid[x][y] = calloc(num_voxels_z, sizeof(unsigned int));
            if (!voxel_grid[x][y]) {
                // Cleanup
                for (int i = 0; i < x; i++) {
                    for (int j = 0; j < num_voxels_y; j++) {
                        if (voxel_grid[i][j]) free(voxel_grid[i][j]);
                    }
                    free(voxel_grid[i]);
                }
                for (int j = 0; j < y; j++) {
                    if (voxel_grid[x][j]) free(voxel_grid[x][j]);
                }
                free(voxel_grid[x]);
                free(voxel_grid);
                convex_decomposition_free(decomp);
                return NULL;
            }
        }
    }
    
    // Assign triangles to voxels
    for (unsigned int i = 0; i < stl->num_triangles; i++) {
        const stl_triangle_t* triangle = &stl->triangles[i];
        
        // Calculate triangle center
        float center_x = (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f;
        float center_y = (triangle->vertices[0][1] + triangle->vertices[1][1] + triangle->vertices[2][1]) / 3.0f;
        float center_z = (triangle->vertices[0][2] + triangle->vertices[1][2] + triangle->vertices[2][2]) / 3.0f;
        
        int voxel_x = (int)((center_x - min_x) / voxel_size);
        int voxel_y = (int)((center_y - min_y) / voxel_size);
        int voxel_z = (int)((center_z - min_z) / voxel_size);
        
        if (voxel_x >= 0 && voxel_x < num_voxels_x &&
            voxel_y >= 0 && voxel_y < num_voxels_y &&
            voxel_z >= 0 && voxel_z < num_voxels_z) {
            voxel_grid[voxel_x][voxel_y][voxel_z]++;
        }
    }
    
    // Create parts for voxels with enough triangles
    unsigned int next_node_id = 0;
    for (int x = 0; x < num_voxels_x; x++) {
        for (int y = 0; y < num_voxels_y; y++) {
            for (int z = 0; z < num_voxels_z; z++) {
                if (voxel_grid[x][y][z] >= min_triangles_per_voxel) {
                    convex_part_t* part = convex_part_create(voxel_grid[x][y][z]);
                    if (part) {
                        // Add triangles from this voxel
                        for (unsigned int i = 0; i < stl->num_triangles; i++) {
                            const stl_triangle_t* triangle = &stl->triangles[i];
                            float center_x = (triangle->vertices[0][0] + triangle->vertices[1][0] + triangle->vertices[2][0]) / 3.0f;
                            float center_y = (triangle->vertices[0][1] + triangle->vertices[1][1] + triangle->vertices[2][1]) / 3.0f;
                            float center_z = (triangle->vertices[0][2] + triangle->vertices[1][2] + triangle->vertices[2][2]) / 3.0f;
                            
                            int tri_voxel_x = (int)((center_x - min_x) / voxel_size);
                            int tri_voxel_y = (int)((center_y - min_y) / voxel_size);
                            int tri_voxel_z = (int)((center_z - min_z) / voxel_size);
                            
                            if (tri_voxel_x == x && tri_voxel_y == y && tri_voxel_z == z) {
                                convex_part_add_triangle(part, i);
                            }
                        }
                        
                        if (part->num_triangles > 0) {
                            convex_part_compute_properties(part, stl);
                            // Create leaf node for this part
                            convex_node_t* leaf_node = convex_node_create_leaf(next_node_id++, part);
                            if (leaf_node) {
                                // Add to tree (simplified - just store as root for now)
                                if (!decomp->root) {
                                    decomp->root = leaf_node;
                                }
                            }
                        } else {
                            convex_part_free(part);
                        }
                    }
                }
            }
        }
    }
    
    // Cleanup voxel grid
    for (int x = 0; x < num_voxels_x; x++) {
        for (int y = 0; y < num_voxels_y; y++) {
            free(voxel_grid[x][y]);
        }
        free(voxel_grid[x]);
    }
    free(voxel_grid);
    
    if (decomp->root) {
        // Count nodes and compute properties
        count_nodes_and_compute_properties(decomp->root, decomp);
        
        // Build adjacency lists
        build_adjacency_lists(decomp);
        
        decomp->decomposition_quality = compute_decomposition_quality(decomp);
    }
    
    return decomp;
}

float compute_part_concavity(const convex_part_t* part, const stl_file_t* stl) {
    if (!part || !stl || part->num_triangles == 0) return 0.0f;
    
    // Compute actual volume from triangles
    float actual_volume = 0.0f;
    for (unsigned int i = 0; i < part->num_triangles; i++) {
        unsigned int triangle_idx = part->triangle_indices[i];
        const stl_triangle_t* triangle = &stl->triangles[triangle_idx];
        
        // Compute signed volume of triangle with respect to origin
        float v1x = triangle->vertices[0][0];
        float v1y = triangle->vertices[0][1];
        float v1z = triangle->vertices[0][2];
        
        float v2x = triangle->vertices[1][0];
        float v2y = triangle->vertices[1][1];
        float v2z = triangle->vertices[1][2];
        
        float v3x = triangle->vertices[2][0];
        float v3y = triangle->vertices[2][1];
        float v3z = triangle->vertices[2][2];
        
        // Volume = (1/6) * dot(v1, cross(v2, v3))
        float ux = v2x - v1x;
        float uy = v2y - v1y;
        float uz = v2z - v1z;
        
        float wx = v3x - v1x;
        float wy = v3y - v1y;
        float wz = v3z - v1z;
        
        float cross_x = uy * wz - uz * wy;
        float cross_y = uz * wx - ux * wz;
        float cross_z = ux * wy - uy * wx;
        
        float signed_volume = (v1x * cross_x + v1y * cross_y + v1z * cross_z) / 6.0f;
        actual_volume += fabsf(signed_volume);
    }
    
    // Compute convex hull volume
    float hull_volume = compute_volume(&part->hull);
    
    if (hull_volume <= 0.0f) return 0.0f;
    
    // Concavity is the ratio of unused hull volume to total hull volume
    float concavity = (hull_volume - actual_volume) / hull_volume;
    
    // Clamp to [0, 1] range
    if (concavity < 0.0f) concavity = 0.0f;
    if (concavity > 1.0f) concavity = 1.0f;
    
    return concavity;
}

// Additional utility functions
int hulls_intersect(const convex_hull_t* hull1, const convex_hull_t* hull2) {
    if (!hull1 || !hull2) return 0;
    
    // Check if bounding boxes overlap
    if (hull1->bounds[3] < hull2->bounds[0] || hull1->bounds[0] > hull2->bounds[3] ||
        hull1->bounds[4] < hull2->bounds[1] || hull1->bounds[1] > hull2->bounds[4] ||
        hull1->bounds[5] < hull2->bounds[2] || hull1->bounds[2] > hull2->bounds[5]) {
        return 0;
    }
    
    return 1;
}

float compute_hull_distance(const convex_hull_t* hull1, const convex_hull_t* hull2) {
    if (!hull1 || !hull2) return FLT_MAX;
    
    // Simple distance calculation using bounding box centers
    float center1[3], center2[3];
    compute_centroid(hull1, center1);
    compute_centroid(hull2, center2);
    
    return distance_3d(center1[0], center1[1], center1[2], center2[0], center2[1], center2[2]);
}

float compute_overlap_volume(const convex_hull_t* hull1, const convex_hull_t* hull2) {
    if (!hull1 || !hull2) return 0.0f;
    
    // Check if bounding boxes overlap
    if (hull1->bounds[3] < hull2->bounds[0] || hull1->bounds[0] > hull2->bounds[3] ||
        hull1->bounds[4] < hull2->bounds[1] || hull1->bounds[1] > hull2->bounds[4] ||
        hull1->bounds[5] < hull2->bounds[2] || hull1->bounds[2] > hull2->bounds[5]) {
        return 0.0f;
    }
    
    // Compute intersection bounds
    float intersection_bounds[6];
    intersection_bounds[0] = fmaxf(hull1->bounds[0], hull2->bounds[0]);
    intersection_bounds[1] = fmaxf(hull1->bounds[1], hull2->bounds[1]);
    intersection_bounds[2] = fmaxf(hull1->bounds[2], hull2->bounds[2]);
    intersection_bounds[3] = fminf(hull1->bounds[3], hull2->bounds[3]);
    intersection_bounds[4] = fminf(hull1->bounds[4], hull2->bounds[4]);
    intersection_bounds[5] = fminf(hull1->bounds[5], hull2->bounds[5]);
    
    // Compute intersection volume (simplified as bounding box volume)
    float width = intersection_bounds[3] - intersection_bounds[0];
    float height = intersection_bounds[4] - intersection_bounds[1];
    float depth = intersection_bounds[5] - intersection_bounds[2];
    
    return width * height * depth;
} 