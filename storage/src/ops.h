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
 * @return 0 en caso de éxito, -1 si falla la creación de carpetas, -2 si falla la creación de metadata
 */
int create_file(const char* name, const char* tag, const char* mount_point, t_log* logger);

#endif
