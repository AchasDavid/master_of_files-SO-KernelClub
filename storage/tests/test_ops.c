#include <cspecs/cspec.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include "../src/ops.h"
#include "test_utils.h"

context(test_ops) {
    describe("create_file op") {
        t_log* test_logger;

        before {
            create_test_directory();
            test_logger = create_test_logger();

            // Crear estructura básica del filesystem para las pruebas
            char files_dir[PATH_MAX];
            snprintf(files_dir, sizeof(files_dir), "%s/files", TEST_MOUNT_POINT);
            mkdir(files_dir, 0755);
        } end

        after {
            destroy_test_logger(test_logger);
            cleanup_test_directory();
        } end

        it("crea archivo nuevo con tag correctamente") {
            int result = create_file("test_file", "v1", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            // Verificar estructura de carpetas creada
            char file_dir[PATH_MAX];
            snprintf(file_dir, sizeof(file_dir), "%s/files/test_file", TEST_MOUNT_POINT);
            should_bool(directory_exists(file_dir)) be truthy;

            char tag_dir[PATH_MAX];
            snprintf(tag_dir, sizeof(tag_dir), "%s/files/test_file/v1", TEST_MOUNT_POINT);
            should_bool(directory_exists(tag_dir)) be truthy;

            char logical_blocks_dir[PATH_MAX];
            snprintf(logical_blocks_dir, sizeof(logical_blocks_dir), "%s/files/test_file/v1/logical_blocks", TEST_MOUNT_POINT);
            should_bool(directory_exists(logical_blocks_dir)) be truthy;

            // Verificar archivo de metadata
            char metadata_file[PATH_MAX];
            snprintf(metadata_file, sizeof(metadata_file), "%s/files/test_file/v1/metadata.config", TEST_MOUNT_POINT);
            should_bool(file_exists(metadata_file)) be truthy;

            // Verificar contenido de metadata
            char metadata_content[256];
            read_file_contents(metadata_file, metadata_content, sizeof(metadata_content));
            should_ptr(strstr(metadata_content, "SIZE=0")) not be null;
            should_ptr(strstr(metadata_content, "BLOCKS=[]")) not be null;
            should_ptr(strstr(metadata_content, "ESTADO=WORK_IN_PROGRESS")) not be null;
        } end

        it("retorna error si ya existe la carpeta del archivo con el mismo tag") {
            create_file("existing_file", "existing_tag", TEST_MOUNT_POINT, test_logger);

            int result = create_file("existing_file", "existing_tag", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(-1);
        } end

        it("crea multiples tags para el mismo archivo") {
            create_file("multi_tag_file", "tag1", TEST_MOUNT_POINT, test_logger);
            int result = create_file("multi_tag_file", "tag2", TEST_MOUNT_POINT, test_logger);

            should_int(result) be equal to(0);

            char tag1_dir[PATH_MAX], tag2_dir[PATH_MAX];
            snprintf(tag1_dir, sizeof(tag1_dir), "%s/files/multi_tag_file/tag1", TEST_MOUNT_POINT);
            snprintf(tag2_dir, sizeof(tag2_dir), "%s/files/multi_tag_file/tag2", TEST_MOUNT_POINT);

            should_bool(directory_exists(tag1_dir)) be truthy;
            should_bool(directory_exists(tag2_dir)) be truthy;
        } end
    } end
}
