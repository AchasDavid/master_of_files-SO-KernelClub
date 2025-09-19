#include "server.h"

int wait_for_client(int server_socket)
{
	struct sockaddr_in client_address;
    socklen_t address_size = sizeof(struct sockaddr_in);

	// Aceptamos un nuevo cliente
    int client_socket = accept(server_socket, (void*)&client_address, &address_size);
    if (client_socket == -1) {
        log_error(g_storage_logger, "Error al aceptar un Worker.");
        return -1;
    }

	g_worker_counter++;
    log_info(g_storage_logger, "## Se conecta un Worker - Cantidad de Workers: %d", g_worker_counter); // TODO: Loggear la cantidad de workers conectados

	return client_socket;
}

void* handle_client(void* arg) {
    t_client_data* client_data = (t_client_data*)arg;
    int client_socket = client_data->client_socket;
    //t_log* logger = client_data->logger;

    t_package *request = package_receive(client_socket);
    if (!request) {
        log_error(g_storage_logger, "Error en la recepción de la solicitud del Worker");
        goto cleanup;
    }
    
    t_package *response;
    switch (request->operation_code) {
        case OP_CODE_REQ_HANDSHAKE:
            log_info(g_storage_logger, "Cód. op.: %d. Handshake recibido del Worker mediante el socket %d.", request->operation_code, client_socket);
            response = reply_handshake();

            break;
        default:
            log_error(g_storage_logger, "Código de operación desconocido recibido del Worker: %u", request->operation_code);
            package_destroy(request);
            goto cleanup;
    }

    package_send(response, client_socket);
    package_destroy(request);
    package_destroy(response);

cleanup:
    close(client_socket);
    free(client_data);
    return NULL;
}

