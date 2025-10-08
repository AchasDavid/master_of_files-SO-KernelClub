#ifndef OPS_H
#define OPS_H

#include <commons/log.h>

/**
 * Crea un nuevo File con el Tag especificado en el filesystem
 *
 * @param name Nombre del File a crear
 * @param tag Tag inicial del File
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para registrar errores y operaciones
 * @return 0 en caso de éxito, -1 si falla la creación de carpetas, -2 si falla
 * la creación de metadata
 */
int create_file(const char *name, const char *tag, const char *mount_point,
                t_log *logger);

/**
 * Modifica el tamaño del File:Tag especificado
 *
 * @param name Nombre del File a truncar
 * @param tag Tag del File a truncar
 * @param new_size_bytes Nuevo tamaño en bytes del File
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para registrar errores y operaciones
 * @return 0 en caso de éxito, -1 si falla abrir superblock config, -2 si falla
 * abrir metadata config, -3 si falla eliminar bloque lógico, -4 si falla crear
 * hard link, -5 si falla asignación de memoria
 */
int truncate_file(const char *name, const char *tag, const int new_size_bytes,
                  const char *mount_point, t_log *logger);

/**
 * Handlea un bloque físico huérfano liberando su bit en el bitmap si no tiene
 * referencias desde bloques lógicos
 *
 * @param physical_block_path Path del bloque físico a verificar
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param query_id ID numérico de la query para logging
 * @param logger Logger para registrar errores y operaciones
 * @return 0 en caso de éxito, -1 si falla obtener el estado, -2 si falla
 * parsear el nombre, -3 si falla abrir superblock, -4 si falla leer bitmap, -5
 * si falla escribir bitmap
 */
int maybe_handle_orphaned_physical_block(const char *physical_block_path,
                                         const char *mount_point,
                                         int query_id,
                                         t_log *logger);

#endif
