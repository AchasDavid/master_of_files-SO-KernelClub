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
#include <connection/protocol.h>
#include <connection/serialization.h>

#define MODULO "QUERY_CONTROL"

int main(int argc, char* argv[])
{
    char* config_filepath = argv[1];
    char* query_filepath = argv[2];
    int priority = atoi(argv[3]);
    int retval = 0;

    if (argc != 4) {
        printf("[ERROR]: Se esperaban ruta al archivo de configuracion, archivo de query y prioridad\nUso: %s [archivo_config] [archivo_query] [prioridad]\n", argv[0]);
        retval = -1;
        goto error;
    }

    if (priority < 0) {
        printf("[ERROR]: La prioridad no puede ser negativa\n");
        retval = -8;
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
    if (master_socket < 0) {
        log_error(logger, "Error al conectar con el master en %s:%s", query_control_config->ip, query_control_config->port);
        retval = -5;
        goto clean_logger;
    }

    // Enviar handshake al master
    // Creo un nuevo package con el op_code
    t_package *package_handshake = package_create_empty(OP_QUERY_HANDSHAKE);
    if (package_handshake == NULL) 
    {
        log_error(logger, "Error al crear el paquete para el handshake al master");
        goto clean_socket;
    }

    // agrego un string como campo del buffer
    package_add_string(package_handshake, "QUERY_CONTROL_HANDSHAKE");

    // Envio el paquete
    if (package_send(package_handshake, master_socket) != 0)
    {
        log_error(logger, "Error al enviar el paquete para handshake al master");
        goto clean_socket;
    }

    // Destruyo el package
    package_destroy(package_handshake);

    // Preparo para recibir respuesta
    t_package *response_package = package_receive(master_socket);

    if (!response_package || response_package->operation_code != OP_QUERY_HANDSHAKE)
    {
        log_error(logger, "Error al recibir respuesta al handshake con Master!");
        goto clean_socket;
    }

    package_destroy(response_package);
    
    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s.", query_control_config->ip, query_control_config->port);

    // Comienza petición de ejecución de query
    log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", query_filepath, priority);

    t_package* package_to_send = package_create_empty(OP_QUERY_FILE_PATH);
    package_add_string(package_to_send, query_filepath);
    package_add_uint8(package_to_send, (uint8_t)priority);

    int connection_code = package_send(package_to_send, master_socket);
    if (connection_code < 0)
    {
        goto clean_socket;
    }

    // Preparo para recibir respuesta
    response_package = package_receive(master_socket); // --> Reutilzó response package

    if (response_package->operation_code != OP_QUERY_FILE_PATH)
    {
        log_error(logger, "Error al recibir respuesta..."); 
    }
    log_info(logger, "Paquete con path de query: %s y prioridad: %d enviado al master correctamente", query_filepath, priority);


    // TODO: Manejar la respuesta

    //package_destroy(pkg);
    package_destroy(response_package);

clean_socket:
    close(master_socket);
clean_logger:
    log_destroy(logger);
clean_config:
    destroy_query_control_config_instance(query_control_config);
error:
    return retval;
}
