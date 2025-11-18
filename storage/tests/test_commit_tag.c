#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h> 
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/dictionary.h>
#include <globals/globals.h>
#include <fresh_start/fresh_start.h>
#include "config/storage_config.h"
#include "connection/protocol.h" 
#include "connection/serialization.h"
#include "errors.h"
#include "test_utils.h"
#include <operations/commit_tag.h>
#include <cspecs/cspec.h>

void setup_filesystem_environment() {
    create_test_directory();
    create_test_superblock(TEST_MOUNT_POINT);
    create_test_storage_config("9090", "99", "false", TEST_MOUNT_POINT, 100, 10, "INFO"); 

    char config_path[PATH_MAX];
    snprintf(config_path, sizeof(config_path), "%s/storage.config", TEST_MOUNT_POINT);
    g_storage_config = create_storage_config(config_path);

    g_open_files_dict = dictionary_create();
    init_physical_blocks(TEST_MOUNT_POINT, g_storage_config->fs_size, g_storage_config->block_size);
}

void teardown_filesystem_environment() {
    destroy_storage_config(g_storage_config);
    cleanup_file_sync();
    cleanup_test_directory();
}

context(tests_commit_tag) {
    
    // -------------------------------------------------------------------------
    // 1. Tests para deserialize_tag_commit_request
    // -------------------------------------------------------------------------
    describe("Deserialización de solicitud COMMIT TAG") {
        before {
            g_storage_logger = create_test_logger();
        } end

        after {
            destroy_test_logger(g_storage_logger);
        } end

        it("Deserializa correctamente todos los datos de una solicitud valida") {
            uint32_t query_id;
            char *name = NULL;
            char *tag = NULL;

            t_package *package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

            package_add_uint32(package, 100);        
            package_add_string(package, "config_file");  
            package_add_string(package, "v1");     

            package_simulate_reception(package);

            int retval = deserialize_tag_commit_request(package, &query_id, &name, &tag);
            
            should_int(retval) be equal to (0);
            should_int(query_id) be equal to (100);
            should_string(name) be equal to ("config_file");
            should_string(tag) be equal to ("v1");

            package_destroy(package);
            free(name);
            free(tag);
        } end

        it("Falla al deserializar el tag y limpia la memoria asignada para el nombre") {
            uint32_t query_id = 0;
            char *name = NULL;
            char *tag = NULL;

            t_package *package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);

            package_add_uint32(package, 101);
            package_add_string(package, "file_to_clean");

            package_simulate_reception(package);
            
            int retval = deserialize_tag_commit_request(package, &query_id, &name, &tag);
            
            should_int(retval) be equal to (-3);
            should_ptr(tag) be null;
            should_ptr(name) be null; 

            package_destroy(package);
            if (name) free(name);
        } end
    } end

    // -------------------------------------------------------------------------
    // 2. Tests para execute_tag_commit (Lógica central)
    // -------------------------------------------------------------------------
    describe ("Lógica central de commit de tag") {
        before {
            g_storage_logger = create_test_logger();
            setup_filesystem_environment();
        } end

        after {
            teardown_filesystem_environment();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Debe comitear exitosamente un archivo en estado WORK_IN_PROGRESS") {
            char *name = "file1";
            char *tag = "tag1";
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT); 
            create_test_metadata(name, tag, 1, "[10]", (char*)IN_PROGRESS, g_storage_config->mount_point);

            int result = execute_tag_commit(200, name, tag);

            should_int(result) be equal to (0);
            
            // VERIFICACIÓN DE ESTADO EN EL FS
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_ptr(metadata_after_commit) not be null;
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);
            
            // Cleanup de verificación
            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

        it ("Debe retornar SUCCESS si el archivo ya esta en estado COMMITTED (Idempotencia)") {
            char *name = "file1";
            char *tag = "tag1";
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT); 
            create_test_metadata(name, tag, 1, "[10]", (char*)COMMITTED, g_storage_config->mount_point);
            
            int result = execute_tag_commit(201, name, tag);

            should_int(result) be equal to (0);
            
            // VERIFICACIÓN: El estado debe seguir siendo COMMITTED
            t_file_metadata *metadata_after = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_ptr(metadata_after) not be null;
            should_string(metadata_after->state) be equal to ((char*)COMMITTED);
            
            // Cleanup de verificación
            if (metadata_after) destroy_file_metadata(metadata_after);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

        it ("Debe retornar FILE_TAG_MISSING si el directorio del archivo no existe") {
            char *name = "file_missing";
            char *tag = "tag_missing";
            
            int result = execute_tag_commit(202, name, tag);

            should_int(result) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

        it ("Debe retornar FILE_TAG_MISSING si no se puede leer el metadata.config (read_file_metadata falla)") {
            char *name = "file_no_metadata";
            char *tag = "tag_no_metadata";
            init_logical_blocks(name, tag, 1, TEST_MOUNT_POINT); 
            
            int result = execute_tag_commit(203, name, tag);

            should_int(result) be equal to (FILE_TAG_MISSING);
            should_bool(correct_unlock(name, tag)) be truthy;
        } end

    } end


    // -------------------------------------------------------------------------
    // 3. Tests para handle_tag_commit_request (Manejador completo)
    // -------------------------------------------------------------------------
    describe ("Manejador de solicitud COMMIT TAG (De punta a punta)") {
        before {
            g_storage_logger = create_test_logger();
            setup_filesystem_environment();
            
            init_logical_blocks("file1", "tag1", 1, TEST_MOUNT_POINT); 
            create_test_metadata("file1", "tag1", 1, "[10]", (char*)IN_PROGRESS, g_storage_config->mount_point);
        } end

        after {
            teardown_filesystem_environment();
            destroy_test_logger(g_storage_logger);
        } end

        it ("Manejo de commit exitoso y paquete de respuesta con SUCCESS code") {
            char *name = "file1";
            char *tag = "tag1";

            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)300); // query_id
            package_add_string(request_package, name);
            package_add_string(request_package, tag);
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_TAG_COMMIT_RES);
            
            int8_t status_code;
            package_read_int8(response, &status_code);
            should_int(status_code) be equal to (0);
            
            t_file_metadata *metadata_after_commit = read_file_metadata(g_storage_config->mount_point, name, tag);
            should_string(metadata_after_commit->state) be equal to ((char*)COMMITTED);

            if (metadata_after_commit) destroy_file_metadata(metadata_after_commit);
            should_bool(correct_unlock(name, tag)) be truthy;
            package_destroy(response);
            package_destroy(request_package);
        } end

        it ("El manejador falla en la ejecucion (FILE_TAG_MISSING) y devuelve el código de error") {
            char *name = "file2";
            char *tag = "tag2";
            
            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)301);
            package_add_string(request_package, name);
            package_add_string(request_package, tag);
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) not be null;
            should_int(response->operation_code) be equal to (STORAGE_OP_TAG_COMMIT_RES);
            
            int8_t err_code;
            package_read_int8(response, &err_code);
            should_int(err_code) be equal to (FILE_TAG_MISSING);
            
            should_bool(correct_unlock(name, tag)) be truthy;
            
            package_destroy(response);
            package_destroy(request_package);
        } end

        it("Falla la deserializacion (paquete incompleto) y devuelve NULL") {
            // Paquete incompleto que falla al leer el tag
            t_package *request_package = package_create_empty(STORAGE_OP_TAG_COMMIT_REQ);
            package_add_uint32(request_package, (uint32_t)302);
            package_add_string(request_package, "file_ok");
            package_simulate_reception(request_package);

            t_package *response = handle_tag_commit_request(request_package);

            should_ptr(response) be null;

            package_destroy(request_package);
        } end
    } end
}