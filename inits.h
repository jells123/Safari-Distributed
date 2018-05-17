#ifndef INITSH
#define INITSH

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include <pthread.h>
#include "packet.h"

struct messageHandler {
    MsgType msgType;
    void (*handler)(packet*);
};
extern int lastHandler;
extern messageHandler handlers[100];

extern pthread_t sender_th, receiver_th;
extern int size, tid, timestamp;

void check_thread_support(int provided);
void init(int *argc, char ***argv);
void addMessageHandler(MsgType msgType, void (*handler)(packet*));

#endif
