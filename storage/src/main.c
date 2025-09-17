#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utils/server.h>
#include <utils/client_socket.h>
#include <config/storage_config.h>
#include <utils/utils.h>
#include <string.h>

#define MODULO "STORAGE"

t_storage_config* g_storage_config;
t_log* g_storage_logger;

int main(int argc, char* argv[]) {
    char config_file_path[PATH_MAX];
    int retval = 0;

    if (argc == 1) {
        strncpy(config_file_path, "./src/config/storage.config", PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else if (argc == 2) {
        strncpy(config_file_path, argv[1], PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else {
        fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]\n");
        retval = -1;
        goto error;
    }

    g_storage_config = create_storage_config(config_file_path);
    if (g_storage_config == NULL) {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        retval = -2;
        goto error;
    }

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        retval = -3;
        goto clean_config;
    }

    g_storage_logger = create_logger(current_directory, MODULO, true, g_storage_config->log_level);
    if (g_storage_logger == NULL) {
        fprintf(stderr, "Error al crear el logger\n");
        retval = -4;
        goto clean_config;
    }

    log_debug(g_storage_logger,
              "Configuracion leida: \\n\\tPUERTO_ESCUCHA=%d\\n\\tFRESH_START=%s\\n\\tPUNTO_MONTAJE=%s\\n\\tRETARDO_OPERACION=%d\\n\\tRETARDO_ACCESO_BLOQUE=%d\\n\\tLOG_LEVEL=%s",
              g_storage_config->puerto_escucha,
              g_storage_config->fresh_start ? "TRUE" : "FALSE",
              g_storage_config->punto_montaje,
              g_storage_config->retardo_operacion,
              g_storage_config->retardo_acceso_bloque,
              log_level_as_string(g_storage_config->log_level));

    char puerto_str[16];
    snprintf(puerto_str, sizeof(puerto_str), "%d", g_storage_config->puerto_escucha);

    int socket = start_server("127.0.0.1", puerto_str);
    if (socket < 0) {
        log_error(g_storage_logger, "No se pudo iniciar el servidor en el puerto %d", g_storage_config->puerto_escucha);
        retval = -5;
        goto clean_logger;
    }
    log_info(g_storage_logger, "Servidor iniciado en el puerto %d", g_storage_config->puerto_escucha);

    close(socket);
    log_destroy(g_storage_logger);
    destroy_storage_config(g_storage_config);
    exit(EXIT_SUCCESS);

clean_logger:
    log_destroy(g_storage_logger);
clean_config:
    destroy_storage_config(g_storage_config);
error:
    return retval;
}
