#include "server.h"

int start_server(char *ip, char *port) {
    struct addrinfo hints, *server_info;

    // Configuramos el socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int result = getaddrinfo(ip, port, &hints, &server_info);
    if (result != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(result));
        return -1;
    }

    // Creamos el socket de escucha del servidor
    int server_socket = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (server_socket == -1) {
        printf("Socket creation error\n%s\n", strerror(errno));
        freeaddrinfo(server_info);
        return -1;
    }

    // Asociamos el socket a un puerto. Verifico si se asoció correctamente
    int yes = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        printf("setsockopt error\n%s\n", strerror(errno));
        freeaddrinfo(server_info);
        close(server_socket);
        return -1;
    }

    // Bindeo el puerto al socket
    if (bind(server_socket, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        close(server_socket);
        printf("Bind error\n%s\n", strerror(errno));
        freeaddrinfo(server_info);
        return -1;
    }

    // Escuchamos las conexiones entrantes
    if (listen(server_socket, SOMAXCONN) == -1) {
        printf("Listen error\n%s\n", strerror(errno));
        freeaddrinfo(server_info);
        return -1;
    }
    freeaddrinfo(server_info);

    return server_socket;
}