#ifndef STORAGE_SERVER_H_
#define STORAGE_SERVER_H_

#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/log.h>
#include<commons/collections/list.h>
#include<string.h>
#include<assert.h>
#include "globals/globals.h"
#include "utils/serialization.h"

typedef struct {
    int client_socket;
    //t_log* logger;
} t_client_data;

int wait_for_client(int server_socket);
void* handle_client(void* arg);

#endif