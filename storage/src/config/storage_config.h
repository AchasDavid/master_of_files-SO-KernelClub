#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

typedef struct {
    int puerto_escucha;         // Numérico
    bool fresh_start;           // Booleano  
    char* punto_montaje;        // String
    int retardo_operacion;      // Numérico
    int retardo_acceso_bloque;  // Numérico
    t_log_level log_level;      // Log level enum
} t_storage_config;

/**
 * Crea la configuración del Storage y la devuelve
 * 
 * @param config_file_path Ruta absoluta del archivo de configuración
 * @warning Cuando se deja de usar, debe ser destruido con destroy_storage_config
 */
t_storage_config* create_storage_config(const char* config_file_path);

/**
 * Elimina de la memoria la configuración de Storage y todos sus atributos
 * 
 * @param storage_config La struct de configuración del storage
 */
void destroy_storage_config(t_storage_config* storage_config);

#endif
