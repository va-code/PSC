#include "path_generator.h"
#include <math.h>

path_generator_t* path_generator_create(const slicing_params_t* params) {
    path_generator_t* generator = malloc(sizeof(path_generator_t));
    if (!generator) return NULL;
    
    generator->capacity = 1000;
    generator->num_commands = 0;
    generator->commands = malloc(generator->capacity * sizeof(gcode_command_t));
    
    if (!generator->commands) {
        free(generator);
        return NULL;
    }
    
    // Initialize position
    generator->current_x = 0.0f;
    generator->current_y = 0.0f;
    generator->current_z = 0.0f;
    generator->current_e = 0.0f;
    
    // Copy parameters
    generator->print_speed = params->print_speed;
    generator->travel_speed = params->travel_speed;
    generator->nozzle_diameter = params->nozzle_diameter;
    generator->filament_diameter = params->filament_diameter;
    
    return generator;
}

void path_generator_free(path_generator_t* generator) {
    if (!generator) return;
    
    // Free command comments
    for (int i = 0; i < generator->num_commands; i++) {
        if (generator->commands[i].comment) {
            free(generator->commands[i].comment);
        }
    }
    
    if (generator->commands) {
        free(generator->commands);
    }
    free(generator);
}

void generate_gcode_from_slices(path_generator_t* generator, const sliced_model_t* model) {
    if (!generator || !model) return;
    
    // Add start commands
    add_home_command(generator);
    add_temperature_command(generator, 200.0f); // Default temperature
    add_fan_command(generator, 0); // Start with fan off
    
    // Process each layer
    for (int layer_idx = 0; layer_idx < model->num_layers; layer_idx++) {
        const layer_t* layer = &model->layers[layer_idx];
        
        // Add layer comment
        gcode_command_t layer_comment = {0};
        layer_comment.type = GCODE_MOVE;
        layer_comment.comment = malloc(50);
        snprintf(layer_comment.comment, 50, "Layer %d, Z=%.3f", layer_idx + 1, layer->z_height);
        add_gcode_command(generator, layer_comment);
        
        // Move to layer height
        add_move_command(generator, generator->current_x, generator->current_y, layer->z_height, generator->current_e, 1);
        
        // Print contours (shell)
        for (int contour_idx = 0; contour_idx < layer->num_contours; contour_idx++) {
            const contour_t* contour = &layer->contours[contour_idx];
            
            if (contour->num_points < 3) continue;
            
            // Move to first point
            add_move_command(generator, contour->points[0].x, contour->points[0].y, layer->z_height, generator->current_e, 1);
            
            // Print contour
            for (int point_idx = 1; point_idx < contour->num_points; point_idx++) {
                float distance = sqrtf(
                    powf(contour->points[point_idx].x - contour->points[point_idx-1].x, 2) +
                    powf(contour->points[point_idx].y - contour->points[point_idx-1].y, 2)
                );
                
                // Calculate extrusion (simplified)
                float extrusion = distance * 0.1f; // 0.1mm per mm of travel
                add_move_command(generator, contour->points[point_idx].x, contour->points[point_idx].y, layer->z_height, generator->current_e + extrusion, 0);
            }
            
            // Close contour
            float distance = sqrtf(
                powf(contour->points[0].x - contour->points[contour->num_points-1].x, 2) +
                powf(contour->points[0].y - contour->points[contour->num_points-1].y, 2)
            );
            float extrusion = distance * 0.1f;
            add_move_command(generator, contour->points[0].x, contour->points[0].y, layer->z_height, generator->current_e + extrusion, 0);
        }
        
        // Print infill
        if (layer->num_infill_points > 0) {
            // Add infill comment
            gcode_command_t infill_comment = {0};
            infill_comment.type = GCODE_MOVE;
            infill_comment.comment = malloc(20);
            snprintf(infill_comment.comment, 20, "Infill");
            add_gcode_command(generator, infill_comment);
            
            for (int i = 0; i < layer->num_infill_points; i += 2) {
                if (i + 1 < layer->num_infill_points) {
                    // Move to start of line
                    add_move_command(generator, layer->infill_points[i].x, layer->infill_points[i].y, layer->z_height, generator->current_e, 1);
                    
                    // Print line
                    float distance = sqrtf(
                        powf(layer->infill_points[i+1].x - layer->infill_points[i].x, 2) +
                        powf(layer->infill_points[i+1].y - layer->infill_points[i].y, 2)
                    );
                    float extrusion = distance * 0.05f; // Less extrusion for infill
                    add_move_command(generator, layer->infill_points[i+1].x, layer->infill_points[i+1].y, layer->z_height, generator->current_e + extrusion, 0);
                }
            }
        }
    }
    
    // Add end commands
    add_fan_command(generator, 0);
    add_end_command(generator);
}

void add_gcode_command(path_generator_t* generator, gcode_command_t command) {
    if (!generator) return;
    
    // Expand capacity if needed
    if (generator->num_commands >= generator->capacity) {
        generator->capacity *= 2;
        generator->commands = realloc(generator->commands, generator->capacity * sizeof(gcode_command_t));
        if (!generator->commands) return;
    }
    
    generator->commands[generator->num_commands] = command;
    generator->num_commands++;
}

void write_gcode_to_file(path_generator_t* generator, const char* filename) {
    if (!generator || !filename) return;
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return;
    }
    
    // Write header
    fprintf(file, "; G-code generated by Parametric Slicer\n");
    fprintf(file, "; Number of commands: %d\n", generator->num_commands);
    fprintf(file, "; Print speed: %.1f mm/s\n", generator->print_speed);
    fprintf(file, "; Travel speed: %.1f mm/s\n", generator->travel_speed);
    fprintf(file, "\n");
    
    // Write commands
    for (int i = 0; i < generator->num_commands; i++) {
        const gcode_command_t* cmd = &generator->commands[i];
        
        switch (cmd->type) {
            case GCODE_MOVE:
                fprintf(file, "G1");
                if (cmd->x != generator->current_x || cmd->y != generator->current_y || 
                    cmd->z != generator->current_z || cmd->e != generator->current_e) {
                    if (cmd->x != generator->current_x) fprintf(file, " X%.3f", cmd->x);
                    if (cmd->y != generator->current_y) fprintf(file, " Y%.3f", cmd->y);
                    if (cmd->z != generator->current_z) fprintf(file, " Z%.3f", cmd->z);
                    if (cmd->e != generator->current_e) fprintf(file, " E%.3f", cmd->e);
                    if (cmd->f > 0) fprintf(file, " F%.1f", cmd->f);
                }
                break;
                
            case GCODE_HOME:
                fprintf(file, "G28");
                break;
                
            case GCODE_SET_TEMP:
                fprintf(file, "M104 S%.1f", cmd->s);
                break;
                
            case GCODE_FAN:
                if (cmd->s > 0) {
                    fprintf(file, "M106 S%d", (int)cmd->s);
                } else {
                    fprintf(file, "M107");
                }
                break;
                
            case GCODE_END:
                fprintf(file, "M2");
                break;
                
            default:
                break;
        }
        
        if (cmd->comment) {
            fprintf(file, " ; %s", cmd->comment);
        }
        fprintf(file, "\n");
        
        // Update current position
        if (cmd->type == GCODE_MOVE) {
            generator->current_x = cmd->x;
            generator->current_y = cmd->y;
            generator->current_z = cmd->z;
            generator->current_e = cmd->e;
        }
    }
    
    fclose(file);
    printf("G-code written to %s\n", filename);
}

void add_move_command(path_generator_t* generator, float x, float y, float z, float e, int is_travel) {
    gcode_command_t cmd = {0};
    cmd.type = GCODE_MOVE;
    cmd.x = x;
    cmd.y = y;
    cmd.z = z;
    cmd.e = e;
    cmd.f = is_travel ? generator->travel_speed * 60.0f : generator->print_speed * 60.0f; // Convert to mm/min
    add_gcode_command(generator, cmd);
}

void add_temperature_command(path_generator_t* generator, float temp) {
    gcode_command_t cmd = {0};
    cmd.type = GCODE_SET_TEMP;
    cmd.s = temp;
    add_gcode_command(generator, cmd);
}

void add_fan_command(path_generator_t* generator, int fan_speed) {
    gcode_command_t cmd = {0};
    cmd.type = GCODE_FAN;
    cmd.s = fan_speed;
    add_gcode_command(generator, cmd);
}

void add_home_command(path_generator_t* generator) {
    gcode_command_t cmd = {0};
    cmd.type = GCODE_HOME;
    add_gcode_command(generator, cmd);
}

void add_end_command(path_generator_t* generator) {
    gcode_command_t cmd = {0};
    cmd.type = GCODE_END;
    add_gcode_command(generator, cmd);
} 