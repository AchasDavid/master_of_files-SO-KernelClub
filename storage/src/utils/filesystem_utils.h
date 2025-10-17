#ifndef FILESYSTEM_UTILS_H
#define FILESYSTEM_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_DIR_PERMISSIONS 0755
#define FILES_DIR "files"
#define LOGICAL_BLOCKS_DIR "logical_blocks"
#define PHYSICAL_BLOCKS_DIR "physical_blocks"
#define METADATA_CONFIG_FILE "metadata.config"

/**
 * Crea una carpeta recursivamente con mkdir -p
 *
 * @param path Path completo de la carpeta a crear
 * @return 0 en caso de éxito, -1 si falla la creación
 */
int create_dir_recursive(const char *path);

/**
 * Crea toda la estructura de carpetas para un archivo con tag
 * Crea: mount_point/files/file_name/tag/logical_blocks/
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo
 * @return 0 en caso de éxito, números negativos si falla algún paso
 */
int create_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag);

/**
 * Elimina toda la estructura de carpetas para un archivo con tag
 * Elimina recursivamente: mount_point/files/file_name/tag/
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo a eliminar
 * @return 0 en caso de éxito, -1 si falla
 */
int delete_file_dir_structure(const char *mount_point, const char *file_name,
                              const char *tag);

/**
 * Crea un archivo metadata.config con contenido inicial
 *
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param file_name Nombre del archivo
 * @param tag Tag del archivo
 * @param initial_content Contenido inicial del metadata (puede ser NULL para
 * contenido por defecto)
 * @return 0 en caso de éxito, -1 si no se puede crear el archivo
 */
int create_metadata_file(const char *mount_point, const char *file_name,
                         const char *tag, const char *initial_content);

#endif
