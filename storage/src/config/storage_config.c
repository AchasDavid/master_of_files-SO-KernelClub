#include "storage_config.h"
#include <errno.h>

static bool has_required_properties(t_config *config);

t_storage_config *create_storage_config(const char *config_file_path)
{
    t_storage_config *storage_config = malloc(sizeof *storage_config);
    if (!storage_config)
    {
        fprintf(stderr, "No se pudo reservar memoria para storage_config: %s\n", strerror(errno));
        return NULL;
    }

    t_config *config = config_create((char *)config_file_path);
    if (!config)
    {
        fprintf(stderr, "No se pudo abrir el config: %s\n", config_file_path);
        free(storage_config);
        return NULL;
    }

    if (!has_required_properties(config))
    {
        fprintf(stderr, "El archivo de configuración no tiene todas las propiedades requeridas: %s\n",
                config_file_path);
        config_destroy(config);
        free(storage_config);
        return NULL;
    }

    storage_config->puerto_escucha = config_get_int_value(config, "PUERTO_ESCUCHA");

    char* fresh_start_str = config_get_string_value(config, "FRESH_START");
    if (!fresh_start_str) {
        goto cleanup;
    }
    if (strcmp(fresh_start_str, "TRUE") == 0 || strcmp(fresh_start_str, "true") == 0) {
        storage_config->fresh_start = true;
    } else {
        storage_config->fresh_start = false;
    }
    
    char* punto_montaje_str = config_get_string_value(config, "PUNTO_MONTAJE");
    if (!punto_montaje_str) {
        goto cleanup;
    }
    storage_config->punto_montaje = strdup(punto_montaje_str);
    if (!storage_config->punto_montaje) {
        goto cleanup;
    }
    
    storage_config->retardo_operacion = config_get_int_value(config, "RETARDO_OPERACION");
    storage_config->retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    
    char* log_level_str = config_get_string_value(config, "LOG_LEVEL");
    if (!log_level_str) {
        goto cleanup;
    }
    storage_config->log_level = log_level_from_string(log_level_str);

    config_destroy(config);
    return storage_config;

cleanup:
    config_destroy(config);
    destroy_storage_config(storage_config);
    return NULL;
}

void destroy_storage_config(t_storage_config *storage_config)
{
    if (!storage_config)
        return;

    // puerto_escucha es int, no necesita free
    // fresh_start es bool, no necesita free
    free(storage_config->punto_montaje);
    // retardo_operacion es int, no necesita free
    // retardo_acceso_bloque es int, no necesita free
    // log_level es enum, no necesita free

    free(storage_config);
}

static bool has_required_properties(t_config *config)
{
    char *required_props[] = {
        "PUERTO_ESCUCHA",
        "FRESH_START",
        "PUNTO_MONTAJE",
        "RETARDO_OPERACION",
        "RETARDO_ACCESO_BLOQUE",
        "LOG_LEVEL"
    };

    size_t n = sizeof(required_props) / sizeof(required_props[0]);
    for (size_t i = 0; i < n; ++i)
    {
        if (!config_has_property(config, required_props[i]))
        {
            fprintf(stderr, "Falta propiedad requerida: %s\n", required_props[i]);
            return false;
        }
    }
    return true;
}

