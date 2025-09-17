#include "operations/handshake.h"

t_package* reply_handshake(void) {

    t_package *package = package_create(OP_CODE_RES_HANSHAKE, NULL);

    return package;
}