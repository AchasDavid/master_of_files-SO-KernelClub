#include <stdio.h>

#include "scheduler.h"
#include "init_master.h"
#include "worker_manager.h"
#include "query_control_manager.h"

int try_dispatch(t_master *master) {
    if (master == NULL || master->workers_table == NULL || master->queries_table == NULL) {
        log_error(master ? master->logger : NULL, "[try_dispatch] Estructura master inválida o no inicializada.");
        return -1;
    }

    int result = 0; // valor de retorno (0 = OK, -1 = error)

    // Bloqueo ordenado: primero workers, luego queries
    // Utilizar este orden consistentemente para evitar posibles deadlocks
    if (pthread_mutex_lock(&master->workers_table->worker_table_mutex) != 0) {
        log_error(master->logger, "[try_dispatch] Error al bloquear mutex de workers.");
        return -1;
    }

    if (pthread_mutex_lock(&master->queries_table->query_table_mutex) != 0) {
        log_error(master->logger, "[try_dispatch] Error al bloquear mutex de queries.");
        pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
        return -1;
    }

    // Validaciones
    if (master->workers_table->idle_list == NULL || master->queries_table->ready_queue == NULL) {
        log_error(master->logger, "[try_dispatch] Listas no inicializadas (idle_list o ready_queue).");
        result = -1;
        goto unlock_and_exit;
    }

    // Si no hay workers o queries, simplemente salimos sin error
    if (list_is_empty(master->workers_table->idle_list)) {
        log_debug(master->logger, "[try_dispatch] No hay workers IDLE disponibles.");
        goto unlock_and_exit;
    }

    if (list_is_empty(master->queries_table->ready_queue)) {
        log_debug(master->logger, "[try_dispatch] No hay queries READY para despachar.");
        goto unlock_and_exit;
    }

    // FIFO: tomar el primero de cada lista (aunque solo planifico queries,
    // utilizo FIFO para workers).
    // TODO: probar si genera afinidad entre queries y workers
    t_query_control_block *query = list_remove(master->queries_table->ready_queue, 0);
    t_worker_control_block *worker = list_remove(master->workers_table->idle_list, 0);

    if (query == NULL || worker == NULL) {
        log_error(master->logger, "[try_dispatch] Error inesperado: query o worker NULL al remover de las listas.");
        result = -1;
        goto unlock_and_exit;
    }

    // Actualizar estados
    worker->current_query_id = query->query_id;
    worker->state = WORKER_STATE_BUSY;
    query->state = QUERY_STATE_RUNNING;

    // Mover a las listas activas
    list_add(master->queries_table->running_list, query);
    list_add(master->workers_table->busy_list, worker);

    log_info(master->logger,
        "[try_dispatch] Asignada Query ID=%d al Worker ID=%d (FIFO Dispatch)",
        query->query_id, worker->worker_id);

    // TODO: enviar mensaje de inicio de ejecución
    // if (send_start_query(worker->socket_fd, query) < 0) { ... }

unlock_and_exit:
    pthread_mutex_unlock(&master->queries_table->query_table_mutex);
    pthread_mutex_unlock(&master->workers_table->worker_table_mutex);
    return result;
}
