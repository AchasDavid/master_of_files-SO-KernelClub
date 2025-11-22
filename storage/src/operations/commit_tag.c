#include "commit_tag.h"

t_package *handle_tag_commit_request(t_package *package) {
  uint32_t query_id;
  char *name = NULL;
  char *tag = NULL;

  if (deserialize_tag_commit_request(package, &query_id, &name, &tag) < 0) {
    return NULL;
  }

  int operation_result = execute_tag_commit(query_id, name, tag);

  free(name);
  free(tag);

  t_package *response = package_create_empty(STORAGE_OP_TAG_COMMIT_RES);
  if (!response) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Fallo al crear paquete de respuesta.",
              query_id);
    return NULL;
  }

  if (!package_add_int8(response, (int8_t)operation_result)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error al escribir status en respuesta de COMMIT TAG",
              query_id);
    package_destroy(response);
    return NULL;
  }

  package_reset_read_offset(response);

  return response;
}

int deserialize_tag_commit_request(t_package *package, uint32_t *query_id,
                                   char **name, char **tag) {
  int retval = 0;

  if (!package_read_uint32(package, query_id)) {
    log_error(g_storage_logger,
              "## Error al deserializar query_id de COMMIT_TAG");
    retval = -1;
    goto end;
  }

  *name = package_read_string(package);
  if (*name == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - Error al deserializar el nombre del "
              "file para la operación COMMIT_TAG",
              *query_id);
    retval = -2;
    goto end;
  }

  *tag = package_read_string(package);
  if (*tag == NULL) {
    log_error(
        g_storage_logger,
        "## Query ID: %" PRIu32
        " - Error al deserializar el tag del file para la operación COMMIT_TAG",
        *query_id);
    retval = -3;
    goto clean_name_tag;
  }

  return retval;

clean_name_tag:
  if (*tag) {
    free(*tag);
    *tag = NULL;
  }
  if (*name) {
    free(*name);
    *name = NULL;
  }
end:
  return retval;
}

int execute_tag_commit(uint32_t query_id, const char *name, const char *tag) {
  int retval = 0;

  lock_file(name, tag);

  if (!file_dir_exists(name, tag)) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32 " - El directorio %s:%s no existe.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  t_file_metadata *metadata =
      read_file_metadata(g_storage_config->mount_point, name, tag);
  if (metadata == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo leer el metadata de %s:%s.",
              query_id, name, tag);
    retval = FILE_TAG_MISSING;
    goto cleanup_unlock;
  }

  if (strcmp(metadata->state, COMMITTED) == 0) {
    log_info(g_storage_logger,
             "## Query ID: %" PRIu32 " - El archivo %s:%s ya está en estado "
             "'COMMITTED'.",
             query_id, name, tag);
    goto cleanup_metadata;
  }

  deduplicate_blocks(query_id, name, tag, metadata);

  free(metadata->state);
  metadata->state = strdup(COMMITTED);
  if (!metadata->state) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error de asignación de memoria al duplicar estado COMMITTED.",
              query_id);
    retval = -1;
    goto cleanup_metadata;
  }

  if (save_file_metadata(metadata) < 0) {
    log_error(
        g_storage_logger,
        "## Query ID: %" PRIu32
        " - No se pudo actualizar el estado de metadata de %s:%s a COMMITTED.",
        query_id, name, tag);
    retval = -2;
    goto cleanup_metadata;
  }

  log_info(g_storage_logger,
           "## Query ID: %" PRIu32 " - Commit de file:tag %s:%s.", query_id,
           name, tag);

cleanup_metadata:
  if (metadata)
    destroy_file_metadata(metadata);
cleanup_unlock:
  unlock_file(name, tag);

  return retval;
}

int deduplicate_blocks(uint32_t query_id, const char *name, const char *tag,
                       t_file_metadata *metadata) {
  int retval = 0;

  // Verifica que el file:tag tiene bloques lógicos.
  if (metadata->block_count == 0) {
    log_warning(g_storage_logger,
                "## Query ID: %" PRIu32
                " - File %s:%s no tiene bloques lógicos para deduplicar.",
                query_id, name, tag);
    goto end;
  }

  // Carga el config para blocks_hash_index
  pthread_mutex_lock(g_blocks_hash_index_mutex);
  char hash_index_config_path[PATH_MAX];
  snprintf(hash_index_config_path, sizeof(hash_index_config_path),
           "%s/blocks_hash_index.config", storage_config->mount_point);
  t_config *hash_index_config = config_load(hash_index_config);

  if (hash_index_config == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo leer el archivo blocks_hash_index.config",
              query_id);
    retval = -1;
    goto unlock_hash_index;
  }

  // Itera sobre los bloques lógicos del file:tag para deduplicar
  for (int i = 0; i < metadata->block_count; i++) {
    int logical_block = metadata->blocks[i];

    // Lee el contenido del bloque físico asociado al bloque lógico
    char logical_block_path[PATH_MAX];
    snprintf(logical_block_path, sizeof(logical_block_path),
             "%s/files/%s/%s/logical_blocks/%04d.dat",
             g_storage_config->mount_point, name, tag, logical_block);

    char *read_buffer = (char *)malloc(g_storage_config->block_size);
    if (read_buffer == NULL) {
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error de asignación de memoria para leer el bloque: %s",
                query_id, logical_block_path);
      retval = -2;
      goto unlock_hash_index;
    }

    if (read_block_content(query_id, logical_block_path,
                           g_storage_config->block_size, read_buffer) < 0) {
      retval = -3;
      goto cleanup_buffer;
    }

    // Hashea el contenido del bloque leído
    char *hash = crypto_md5(read_buffer, (size_t)g_storage_config->block_size);
    if (hash == NULL) {
      log_error(g_storage_config,
                "## Query ID: %" PRIu32
                " - No se generó el hash para el bloque %s",
                query_id, logical_block_path);
      retval = -3;
      goto cleanup_buffer;
    }

    free(read_buffer);

    // Si blocks_hash_index contiene el hash, elimina el archivo del bloque
    // lógico actual y lo vuelve a crear como hardlink del bloque físico
    // registrado en blocks_hash_index
    if (config_has_property(hash_index_config, hash)) {
      char *physical_block_from_hash =
          strdup(config_get_string_value(hash_index_config, hash));

      char physical_block_from_logical_path[PATH_MAX];
      if (get_current_physical_block(query_id, logical_block_path,
                                     physical_block_from_logical_path) < 0) {
        retval = -4;
        goto cleanup_buffer;
      }

      if (strcmp(physical_block_from_hash, physical_block_from_logical_path) !=
          0) {
        // El hash existe, pero apunta a otro bloque físico. Reasignamos el
        // bloque lógico.
        log_debug(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Bloque %s es DUPLICADO. Reasignando a bloque físico %s.",
                  query_id, logical_block_path, physical_block_from_hash);

        // Esta función DEBE manejar la liberación del bloque físico *anterior*
        // si ya no tiene referencias
        if (update_logical_block_link(query_id, logical_block_path,
                                      physical_block_from_hash) < 0) {
          log_error(g_storage_logger,
                    "## Query ID: %" PRIu32
                    " - Fallo en la reasignación del link para %s.",
                    query_id, logical_block_path);
          retval = -5;
          goto cleanup_buffer;
        }

        if (update_ph_block_status_in_bitmap(
                query_id, physical_block_from_logical_path) < 0) {
          log_error(g_storage_logger,
                    "## Query ID: %" PRIu32
                    " - El bloque físico %s no se pudo actualizar como libre "
                    "en el bitmap.",
                    query_id, physical_block_from_logical_path);
          retval = -6;
          goto cleanup_buffer;
        }

        log_info(g_storage_logger,
                 "## Query ID: %" PRIu32
                 " - %s:%s - Bloque Lógico %s se reasigna de %s a %s",
                 query_id, name, tag, logical_block,
                 physical_block_from_logical_path, physical_block_from_hash);

      } else {
        // El hash ya está en el índice y apunta al bloque físico correcto. No
        // hacer nada.
        log_info(g_storage_logger,
                 "## Query ID: %" PRIu32
                 " - Bloque %s ya está correctamente asociado.",
                 query_id, logical_block_path);
      }
    }
  }

cleanup_buffer:
  if (read_buffer)
    free(read_buffer);
unlock_hash_index:
  if (hash_index_config)
    destroy_config(hash_index_config);
  pthread_mutex_unlock(g_blocks_hash_index_mutex);
end:
  return retval;
}

int read_block_content(uint32_t query_id, const char *logical_block_path,
                       uint32_t block_size, char *read_buffer) {
  int retval = 0;

  FILE *file = fopen(logical_block_path, "rb");
  if (file == NULL) {
    log_error(g_storage_logger,
              "Query ID: %" PRIu32
              " - Error al abrir el archivo de bloque lógico: %s",
              query_id, logical_block_path);
    retval = -1;
    goto end;
  }

  size_t read_bytes = fread(read_buffer, 1, block_size, file);

  if (read_bytes < block_size) {
    if (feof(file)) {
      // Lectura parcial
      memset(read_buffer + read_bytes, 0, block_size - read_bytes);
      log_warning(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Bloque lógico %s leído, pero su tamaño (%zu bytes) era "
                  "menor que el tamaño de bloque estándar (%u). Rellenado con "
                  "ceros para hashing.",
                  query_id, logical_block_path, read_bytes, block_size);
    } else if (ferror(file)) {
      // Error de lectura
      log_error(g_storage_logger,
                "## Query ID: %" PRIu32
                " - Error de lectura en el bloque lógico: %s",
                query_id, logical_block_path);
      retval = -2;
    }
  }

close_file:
  if (file)
    fclose(file);
end:
  return retval;
}

int get_current_physical_block(uint32_t query_id,
                               const char *logical_block_path,
                               char *physical_block_name) {
  int retval = -3;
  struct stat logical_stat;
  struct stat physical_stat;
  DIR *blocks_dir;
  struct dirent *entry;
  char physical_block_path[PATH_MAX];
  char *physical_block_name = NULL;

  // Obtener el i-node del bloque lógico (hard link)
  if (stat(logical_block_path, &logical_stat) < 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Fallo al obtener stat del bloque lógico: %s",
              query_id, logical_block_path);
    return -1;
  }

  // Abrir el directorio que contiene todos los bloques físicos
  char physical_blocks_path[PATH_MAX];
  snprintf(physical_block_path, size_of(physical_blocks_path),
           "%s/physical_blocks/", g_storage_config->mount_point);
  blocks_dir = opendir(physical_block_path);
  if (blocks_dir == NULL) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo abrir el directorio de bloques: %s",
              query_id, physical_blocks_path);
    return -2;
  }

  // Iterar sobre todos los archivos en el directorio de bloques físicos
  while ((entry = readdir(blocks_dir)) != NULL) {
    // Ignorar "." y ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Construir la ruta completa al bloque físico actual
    snprintf(physical_block_path, sizeof(physical_block_path), "%s/%s",
             physical_blocks_path, entry->d_name);

    // Obtener el stat del archivo físico
    if (stat(physical_block_path, &physical_stat) < 0) {
      log_warning(g_storage_logger,
                  "## Query ID: %" PRIu32
                  " - Fallo al obtener stat de %s. Ignorando.",
                  query_id, physical_block_path);
      continue;
    }

    // Comparar i-nodes
    if (logical_stat.st_ino == physical_stat.st_ino) {
      // Devolver el nombre del archivo físico (ej: "block0042")
      physical_block_name = strdup(entry->d_name);
      log_debug(g_storage_logger,
                "Bloque lógico %s apunta al bloque físico: %s",
                logical_block_path, physical_block_name);
      retval = 0;

      break;
    }
  }

  closedir(blocks_dir);

  return retval;
}

int32_t get_physical_block_number(const char *physical_block_id) {
  int32_t block_number;

  int result = sscanf(physical_block_id, "block%" SCNu32, &block_number);

  if (result == 1) {
    return block_number;
  } else {
    log_error(g_storage_logger,
              "Fallo al parsear el número de bloque de la cadena: %s",
              physical_block_id);
    return -1;
  }
}

int update_logical_block_link(uint32_t query_id, const char *logical_block_path,
                              char *physical_block_name) {
  if (remove(logical_block_path) != 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Ocurrió un error al eliminar el bloque lógico %s",
              query_id, logical_block_path);
    return -1;
  }

  char physical_block_path[PATH_MAX];
  snprintf(physical_block_path, sizeof(physical_block_path),
           "%s/physical_blocks/%s", g_storage_config->mount_point,
           physical_block_name);
  if (link(physical_block_path, logical_block_path) != 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - No se pudo crear el hard link de %s a %s.",
              query_id, physical_block_path, logical_block_path);
    return -2;
    goto end;
  }

  log_debug(g_storage_logger,
            "## Query ID: %" PRIu32 " - Se agregó el hardlink del "
            "bloque lógico %d al bloque físico %s",
            query_id, logical_block_path, physical_block_path);
}

int update_ph_block_status_in_bitmap(uint32_t query_id,
                                     char *physical_block_name) {
  // Obtenemos el número de referencias (links) del bloque físico
  char physical_block_path[PATH_MAX];
  snprintf(physical_block_path, size_of(physical_block_path),
           "%s/physical_blocks/%s", g_stroage_config->mount_point,
           physical_block_name);

  int numb_links = ph_block_links(physical_block_path);
  if (numb_links < 0) {
    log_error(
        g_storage_logger,
        "Query ID: %" PRIu32
        " - Error al obtener el número actual de links del bloque físico %s",
        query_id, physical_block_name);

    return -1;
  }

  if (numb_links > 1) {
    log_info(g_storage_logger,
             "Query ID: %" PRIu32
             " - El bloque físico %s continúa siendo referenciado por otros "
             "bloques lógicos. Se mantiene como ocupado en el bitmap.",
             query_id, physical_block_name);

    return 0;
  }

  log_info(g_storage_logger,
           "Query ID: %" PRIu32
           " - El bloque físico %s no es referenciado por ningún bloque "
           "lógico. Se procede a marcarlo como libre en el bitmap.",
           query_id, physical_block_name);

  int32_t physical_block_id = get_physical_block_number(physical_block_name);
  if (physical_block_id < 0) {
    log_error(g_storage_logger,
              "Query ID: %" PRIu32
              " - No se pudo obtener el id de bloque de la cadena %s",
              query_id, physical_block_name);

    return -2;
  }

  t_bitarray *bitmap = NULL;
  char *bitmap_buffer = NULL;
  if (bitmap_load(&bitmap, &bitmap_buffer) < 0) {
    log_error(g_storage_logger,
              "# Query ID: %" PRIu32 " - Fallo al cargar el bitmap.", query_id);
    return = -3;
  }

  bitarray_set_bit(bitmap, (off_t)physical_block_id);

  if (bitmap_persist(bitmap, bitmap_buffer) < 0) {
    log_error(g_storage_logger,
              "## Query ID: %" PRIu32
              " - Error al persistir bits libres en el bitmap.",
              query_id);
    return -4;
  }

  return 0;
}
