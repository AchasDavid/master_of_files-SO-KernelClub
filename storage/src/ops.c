#include "ops.h"
#include "../../utils/src/utils/utils.h"
#include "storage_utils.h"
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
                t_log *logger) {
  char target_path[PATH_MAX];

  // Crear carpeta del File (no falla si ya existe)
  snprintf(target_path, sizeof(target_path), "%s/files/%s", mount_point, name);
  mkdir(target_path, 0755); // Ignoramos el error si ya existe

  // Crear carpeta del Tag
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s", mount_point,
           name, tag);
  if (mkdir(target_path, 0755) != 0)
    goto file_creation_error;

  // Crear carpeta de bloques lógicos
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/logical_blocks",
           mount_point, name, tag);
  if (mkdir(target_path, 0755) != 0)
    goto file_creation_error;

  // Crear archivo de metadata
  snprintf(target_path, sizeof(target_path), "%s/files/%s/%s/metadata.config",
           mount_point, name, tag);
  FILE *metadata_ptr = fopen(target_path, "w");
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

int maybe_handle_orphaned_physical_block(const char *physical_block_path,
                                         const char *mount_point, int query_id,
                                         t_log *logger) {
  struct stat statbuf;

  if (stat(physical_block_path, &statbuf) != 0) {
    log_error(logger, "No se pudo obtener el estado del bloque físico %s",
              physical_block_path);
    return -1;
  }
  if (statbuf.st_nlink > 1) {
    log_info(logger,
             "El bloque físico %s todavía tiene %lu hard links, no se libera",
             physical_block_path, statbuf.st_nlink);
    return 0;
  }

  // Extraer el número de bloque del path
  int block_number;

  // Obtener solo el nombre del archivo
  const char *filename = strrchr(physical_block_path, '/');
  if (filename == NULL) {
    filename =
        physical_block_path; // Si no hay '/' el path completo es el filename
  } else {
    filename++; // Saltea el '/'
  }

  // Validar que el nombre del archivo siga el formato "blockXXXX.dat"
  if (sscanf(filename, "block%d.dat", &block_number) != 1) {
    log_error(logger, "No se pudo parsear el número de bloque de %s",
              physical_block_path);
    return -2;
  }

  // Unsetear el bit del bloque en el bitmap
  int result = modify_bitmap_bits(mount_point, &block_number, 1, 0);
  if (result != 0) {
    log_error(logger, "No se pudo liberar el bloque %d en el bitmap",
              block_number);
    return -3;
  }

  log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d",
           query_id, block_number);
  log_info(logger, "Bloque físico %s liberado exitosamente",
           physical_block_path);
  return 0;
}

int truncate_file(const char *name, const char *tag, const int new_size_bytes,
                  const char *mount_point, t_log *logger) {
  int retval = 0;

  char storage_config_path[PATH_MAX];
  snprintf(storage_config_path, sizeof(storage_config_path),
           "%s/superblock.config", mount_point);
  t_config *superblock_config = config_create(storage_config_path);
  if (!superblock_config) {
    log_error(logger, "No se pudo abrir el config: %s", storage_config_path);
    retval = -1;
    goto end;
  }

  int block_size = config_get_int_value(superblock_config, "BLOCK_SIZE");

  char metadata_path[PATH_MAX];
  snprintf(metadata_path, sizeof(metadata_path),
           "%s/files/%s/%s/metadata.config", mount_point, name, tag);
  t_config *metadata_config = config_create(metadata_path);
  if (!metadata_config) {
    log_error(logger, "No se pudo abrir el config: %s", metadata_path);
    retval = -2;
    goto clean_superblock_config;
  }

  char **old_blocks = config_get_array_value(metadata_config, "BLOCKS");
  int old_block_count = string_array_size(old_blocks);
  int new_block_count =
      (new_size_bytes + block_size - 1) / block_size; // Redondeo hacia arriba

  if (new_block_count == old_block_count) {
    log_info(logger, "El nuevo tamaño encaja en la misma cantidad de bloques, "
                     "no se requiere truncado.");
    goto clean_metadata_config;
  }

  char **new_blocks = string_array_new();
  char target_path[PATH_MAX];
  char resolved_path[PATH_MAX];

  if (new_block_count < old_block_count) {
    // Truncar: liberar bloques sobrantes
    for (int i = new_block_count; i < old_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%d.bin", mount_point, name, tag,
               i);

      if (access(target_path, F_OK) != 0) {
        log_info(logger, "Bloque lógico %d no existía en %s", i, target_path);
        continue;
      }

      if (realpath(target_path, resolved_path) == NULL) {
        log_error(logger, "No se pudo resolver el path %s", target_path);
        retval = -3;
        goto clean_new_blocks;
      }

      if (remove(target_path) != 0) {
        log_error(logger, "No se pudo eliminar el bloque lógico %d en %s", i,
                  target_path);
        retval = -3;
        goto clean_new_blocks;
      }

      maybe_handle_orphaned_physical_block(resolved_path, mount_point, 0,
                                           logger);
    }

    for (int i = 0; i < new_block_count; i++) {
      string_array_push(&new_blocks, string_duplicate(old_blocks[i]));
    }
  } else {
    // Expandir: asignar nuevos bloques
    char physical_block_zero_path[PATH_MAX];
    snprintf(physical_block_zero_path, sizeof(physical_block_zero_path),
             "%s/physical_blocks/block0000.dat", mount_point);

    for (int i = old_block_count; i < new_block_count; i++) {
      snprintf(target_path, sizeof(target_path),
               "%s/files/%s/%s/logical_blocks/%d.bin", mount_point, name, tag,
               i);
      if (link(physical_block_zero_path, target_path) != 0) {
        log_error(logger, "No se pudo crear hard link de %s a %s",
                  physical_block_zero_path, target_path);
        retval = -4;
        goto clean_new_blocks;
      }
    }

    for (int i = 0; i < old_block_count; i++)
      string_array_push(&new_blocks, string_duplicate(old_blocks[i]));
    for (int i = old_block_count; i < new_block_count; i++)
      string_array_push(&new_blocks, string_duplicate("0"));
  }

  char *stringified_blocks = get_stringified_array(new_blocks);
  config_set_value(metadata_config, "BLOCKS", stringified_blocks);

  char size_str[32];
  snprintf(size_str, sizeof(size_str), "%d", new_size_bytes);
  config_set_value(metadata_config, "SIZE", size_str);

  config_save(metadata_config);
  free(stringified_blocks);

clean_new_blocks:
  if (new_blocks)
    string_array_destroy(new_blocks);
clean_metadata_config:
  config_destroy(metadata_config);
clean_superblock_config:
  config_destroy(superblock_config);
end:
  return retval;
}
