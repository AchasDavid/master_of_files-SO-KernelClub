#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "init_master.h"

/**
 * Intenta despachar una query READY a un worker IDLE.
 * Retorna 0 si se despachó correctamente, -1 en caso de error.
 * Si no hay workers IDLE o queries READY, retorna 0 sin hacer nada.
 */
int try_dispatch(t_master* master);

#endif // SCHEDULER_H