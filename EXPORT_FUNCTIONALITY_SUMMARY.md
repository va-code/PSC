# PSC Model Inspector - Export Functionality Summary

## Overview
The PSC Model Inspector has been enhanced with comprehensive export functionality for decomposed meshes and adjacency detection. After a model has been split apart through convex decomposition, the system can now:

1. **Detect adjacency** between all decomposed mesh pieces
2. **Create an adjacency list** showing which meshes are connected
3. **Save individual meshes** with proper naming based on the adjacency list
4. **Export additional information** about the decomposed models

## New Command Line Options

### Export Option
```
--export <output_dir>  Export decomposed meshes and adjacency information
                      (requires --convex option)
```

### Complete Usage Examples
```bash
# Basic convex decomposition with export
./build/parametric_slicer model.stl --convex 0.8 4 --export ./output

# Topology analysis only
./build/parametric_slicer model.stl --topology complete

# Convex decomposition only (visualization)
./build/parametric_slicer model.stl --convex 0.8 4

# Help
./build/parametric_slicer --help
```

## What Gets Exported

### 1. Individual Mesh Files
- **Format**: STL files
- **Naming**: `{base_filename}_part_{NNN}.stl`
- **Example**: `A.stl_part_000.stl`, `A.stl_part_001.stl`, etc.
- **Content**: Each decomposed mesh piece as a separate STL file
- **Metadata**: Concavity score and other properties in STL header

### 2. Adjacency Information
- **File**: `{base_filename}_adjacency.txt`
- **Content**:
  - Adjacency matrix showing which meshes are connected
  - Detailed adjacency information with shared vertices and contact areas
  - Total count of meshes and adjacencies

### 3. Complete Decomposition Data
- **File**: `{base_filename}_decomposition.json`
- **Content**:
  - Decomposition tree statistics (depth, nodes, leaves)
  - Individual mesh information (index, filename, concavity, bounds)
  - Adjacency relationships between meshes
  - All data in structured JSON format for programmatic use

## Adjacency Detection Algorithm

### How It Works
1. **Vertex Comparison**: Compares vertices between mesh pairs using tolerance-based matching
2. **Shared Vertex Detection**: Identifies meshes that share at least 3 vertices (forming a face)
3. **Contact Area Calculation**: Estimates contact area based on shared vertices and triangle areas
4. **Adjacency Graph**: Builds a complete graph structure showing all mesh relationships

### Adjacency Criteria
- **Minimum shared vertices**: 3 (to ensure actual face contact)
- **Tolerance**: 1e-6 units for vertex matching
- **Contact area**: Approximated from shared vertices and average triangle area

## Technical Implementation

### New Files Added
- `src/mesh_adjacency.h` - Header file with data structures and function declarations
- `src/mesh_adjacency.c` - Implementation of adjacency detection and export functions

### Key Data Structures
```c
// Adjacency between two meshes
typedef struct {
    int mesh1_index;           // Index of first mesh
    int mesh2_index;           // Index of second mesh
    int shared_vertices;        // Number of shared vertices
    float contact_area;         // Approximate contact area
} mesh_adjacency_t;

// Complete adjacency graph
typedef struct {
    int num_meshes;                    // Total number of meshes
    int num_adjacencies;              // Total number of adjacency relationships
    mesh_adjacency_t* adjacencies;    // Array of adjacency relationships
    int* adjacency_counts;            // Number of adjacencies per mesh
    int** adjacency_lists;            // Lists of adjacent mesh indices
} adjacency_graph_t;
```

### Core Functions
- `detect_mesh_adjacency()` - Main adjacency detection function
- `check_mesh_adjacency()` - Checks adjacency between two specific meshes
- `export_individual_meshes()` - Exports individual STL files
- `export_adjacency_info()` - Exports adjacency text file
- `export_decomposition_info_json()` - Exports complete JSON data

## Build and Integration

### Makefile Updates
- Added `mesh_adjacency.c` to both main program and convex decomposition builds
- Ensures all functionality is available in the compiled program

### Dependencies
- Requires `--convex` option to be used with `--export`
- Integrates with existing convex decomposition pipeline
- No additional external dependencies

## Example Output

### Adjacency Matrix
```
Adjacency Matrix:
     0  1  2  3  4  5  6  7
 0   .  X  .  .  X  .  X  X
 1   X  .  X  X  .  .  .  X
 2   .  X  .  .  X  .  X  .
 3   .  X  .  .  X  X  .  X
 4   X  .  X  X  .  X  .  .
 5   .  .  .  X  X  .  X  X
 6   X  .  X  .  .  X  .  .
 7   X  X  .  X  .  X  .  .
```
Where `X` indicates adjacent meshes and `.` indicates non-adjacent meshes.

### JSON Structure
```json
{
  "decomposition_info": {
    "base_filename": "A.stl",
    "num_meshes": 8,
    "tree_depth": 3,
    "total_nodes": 13,
    "leaf_nodes": 8
  },
  "meshes": [...],
  "adjacencies": [...]
}
```

## Use Cases

### 1. Manufacturing and Assembly
- Export individual parts for separate manufacturing
- Use adjacency information for assembly planning
- Understand part relationships and dependencies

### 2. Analysis and Visualization
- Import individual meshes into other CAD/CAM software
- Analyze adjacency patterns for design optimization
- Generate assembly diagrams and instructions

### 3. Data Processing
- JSON format enables programmatic analysis
- Adjacency data useful for graph-based algorithms
- Complete decomposition metadata for research/development

## Performance Considerations

### Adjacency Detection
- **Complexity**: O(N²) where N is number of meshes
- **Memory**: Scales linearly with number of meshes and adjacencies
- **Optimization**: Uses efficient vertex comparison with tolerance

### Export Performance
- **STL Export**: Linear time with number of triangles
- **File I/O**: Minimal overhead, primarily disk-bound
- **Memory**: Efficient streaming of mesh data

## Future Enhancements

### Potential Improvements
1. **Parallel Processing**: Multi-threaded adjacency detection for large meshes
2. **Advanced Adjacency**: Edge-based adjacency detection in addition to vertex-based
3. **Export Formats**: Support for additional formats (OBJ, PLY, etc.)
4. **Visualization**: Interactive adjacency graph visualization
5. **Batch Processing**: Support for multiple input files

## Conclusion

The new export functionality provides a complete solution for:
- **Mesh Decomposition**: Splitting complex models into manageable pieces
- **Adjacency Detection**: Understanding spatial relationships between parts
- **Data Export**: Saving all information in multiple useful formats
- **Integration**: Seamless workflow from decomposition to export

All functionality is now callable from the compiled program through command-line options, making it easy to integrate into automated workflows and batch processing systems.
