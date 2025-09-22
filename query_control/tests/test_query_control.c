#include <commons/config.h>
#include <commons/log.h>
#include <config/query_control_config.h>
#include <cspecs/cspec.h>
#include <linux/limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

context(test_query_control) {
    describe("Manejo de configuracion") {
        describe("archivo de configuracion valido") {
            t_query_control_config* config;

            before {
                config = create_query_control_config("tests/resources/test.config");
            } end

            after {
                destroy_query_control_config_instance(config);
            } end

            it("deberia cargar la configuracion correctamente desde un archivo valido") {
                should_ptr(config) not be null;
            } end

            it("deberia parsear correctamente la direccion IP") {
                should_string(config->ip) be equal to("127.0.0.1");
            } end

            it("deberia parsear correctamente el puerto") {
                should_string(config->port) be equal to("8080");
            } end

            it("deberia parsear correctamente el nivel de log") {
                should_int(config->log_level) be equal to(LOG_LEVEL_INFO);
            } end
        } end

        describe("Archivo de configuracion invalido") {
            it("deberia devolver error cuando el archivo de configuracion no existe") {
                t_query_control_config* config = create_query_control_config("nonexistent.config");
                should_ptr(config) be equal to((t_query_control_config*) -1);
            } end
        } end
    } end

    describe("Validacion de argumentos") {
        describe("Formateo de buffer") {
            it("deberia formatear correctamente el mensaje de ruta de query y prioridad") {
                char send_buffer[PATH_MAX+1+11+1];
                char* query_filepath = "/path/to/query.txt";
                int priority = 3;

                snprintf(send_buffer, sizeof(send_buffer), "%s\x1F%d", query_filepath, priority);

                should_string(send_buffer) be equal to("/path/to/query.txt\x1F" "3");
            } end

            it("deberia manejar el valor maximo de prioridad") {
                char send_buffer[PATH_MAX+1+11+1];
                char* query_filepath = "/test/query.txt";
                int priority = 2147483647; // INT_MAX

                int result = snprintf(send_buffer, sizeof(send_buffer), "%s\x1F%d", query_filepath, priority);

                should_bool(result > 0) be truthy;
                should_bool(result < sizeof(send_buffer)) be truthy;
            } end
        } end
    } end

    describe("Operaciones de directorio") {
        describe("Directorio de trabajo actual") {

            it("deberia obtener exitosamente el directorio de trabajo actual") {
                char current_directory[PATH_MAX];
                char* result = getcwd(current_directory, sizeof(current_directory));

                should_ptr(result) not be null;
                should_bool(strlen(current_directory) > 0) be truthy;
            } end
        } end
    } end
}
