#include <commons/bitarray.h>
#include <commons/config.h>
#include <commons/log.h>
#include <config/storage_config.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utils/client_socket.h>
#include <utils/server.h>
#include <utils/utils.h>

#define MODULO "STORAGE"
#define CONST_FS_SIZE 4096
#define CONST_BLOCK_SIZE 128

t_storage_config* g_storage_config;
t_log* g_storage_logger;

/**
 * Borra todo el contenido del directorio de montaje del filesystem
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si se rompe
 */
int wipe_storage_dir(const char* mount_point, t_log* logger) {
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
 * @param mount_point Ruta del directorio donde está montado el filesystem
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
 * @param mount_point Ruta del directorio donde está montado el filesystem
 * @param logger Logger para loggear errores y operaciones
 * @return 0 en caso de exito, -1 si no puede abrir el archivo, -2 si no hay memoria
 */
int init_bitmap(const char* mount_point, t_log* logger) {
    int total_blocks = CONST_FS_SIZE / CONST_BLOCK_SIZE;

    char bitmap_path[PATH_MAX];
    snprintf(bitmap_path, sizeof(bitmap_path), "%s/bitmap.bin", mount_point);

    t_bitarray* bitmap = bitarray_create_with_mode(bitmap_path, total_blocks, MSB_FIRST);
    if (bitmap == NULL) {
        log_error(logger, "No se pudo crear el bitmap en %s", bitmap_path);
        return -1;
    }

    // Inicializa todos los bits en 0 (bloques libres)
    for (int i = 0; i < total_blocks; i++) {
        bitarray_clean_bit(bitmap, i);
    }

    // El primer bloque siempre está ocupado por el archivo inicial
    bitarray_set_bit(bitmap, 0);

    bitarray_destroy(bitmap);

    log_info(logger, "Bitmap inicializado en %s con %d bloques", bitmap_path, total_blocks);
    return 0;
}

/**
 * Arma el archivo de hashes de bloques para no tener bloques duplicados
 * 
 * @param mount_point Ruta del directorio donde está montado el filesystem
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
 * @param mount_point Ruta del directorio donde está montado el filesystem
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
 * @param mount_point Ruta del directorio donde está montado el filesystem
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
 * @param mount_point Ruta del directorio donde está montado el filesystem
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

int main(int argc, char* argv[]) {
    char config_file_path[PATH_MAX];
    int retval = 0;

    if (argc == 1) {
        strncpy(config_file_path, "./src/config/storage.config", PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else if (argc == 2) {
        strncpy(config_file_path, argv[1], PATH_MAX - 1);
        config_file_path[PATH_MAX - 1] = '\0';
    } else {
        fprintf(stderr, "Solo se puede ingresar el argumento [archivo_config]\n");
        retval = -1;
        goto error;
    }

    g_storage_config = create_storage_config(config_file_path);
    if (g_storage_config == NULL) {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        retval = -2;
        goto error;
    }

    char current_directory[PATH_MAX];
    if (getcwd(current_directory, sizeof(current_directory)) == NULL) {
        fprintf(stderr, "Error al obtener el directorio actual\n");
        retval = -3;
        goto clean_config;
    }

    g_storage_logger = create_logger(current_directory, MODULO, true, g_storage_config->log_level);
    if (g_storage_logger == NULL) {
        fprintf(stderr, "Error al crear el logger\n");
        retval = -4;
        goto clean_config;
    }

    log_debug(g_storage_logger,
              "Configuracion leida: \\n\\tPUERTO_ESCUCHA=%d\\n\\tFRESH_START=%s\\n\\tPUNTO_MONTAJE=%s\\n\\tRETARDO_OPERACION=%d\\n\\tRETARDO_ACCESO_BLOQUE=%d\\n\\tLOG_LEVEL=%s",
              g_storage_config->puerto_escucha,
              g_storage_config->fresh_start ? "TRUE" : "FALSE",
              g_storage_config->punto_montaje,
              g_storage_config->retardo_operacion,
              g_storage_config->retardo_acceso_bloque,
              log_level_as_string(g_storage_config->log_level));

    if (g_storage_config->fresh_start) {
        log_info(g_storage_logger, "Iniciando en modo FRESH_START, se eliminará el contenido previo en %s",
                g_storage_config->punto_montaje);

        int init_result = init_storage(g_storage_config->punto_montaje, g_storage_logger);
        if (init_result != 0) {
            log_error(g_storage_logger, "Error al inicializar el filesystem: %d", init_result);
            retval = -6;
            goto clean_logger;
        }

        log_info(g_storage_logger, "Filesystem inicializado exitosamente en %s", g_storage_config->punto_montaje);
    }

    char puerto_str[16];
    snprintf(puerto_str, sizeof(puerto_str), "%d", g_storage_config->puerto_escucha);

    int socket = start_server("127.0.0.1", puerto_str);
    if (socket < 0) {
        log_error(g_storage_logger, "No se pudo iniciar el servidor en el puerto %d", g_storage_config->puerto_escucha);
        retval = -5;
        goto clean_logger;
    }
    log_info(g_storage_logger, "Servidor iniciado en el puerto %d", g_storage_config->puerto_escucha);

    close(socket);
    log_destroy(g_storage_logger);
    destroy_storage_config(g_storage_config);
    exit(EXIT_SUCCESS);

clean_logger:
    log_destroy(g_storage_logger);
clean_config:
    destroy_storage_config(g_storage_config);
error:
    return retval;
}
