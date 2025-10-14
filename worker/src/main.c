#include <stdio.h>
#include <commons/log.h>
#include <commons/config.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <config/worker_config.h>
#include <utils/logger.h>
#include <connections/master.h>
#include <connections/storage.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <query_interpreter/query_interpreter.h>

#define MICROSECONDS_TO_SLEEP 1000000

typedef struct
{
    char query_path[PATH_MAX];
    int program_counter;
    bool is_executing;
    int query_id;
} query_context_t;

typedef struct
{
    pthread_mutex_t mux;
    bool has_query;
    bool should_stop;
    query_context_t current_query;
    int master_socket;
    int storage_socket;
    t_worker_config *config;
    t_log *logger;
    memory_manager_t *memory_manager;
    int worker_id;
} worker_state_t;

void *query_executor_thread(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Se deben ingresar los argumentos [archivo_config] y [ID Worker]\n");
        goto error;
    }

    char *config_file_path = argv[1];
    int worker_id = atoi(argv[2]);
    char worker_id_str[12];
    sprintf(worker_id_str, "%d", worker_id);

    t_worker_config *worker_config = create_worker_config(config_file_path);
    if (worker_config == NULL)
    {
        fprintf(stderr, "No se pudo cargar la configuración\n");
        goto error;
    }

    t_log_level log_level = log_level_from_string(worker_config->log_level);
    if (logger_init("worker", log_level, true) < 0)
    {
        fprintf(stderr, "No se pudo inicializar el logger global\n");
        goto clean;
    }

    t_log *logger = logger_get();
    log_info(logger, "## Logger inicializado");

    int client_socket_master = handshake_with_master(
        worker_config->master_ip,
        worker_config->master_port,
        worker_id_str);

    if (client_socket_master < 0)
    {
        goto clean;
    }

    int client_socket_storage = handshake_with_storage(
        worker_config->storage_ip,
        worker_config->storage_port,
        worker_id_str);

    if (client_socket_storage < 0)
    {
        goto clean;
    }

    uint16_t block_size;
    if (get_block_size(client_socket_storage, &block_size))
    {
        goto clean;
    }

    worker_config->block_size = (int)block_size;
    log_info(logger, "## Tamaño de bloque recibido desde Storage: %d", worker_config->block_size);

    memory_manager_t *memory_manager = create_memory_manager(
        worker_config->memory_size,
        worker_config->replacement_algorithm,
        worker_config->block_size);

    if (memory_manager == NULL)
    {
        log_error(logger, "## No se pudo inicializar la memoria interna");
        goto clean;
    }

    log_info(logger, "## Memoria interna creada (size=%d, page_size=%d, replacement=%s)",
             worker_config->memory_size,
             worker_config->block_size,
             worker_config->replacement_algorithm);

    worker_state_t worker_state = {
        .mux = PTHREAD_MUTEX_INITIALIZER,
        .has_query = false,
        .should_stop = false,
        .current_query = {.is_executing = false},
        .master_socket = client_socket_master,
        .storage_socket = client_socket_storage,
        .config = worker_config,
        .logger = logger,
        .memory_manager = memory_manager,
        .worker_id = worker_id};

    pthread_mutex_init(&worker_state.mux, NULL);

    pthread_t executor_thread;
    pthread_create(&executor_thread, NULL, query_executor_thread, &worker_state);
    pthread_join(executor_thread, NULL);

    mm_destroy(memory_manager);

    destroy_worker_config(worker_config);
    logger_destroy();
    if (client_socket_master >= 0)
        close(client_socket_master);
    if (client_socket_storage >= 0)
        close(client_socket_storage);
    exit(EXIT_SUCCESS);

clean:
    if (client_socket_master >= 0)
        close(client_socket_master);
    if (client_socket_storage >= 0)
        close(client_socket_storage);

    destroy_worker_config(worker_config);
    logger_destroy();
    return 0;

error:
    exit(EXIT_FAILURE);
}

void *query_executor_thread(void *arg)
{
    worker_state_t *state = (worker_state_t *)arg;

    while (true)
    {
        pthread_mutex_lock(&state->lock);
        bool can_execute = state->has_query && !state->should_stop;
        bool is_executing = state->current_query.is_executing;
        pthread_mutex_unlock(&state->lock);

        if (!can_execute && !is_executing)
        {
            usleep(MICROSECONDS_TO_SLEEP);
            continue;
        }

        pthread_mutex_lock(&state->lock);
        state->current_query.is_executing = true;
        char query_path[PATH_MAX];
        strcpy(query_path, state->current_query.query_path);
        int pc = state->current_query.program_counter;
        pthread_mutex_unlock(&state->lock);

        log_info(logger, "## Ejecutando Query: %s (PC=%d)", query_path, pc);

        char *raw_instruction = NULL;
        int fetch_result = fetch_instruction(query_path, pc, &raw_instruction);
        if (fetch_result < 0)
        {
            log_error(logger, "Error al hacer FETCH de la instrucción en PC=%d", pc);
            goto finish_query;
        }

        instruction_t *instruction = NULL;
        decode_instruction(raw_instruction, &instruction);
        free(raw_instruction);

        if (instruction == NULL)
        {
            log_error(logger, "No se pudo decodificar la instrucción (PC=%d)", pc);
            goto finish_query;
        }

        execute_instruction(
            instruction,
            state->storage_socket,
            state->master_socket,
            state->memory_manager,
            state->current_query->query_id,
            state->worker_id);

        free(instruction);

        pthread_mutex_lock(&state->lock);
        state->current_query.program_counter++;
        state->current_query.is_executing = false;
        pthread_mutex_unlock(&state->lock);

        usleep(state->config->memory_retardation);
        continue;

    finish_query:
        pthread_mutex_lock(&state->lock);
        state->has_query = false;
        state->current_query.is_executing = false;
        pthread_mutex_unlock(&state->lock);
        log_info(logger, "## Query finalizada o abortada");
        continue;
    }

    return NULL;
}
