#include "commit_tag.h"

t_package *handle_tag_commit_request(t_package *package) {
  uint32_t query_id;
  char *name = NULL;
  char *tag = NULL;

  if (deserialize_block_read_request(package, &query_id, &name, &tag) < 0) {
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
