#ifndef HANDLERSH
#define HANDLERSH

#include <mpi.h>
#include <stdlib.h>
#include <iostream>
#include <stddef.h>
#include <cstring>
#include <vector>
#include <csignal>

#include "packet.h"
#include "turysta.h"

struct messageHandler {
    MsgType msgType;
    void (*handler)(packet*, int);
};
extern std::vector<messageHandler> handlers;

void addMessageHandler(MsgType msgType, void (*handler)(packet*, int));

void inviteHandler(packet *pkt, int src);
void change_groupHandler(packet *pkt, int src);
void not_orgHandler(packet *pkt, int src);
void acceptHandler(packet *pkt, int src);
void reject_isorgHandler(packet *pkt, int src);
void reject_hasgroupHandler(packet *pkt, int src);
void guide_reqHandler(packet *pkt, int src);
void guide_respHandler(packet *pkt, int src);
void trip_endHandler(packet *pkt, int src);
void i_was_beatedHandler(packet *pkt, int src);
void omg_deadlockHandler(packet *pkt, int src);

#endif
