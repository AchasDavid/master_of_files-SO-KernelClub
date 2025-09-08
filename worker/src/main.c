#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <utils/cliente.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>

t_log* log_create_in_working_dir(char *process_name, bool is_active_console, char* s_level) {
    char directorio_actual[PATH_MAX];
    char ruta_log[PATH_MAX];

    if (getcwd(directorio_actual, sizeof(directorio_actual)) == NULL) {
        return NULL;
    }

    t_log_level level = log_level_from_string(s_level);

    snprintf(ruta_log, sizeof(ruta_log), "%s/worker.log", directorio_actual);

    t_log* logger = log_create(ruta_log, process_name, is_active_console, level);

    return logger;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        perror("Se deben ingresar los parametros [archivo_config] y [ID Worker]");
        exit(EXIT_FAILURE);
    }
    
    char* config_file_path = argv[1];

    t_config* config = config_create(config_file_path);
    if (!config) {
        perror("No se pudo abrir el config");
        exit(EXIT_FAILURE);
    }


    char* master_ip = config_get_string_value(config, "IP_MASTER");
    char* master_port = config_get_string_value(config, "PUERTO_MASTER");
    char* log_level = config_get_string_value(config, "LOG_LEVEL");

    t_log* logger = log_create_in_working_dir("worker", true, log_level);

    int client_socket = conectar_servidor(master_ip, master_port);

    if (client_socket < 0) {
        config_destroy(config);
        log_destroy(logger);
        exit(EXIT_FAILURE);
    }

    log_info(logger, "## Se establecio conexión con Master %s:%s", master_ip, master_port);

    config_destroy(config);
    log_destroy(logger);
    return 0;
}

