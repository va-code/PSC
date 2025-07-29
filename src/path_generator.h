#ifndef PATH_GENERATOR_H
#define PATH_GENERATOR_H

#include "slicer.h"

// G-code command types
typedef enum {
    GCODE_MOVE,     // G0/G1 - Linear move
    GCODE_ARC,      // G2/G3 - Arc move
    GCODE_SET_POS,  // G92 - Set position
    GCODE_HOME,     // G28 - Home axes
    GCODE_SET_UNITS, // G20/G21 - Set units
    GCODE_SET_TEMP, // M104/M109 - Set temperature
    GCODE_FAN,      // M106/M107 - Fan control
    GCODE_END       // M2 - End program
} gcode_type_t;

// G-code command structure
typedef struct {
    gcode_type_t type;
    float x, y, z, e;       // Coordinates and extrusion
    float f;                // Feed rate
    float s;                // Speed/temperature
    char* comment;          // Optional comment
} gcode_command_t;

// Path generator structure
typedef struct {
    gcode_command_t* commands;
    int num_commands;
    int capacity;
    float current_x, current_y, current_z, current_e;
    float print_speed, travel_speed;
    float nozzle_diameter, filament_diameter;
} path_generator_t;

// Function declarations
path_generator_t* path_generator_create(const slicing_params_t* params);
void path_generator_free(path_generator_t* generator);
void generate_gcode_from_slices(path_generator_t* generator, const sliced_model_t* model);
void add_gcode_command(path_generator_t* generator, gcode_command_t command);
void write_gcode_to_file(path_generator_t* generator, const char* filename);
void add_move_command(path_generator_t* generator, float x, float y, float z, float e, int is_travel);
void add_temperature_command(path_generator_t* generator, float temp);
void add_fan_command(path_generator_t* generator, int fan_speed);
void add_home_command(path_generator_t* generator);
void add_end_command(path_generator_t* generator);

#endif // PATH_GENERATOR_H 