#include "utils.h"


t_log* create_logger(char *directory, char *process_name, bool is_active_console, t_log_level log_level) {
    char path_log[PATH_MAX];

    snprintf(path_log, PATH_MAX, "%s/%s.log", directory, process_name);

    t_log* logger = log_create(path_log, process_name, is_active_console, log_level);

    return logger;
}

int log_set_level(t_log* logger, t_log_level new_log_level) {
    switch (new_log_level)
    {
    case 0: /* LOG_LEVEL_TRACE */
        logger->detail = LOG_LEVEL_TRACE;
        break;
    case 1: /* LOG_LEVEL_DEBUG */
        logger->detail = LOG_LEVEL_DEBUG;
        break;
    case 2: /* LOG_LEVEL_INFO */
        logger->detail = LOG_LEVEL_INFO;
        break;
    case 3: /* LOG_LEVEL_WARNING */
        logger->detail = LOG_LEVEL_WARNING;
        break;
    case 4: /* LOG_LEVEL_ERROR */
        logger->detail = LOG_LEVEL_ERROR;
        /* code */
        break;
    
    default:
        return 1; // Nivel de logeo invalido
    }

    return 0;
}
