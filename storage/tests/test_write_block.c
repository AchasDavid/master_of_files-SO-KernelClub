#include <connection/serialization.h>
#include <connection/protocol.h>
#include <operations/write_block.h>
#include <cspecs/cspec.h>

context(tests_write_block) {
    
    describe("Deserialización de datos recibidos de cliente") {        
        uint32_t query_id;
        char *name = NULL;
        char *tag = NULL;
        uint32_t block_number;
        char *block_content = NULL;

        t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);

        it("Deserializa correctamente todos los datos") {
            package_add_uint32(package, 12);
            package_add_string(package, "file");
            package_add_string(package, "tag1");
            package_add_uint32(package, 21);
            package_add_string(package, "TEXTO");

            int retval = deserialize_block_write_request(package, &query_id, &name, &tag, &block_number, &block_content);
            should_int(retval) be equal to (0);
            free(package);
        } end

        it("Error al deserializar query_id") {
            int retval = deserialize_block_write_request(package, &query_id, &name, &tag, &block_number, &block_content);
            should_int(retval) be equal to (-1);
            free(package);
        } end

        it("Error al deserializar name") {
            package_add_uint32(package, 12);

            int retval = deserialize_block_write_request(package, &query_id, &name, &tag, &block_number, &block_content);
            should_int(retval) be equal to (-1);
            free(package);
        } end
    } end
} 
