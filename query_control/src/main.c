#include <commons/config.h>
#include <commons/log.h>
#include <config/query_control_config.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/hello.h>
#include <utils/server.h>
#include <utils/utils.h>

#define MODULO "QUERY_CONTROL"

int main(int argc, char* argv[])
{
    char* config_filepath = argv[1];
    char* query_filepath = argv[2];
    char* priority_str = argv[3];
    int retval = 0;

    if (argc != 4) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion, archivo de query y prioridad\nUso: %s [archivo_config] [archivo_query] [prioridad]\n", argv[0]);
        retval = -1;
        goto error;
    }

    saludar("query_control");

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        retval = -2;
        goto error;
    }

    t_query_control_config *query_control_config = create_query_control_config(config_filepath);
    if (!query_control_config)
    {
        fprintf(stderr, "Error al leer el archivo de configuracion %s\n", config_filepath);
        retval = -3;
        goto clean_config;
    }

    t_log* logger = create_logger(current_directory, MODULO, true, query_control_config->log_level);
    if (logger == NULL)
    {
        fprintf(stderr, "Error al crear el logger\n");
        retval = -4;
        goto clean_config;
    }

    log_debug(logger,
              "Configuracion leida: \n\tIP_MASTER=%s\n\tPUERTO_MASTER=%s\n\tLOG_LEVEL=%s",
              query_control_config->ip,
              query_control_config->port,
              log_level_as_string(query_control_config->log_level));

    int master_socket = connect_to_server(query_control_config->ip, query_control_config->port);
    if (master_socket == -1) {
        log_error(logger, "Error al conectar con el master en %s:%s", query_control_config->ip, query_control_config->port);
        retval = -5;
        goto clean_logger;
    }

    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s", query_control_config->ip, query_control_config->port);

    char* handshake_msg = "QUERY_CONTROL_HANDSHAKE";
    if (send(master_socket, handshake_msg, strlen(handshake_msg), 0) == -1) {
        log_error(logger, "Error al enviar handshake al master");
        retval = -6;
        goto clean_socket;
    }

    char response_buffer[256];
    int response_bytes = recv(master_socket, response_buffer, sizeof(response_buffer) - 1, 0);
    if (response_bytes <= 0) {
        log_error(logger, "Error al recibir respuesta del handshake del master");
        retval = -7;
        goto clean_socket;
    }

    response_buffer[response_bytes] = '\0';
    log_debug(logger, "Handshake completado con el master. Respuesta: %s", response_buffer);

    // TODO: Enviar el query file y manejar la respuesta

clean_socket:
    close(master_socket);
clean_logger:
    log_destroy(logger);
clean_config:
    destroy_query_control_config_instance(query_control_config);
error:
    return retval;
}
