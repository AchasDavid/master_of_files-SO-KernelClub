#include <connection/serialization.h>
#include <connection/protocol.h>
#include <cspecs/cspec.h>

context(tests_write_block) {
    
    describe("Deserialización de datos recibidos de cliente") {
        
        it("Deserializa correctamente todos los datos") {
            t_package *package = package_create_empty(STORAGE_OP_BLOCK_WRITE_REQ);
            
            should_int(package->operation_code) be equal to (STORAGE_OP_BLOCK_WRITE_REQ);
            free(package);
        } end
    } end
} 
