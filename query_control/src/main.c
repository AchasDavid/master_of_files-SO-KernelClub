#include <utils/hello.h>
#include <utils/server.h>

int main(int argc, char* argv[]) {
    saludar("query_control");

    // SOLO PARA TEST DE FUNCIÓN start_server hardcodeo ip y puerto
    int socket;
    socket = start_server("127.0.0.1", "8001");
    printf("Socket %d creado con exito!\n", socket);
    return 0;
}
