#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    OP_QUERY_HANDSHAKE,
    OP_QUERY_FILE_PATH,
// Agregar codigos para workers y otros modulos
} t_message_op_code;

#endif
