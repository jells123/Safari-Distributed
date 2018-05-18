#ifndef HANDLERSH
#define HANDLERSH

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <cstring>
#include <vector>

#include "packet.h"

struct messageHandler {
    MsgType msgType;
    void (*handler)(packet*, int);
};
extern std::vector<messageHandler> handlers;

void addMessageHandler(MsgType msgType, void (*handler)(packet*, int));

#endif