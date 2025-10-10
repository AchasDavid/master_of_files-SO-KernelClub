#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "page_table.h"
#include <stdbool.h>

/**
 * Representa una tabla de páginas asociada a un File:Tag específico.
 */
typedef struct {
    char *file;
    char *tag;
    page_table_t *page_table;
} file_tag_entry_t;

/**
 * Gestor global de memoria interna del Worker.
 * Mantiene una lista dinámica de tablas de páginas por File:Tag.
 */
typedef struct {
    file_tag_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
    size_t page_size;
    pt_replacement_t policy;
} memory_manager_t;

typedef struct {
    uint32_t page_number;
    void *frame_data;
    size_t frame_size;
} page_t;


memory_manager_t *mm_create(size_t page_size, pt_replacement_t policy);
void mm_destroy(memory_manager_t *mm);
page_table_t *mm_find_page_table(memory_manager_t *mm, char *file, char *tag);
page_table_t *mm_get_or_create_page_table(memory_manager_t *mm, char *file, char *tag);
void mm_remove_page_table(memory_manager_t *mm, char *file, char *tag);
bool mm_has_page_table(memory_manager_t *mm, char *file, char *tag);

int mm_write_to_memory(memory_manager_t *mm, page_table_t *page_table, uint32_t base_address, void *data);
int mm_read_from_memory(memory_manager_t *mm, page_table_t *page_table, uint32_t base_address, size_t size);
page_t *mm_get_dirty_pages(memory_manager_t *mm, char *file, char *tag, size_t *count);
void mm_mark_all_clean(memory_manager_t *mm, char *file, char *tag);

#endif
