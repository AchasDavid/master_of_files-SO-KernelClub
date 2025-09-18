#ifndef MAIN_H
#define MAIN_H

#include <commons/log.h>

/**
 * Borra todo el contenido del directorio de montaje del filesystem
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int wipe_storage_dir(const char* mount_point, t_log* logger);

/**
 * Arma el archivo superblock.config con toda la config del filesystem
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_superblock(const char* mount_point, t_log* logger);

/**
 * Crea el bitmap que dice qué bloques están libres o ocupados
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si no puede abrir el archivo, -2 si no hay memoria
 */
int init_bitmap(const char* mount_point, t_log* logger);

/**
 * Arma el archivo de hashes de bloques para no tener bloques duplicados
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_blocks_index(const char* mount_point, t_log* logger);

/**
 * Crea el directorio de bloques físicos y todos los archivos de bloques
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si no puede crear el directorio, -2 si fallan los bloques, -3 si no hay memoria
 */
int init_physical_blocks(const char* mount_point, t_log* logger);

/**
 * Arma toda la estructura de archivos con el archivo inicial y sus metadatos
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, números negativos (-1 a -6) si se rompe algo
 */
int init_files(const char* mount_point, t_log* logger);

/**
 * Monta el filesystem completo ejecutando todas las funciones de inicialización
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, números negativos (-1 a -6) que te dicen qué función se rompió
 */
int init_storage(const char* mount_point, t_log* logger);

#endif
