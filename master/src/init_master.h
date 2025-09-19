#ifndef INIT_MASTER_H
#define INIT_MASTER_H

#include <pthread.h>

// Forward declarations (para evitar inclusiones circulares)
typedef struct query_table t_query_table;
typedef struct worker_table t_worker_table;

// Estructura principal del Master
typedef struct master {
    // Tablas principales
    t_query_table *queries_table; // Tabla de control de queries
    t_worker_table *workers_table; // Tabla de control de workers

    // Datos de configuración
    char *ip; // IP del Master
    char *port; // Puerto del Master
    int aging_interval; // Intervalo de "envejecimiento"
    char *scheduling_algorithm; // Algoritmo de planificación
    int multiprogramming_level; // Nivel de multiprogramación (Workers conectados)

    // Hilos principales
    pthread_t aging_thread; // Hilo de envejecimiento
    pthread_t scheduling_thread; // Hilo de planificación
    pthread_t health_check_thread; // Hilo de chequeo de salud de workers y queries

    // Logger
    t_log *logger;
} t_master;

t_master* init_master(char *ip, char *port, int aging_interval, char *scheduling_algorithm, t_log *logger);
void destroy_master(t_master *master);

#endif