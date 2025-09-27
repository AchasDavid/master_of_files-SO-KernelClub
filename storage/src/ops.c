#include "ops.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/**
 * Crea un nuevo File con el Tag especificado en el filesystem
 * 
 * @param name Nombre del File a crear
 * @param tag Tag inicial del File
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para registrar errores y operaciones
 * @return 0 en caso de éxito, -1 si falla la creación de carpetas, -2 si falla la creación de metadata
 */
int create_file(const char* name, const char* tag, const char* mount_point, t_log* logger) {
    char target_path[PATH_MAX];

    // Crear carpeta del File (no falla si ya existe)
    snprintf(target_path, sizeof(target_path), "%s/files/%s", mount_point, name);
    mkdir(target_path, 0755); // Ignoramos el error si ya existe

    // Crear carpeta del Tag
    snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point, name, tag);
    if (mkdir(target_path, 0755) != 0) goto file_creation_error;

    // Crear carpeta de bloques lógicos
    snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/logical_blocks", mount_point, name, tag);
    if (mkdir(target_path, 0755) != 0) goto file_creation_error;

    // Crear archivo de metadata
    snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/metadata.config", mount_point, name, tag);
    FILE* metadata_ptr = fopen(target_path, "w");
    if (metadata_ptr == NULL) {
        log_error(logger, "No se pudo crear el archivo %s", target_path);
        return -2;
    }

    fprintf(metadata_ptr, "SIZE=0\nBLOCKS=[]\nESTADO=WORK_IN_PROGRESS\n");
    log_info(logger, "Archivo de metadata creado en %s", target_path);

    fclose(metadata_ptr);

    return 0;

file_creation_error:
    log_error(logger, "Error al crear el archivo %s con tag %s", name, tag);
    return -1;
}
