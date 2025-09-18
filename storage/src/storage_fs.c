#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/log.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONST_FS_SIZE 4096
#define CONST_BLOCK_SIZE 128

/**
 * Borra todo el contenido de la carpeta de montaje del filesystem
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int wipe_storage_dir(const char* mount_point, t_log* logger) {
    struct stat st;
    if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error(logger, "La carpeta %s no existe o no es una carpeta", mount_point);
        return -1;
    }

    char command[PATH_MAX + 10];
    snprintf(command, sizeof(command), "rm -rf %s/*", mount_point);

    if (system(command) != 0) {
        log_error(logger, "No se pudo limpiar el directorio %s", mount_point);
        return -1;
    }

    log_info(logger, "Directorio %s limpiado exitosamente", mount_point);
    return 0;
}

/**
 * Arma el archivo superblock.config con toda la config del filesystem
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_superblock(const char* mount_point, t_log* logger) {
    char superblock_path[PATH_MAX];
    snprintf(superblock_path, sizeof(superblock_path), "%s/superblock.config", mount_point);

    FILE* superblock_ptr = fopen(superblock_path, "w");
    if (superblock_ptr == NULL) {
        log_error(logger, "No se pudo abrir %s para escritura", superblock_path);
        return -1;
    }

    fprintf(superblock_ptr, "FS_SIZE=%d\nBLOCK_SIZE=%d\n", CONST_FS_SIZE, CONST_BLOCK_SIZE);
    fclose(superblock_ptr);

    log_info(logger, "Superblock creado en %s", superblock_path);
    return 0;
}

/**
 * Crea el bitmap que dice qué bloques están libres o ocupados
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si no puede abrir el archivo, -2 si no hay memoria
 */
int init_bitmap(const char* mount_point, t_log* logger) {
    struct stat st;
    if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error(logger, "El directorio %s no existe o no es un directorio", mount_point);
        return -1;
    }

    int total_blocks = CONST_FS_SIZE / CONST_BLOCK_SIZE;
    size_t bitmap_size_bytes = (total_blocks + 7) / 8; // Round up to next byte

    char bitmap_path[PATH_MAX];
    snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", mount_point);

    FILE* bitmap_file = fopen(bitmap_path, "wb");
    if (bitmap_file == NULL) {
        log_error(logger, "No se pudo crear el archivo bitmap en %s", bitmap_path);
        return -1;
    }

    // Inicializado en ceros
    char* zero_buffer = calloc(1, bitmap_size_bytes);
    if (zero_buffer == NULL) {
        log_error(logger, "No se pudo asignar memoria para el bitmap");
        fclose(bitmap_file);
        return -2;
    }

    fwrite(zero_buffer, 1, bitmap_size_bytes, bitmap_file);
    fclose(bitmap_file);

    t_bitarray* bitmap = bitarray_create_with_mode(zero_buffer, bitmap_size_bytes, MSB_FIRST);
    if (bitmap == NULL) {
        log_error(logger, "No se pudo crear el bitmap en memoria");
        free(zero_buffer);
        return -1;
    }

    // El primer bloque siempre está ocupado por el archivo inicial
    bitarray_set_bit(bitmap, 0);

    bitmap_file = fopen(bitmap_path, "wb");
    if (bitmap_file != NULL) {
        fwrite(zero_buffer, 1, bitmap_size_bytes, bitmap_file);
        fclose(bitmap_file);
    }

    bitarray_destroy(bitmap);
    free(zero_buffer);

    log_info(logger, "Bitmap inicializado en %s con %d bloques", bitmap_path, total_blocks);
    return 0;
}

/**
 * Arma el archivo de hashes de bloques para no tener bloques duplicados
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int init_blocks_index(const char* mount_point, t_log* logger) {
    char blocks_path[PATH_MAX];
    snprintf(blocks_path, sizeof(blocks_path), "%s/blocks_hash_index.config", mount_point);

    FILE* blocks_ptr = fopen(blocks_path, "wb");
    if (blocks_ptr == NULL) {
        log_error(logger, "No se pudo crear el archivo %s", blocks_path);
        return -1;
    }

    fclose(blocks_ptr);

    log_info(logger, "Índice de bloques creado en %s", blocks_path);
    return 0;
}

/**
 * Crea el directorio de bloques físicos y todos los archivos de bloques
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si no puede crear el directorio, -2 si fallan los bloques, -3 si no hay memoria
 */
int init_physical_blocks(const char* mount_point, t_log* logger) {
    char physical_blocks_dir_path[PATH_MAX];
    snprintf(physical_blocks_dir_path, sizeof(physical_blocks_dir_path), "%s/physical_blocks", mount_point);

    if (mkdir(physical_blocks_dir_path, 0755) != 0) {
        log_error(logger, "No se pudo crear el directorio %s", physical_blocks_dir_path);
        return -1;
    }

    int total_blocks = CONST_FS_SIZE / CONST_BLOCK_SIZE;
    char new_block_path[PATH_MAX];
    void* zero_buffer = calloc(1, CONST_BLOCK_SIZE);
    if (zero_buffer == NULL) {
        log_error(logger, "No se pudo asignar memoria para los bloques");
        return -3;
    }

    for (int i = 0; i < total_blocks; i++) {
        // Desactiva el warning de GCC sobre truncamiento de snprintf
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(new_block_path, sizeof(new_block_path), "%s/block%04d.dat", physical_blocks_dir_path, i);
        #pragma GCC diagnostic pop

        FILE* block_ptr = fopen(new_block_path, "wb");
        if (block_ptr == NULL) {
            log_error(logger, "No se pudo crear el archivo de bloque %s", new_block_path);
            free(zero_buffer);
            return -2;
        }

        fwrite(zero_buffer, 1, CONST_BLOCK_SIZE, block_ptr);

        fclose(block_ptr);
    }

    free(zero_buffer);

    log_info(logger, "Creados %d bloques físicos en %s", total_blocks, physical_blocks_dir_path);
    return 0;
}

/**
 * Arma toda la estructura de archivos con el archivo inicial y sus metadatos
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, números negativos (-1 a -6) si se rompe algo
 */
int init_files(const char* mount_point, t_log* logger) {
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];

    snprintf(target_path, PATH_MAX, "%s/files", mount_point);
    if (mkdir(target_path, 0755) != 0) {
        log_error(logger, "No se pudo crear el directorio %s", target_path);
        return -1;
    }
    log_info(logger, "Directorio files creado en %s", target_path);

    snprintf(target_path, PATH_MAX, "%s/files/initial_file", mount_point);
    if (mkdir(target_path, 0755) != 0) {
      log_error(logger, "No se pudo crear el directorio %s", target_path);
      return -2;
    }

    snprintf(target_path, PATH_MAX, "%s/files/initial_file/BASE", mount_point);
    if (mkdir(target_path, 0755) != 0) {
      log_error(logger, "No se pudo crear el directorio %s", target_path);
      return -3;
    }

    snprintf(target_path, PATH_MAX, "%s/files/initial_file/BASE/logical_blocks", mount_point);
    if (mkdir(target_path, 0755) != 0) {
      log_error(logger, "No se pudo crear el directorio %s", target_path);
      return -4;
    }

    snprintf(source_path, PATH_MAX, "%s/physical_blocks/block0000.dat", mount_point);
    snprintf(target_path, PATH_MAX, "%s/files/initial_file/BASE/logical_blocks/block0000.dat", mount_point);
    if (link(source_path, target_path) != 0) {
      log_error(logger, "No se pudo crear el hard link %s -> %s", source_path, target_path);
      return -5;
    }

    snprintf(target_path, PATH_MAX, "%s/files/initial_file/BASE/metadata.config", mount_point);
    FILE* metadata_ptr = fopen(target_path, "w");
    if (metadata_ptr == NULL) {
      log_error(logger, "No se pudo crear el archivo %s", target_path);
      return -6;
    }

    fprintf(metadata_ptr, "SIZE=0\nBLOCKS=[0]\nESTADO=COMMITTED\n");
    log_info(logger, "Archivo inicial creado en %s", target_path);

    fclose(metadata_ptr);

    return 0;
}

/**
 * Monta el filesystem completo ejecutando todas las funciones de inicialización
 * 
 * @param mount_point Path de la carpeta donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, números negativos (-1 a -6) que te dicen qué función se rompió
 */
int init_storage(const char* mount_point, t_log* logger) {
    if (wipe_storage_dir(mount_point, logger) != 0) return -1;
    if (init_superblock(mount_point, logger) != 0) return -2;
    if (init_bitmap(mount_point, logger) != 0) return -3;
    if (init_blocks_index(mount_point, logger) != 0) return -4;
    if (init_physical_blocks(mount_point, logger) != 0) return -5;
    if (init_files(mount_point, logger) != 0) return -6;

    return 0;
}
