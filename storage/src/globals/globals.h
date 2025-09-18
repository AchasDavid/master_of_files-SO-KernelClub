#ifndef STORAGE_GLOBALS_H_
#define STORAGE_GLOBALS_H_

#include <commons/log.h>

typedef struct {
    char* storage_ip;
    char* storage_port;
    bool fresh_start;
    char* module_path;
    int operation_delay;
    int block_access_delay;
    t_log_level log_level;
} t_storage_config;

extern t_log* g_storage_logger;
extern t_storage_config* g_storage_config;
extern int g_worker_counter;

#endif