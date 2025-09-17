#ifndef STORAGE_GLOBALS_H_
#define STORAGE_GLOBALS_H_

#include <commons/log.h>

typedef struct {
    char* storage_ip;
    char* storage_port;
    char* fresh_start;
    char* module_path;
    char* operation_delay;
    char* block_access_delay;
    char* log_level;
} t_storage_config;

extern t_log* g_storage_logger;
extern t_storage_config* g_storage_config;
int g_worker_counter;

#endif