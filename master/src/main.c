#include <utils/utils.h>
#include <utils/server.h>
#include <commons/config.h>
#include <linux/limits.h>

#define MODULO "MASTER"

int main(int argc, char* argv[]) {

    // Verifico que se hayan pasado los parametros correctamente
    if (argc != 2) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion\nUso: %s [archivo_config]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char* path_config_file = argv[1];

    // Cargo el archivo de configuracion 
    t_config* config = config_create(path_config_file);
    if (!config) {
        perror("No se pudo abrir el config");
        exit(EXIT_FAILURE);
    }

    // Leo las variables de configuracion
    char* ip = config_get_string_value(config, "IP_ESCUCHA");
    char* port = config_get_string_value(config, "PUERTO_ESCUCHA");
    char* scheduler_algorithm = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    int aging_time = config_get_int_value(config, "TIEMPO_AGING");
    char* log_level = config_get_string_value(config, "LOG_LEVEL");

    // Obtengo el directorio actual
    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        exit(EXIT_FAILURE);
    }

    // Inicializo el logger
    t_log* logger = create_logger(current_directory, MODULO, true, log_level);
    if (logger == NULL) {
        fprintf(stderr, "Error al crear el logger\n");
        return 1;
    }

    log_info(logger, "Configuracion leida: \n\tIP_ESCUCHA=%s\n\tPUERTO_ESCUCHA=%s\n\tALGORITMO_PLANIFICACION=%s\n\tTIEMPO_AGING=%d\n\tLOG_LEVEL=%s",
             ip, port, scheduler_algorithm, aging_time, log_level);

    // Inicio el servidor
    int socket;
    printf("DEBUG -> ip: %s, puerto: %s\n", ip, port);

    socket = start_server(ip, port);
    log_info(logger, "Socket %d creado con exito!", socket);

    // Libero recursos
    // Todo: Seguramente es más prolijo hacer una función que libere todo en utils
    config_destroy(config);
    log_destroy(logger);

    return 0;
}
