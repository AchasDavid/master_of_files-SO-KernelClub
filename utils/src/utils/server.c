#include "server.h"

// Códigos de error específicos
#define ERROR_INVALID_PARAMS    -1
#define ERROR_GETADDRINFO       -2
#define ERROR_SOCKET_CREATE     -3
#define ERROR_SETSOCKOPT        -4
#define ERROR_BIND              -5
#define ERROR_LISTEN            -6

int start_server(char *ip, char *port) {
    // Validación de parámetros
    if (ip == NULL || port == NULL) {
        return ERROR_INVALID_PARAMS;
    }

    struct addrinfo hints, *server_info = NULL;
    int server_socket = -1;
    int retval = 0;

    // Configuramos el socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 o IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // Para bind

    // Obtenemos información de la dirección
    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0) {
        retval = ERROR_GETADDRINFO;
        goto cleanup;
    }

    // Creamos el socket del servidor
    server_socket = socket(server_info->ai_family,
                          server_info->ai_socktype,
                          server_info->ai_protocol);
    if (server_socket == -1) {
        retval = ERROR_SOCKET_CREATE;
        goto cleanup;
    }

    // Configuramos SO_REUSEADDR para evitar "Address already in use"
    int yes = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        retval = ERROR_SETSOCKOPT;
        goto cleanup;
    }

    // Bindeamos el socket al puerto
    if (bind(server_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        retval = ERROR_BIND;
        goto cleanup;
    }

    // Ponemos el socket en modo de escucha
    if (listen(server_socket, SOMAXCONN) == -1) {
        retval = ERROR_LISTEN;
        goto cleanup;
    }

    // Éxito: liberamos server_info y retornamos el socket
    freeaddrinfo(server_info);
    return server_socket;

cleanup:
    // Cleanup centralizado - solo libera recursos que fueron asignados exitosamente
    if (server_info != NULL) {
        freeaddrinfo(server_info);
    }

    if (server_socket != -1) {
        close(server_socket);
    }

    return retval;
}

// Función auxiliar para obtener descripción del error
const char* start_server_error_string(int error_code) {
    switch (error_code) {
        case ERROR_INVALID_PARAMS:
            return "Invalid parameters (NULL IP or port)";
        case ERROR_GETADDRINFO:
            return "Failed to resolve address information";
        case ERROR_SOCKET_CREATE:
            return "Failed to create socket";
        case ERROR_SETSOCKOPT:
            return "Failed to set socket options";
        case ERROR_BIND:
            return "Failed to bind socket to address";
        case ERROR_LISTEN:
            return "Failed to set socket to listen mode";
        default:
            return "Unknown error";
    }
}

int connect_to_server(char *ip, char *port) {
    struct addrinfo hints, *server_info;
    int retval;

    if (ip == NULL || port == NULL) {
        retval = -1;
        goto error;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0) {
        retval = -2;
        goto error;
    }

    int client_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (client_socket == -1) {
        retval = -3;
        goto clean;
    }

    if (connect(client_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        retval = -4;
        goto connect_error;
    }

    freeaddrinfo(server_info);
    return client_socket;

connect_error:
    close(client_socket);
clean:
    freeaddrinfo(server_info);
error:
    return retval;
}
