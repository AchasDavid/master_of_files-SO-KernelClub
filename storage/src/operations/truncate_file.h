#ifndef STORAGE_OPERATIONS_TRUNCATE_FILE_H_
#define STORAGE_OPERATIONS_TRUNCATE_FILE_H_

#include "connection/protocol.h"
#include "connection/serialization.h"
#include "globals/globals.h"
#include <stdint.h>

/**
 * Handler de protocolo para la operación TRUNCATE_FILE
 * Deserializa el package recibido y trunca el archivo especificado
 *
 * @param package Package recibido con query_id, nombre de archivo, tag y nuevo
 * tamaño
 * @return t_package* Package de respuesta con resultado de la operación, o NULL
 * en caso de error
 */
t_package *handle_truncate_file_op_package(t_package *package);

/**
 * Trunca un archivo existente a un tamaño nuevo
 * Si el tamaño nuevo es menor, libera bloques sobrantes
 * Si el tamaño nuevo es mayor, asigna nuevos bloques apuntando al bloque 0
 *
 * @param query_id ID de la query para logging
 * @param name Nombre del archivo a truncar
 * @param tag Tag del archivo a truncar
 * @param new_size_bytes Nuevo tamaño del archivo en bytes
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @return 0 en caso de éxito, valores negativos en caso de error
 *         -1: Error al abrir superblock.config
 *         -2: Error al abrir metadata.config
 *         -3: Error al eliminar bloques lógicos
 *         -4: Error al crear hard links para expansión
 */
int truncate_file(uint32_t query_id, const char *name, const char *tag,
                  int new_size_bytes, const char *mount_point);

/**
 * Maneja un bloque físico que potencialmente quedó huérfano
 * y marca su bit en el bitmap como libre de ser el caso
 *
 * @param physical_block_path Path completo al bloque físico
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param query_id ID de la query para logging
 * @return 0 en caso de éxito, valores negativos en caso de error
 *         -1: Error al obtener estado del bloque
 *         -2: Error al parsear número de bloque
 *         -3: Error al liberar el bloque en el bitmap
 */
int maybe_handle_orphaned_physical_block(const char *physical_block_path,
                                         const char *mount_point,
                                         uint32_t query_id);

#endif
