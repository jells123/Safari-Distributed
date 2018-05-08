#ifndef PACKETH
#define PACKETH

#include <mpi.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum MsgType {
    INVITE,
    ACCEPT,
    REJECT_HASGROUP,
    REJECT_ISORG,
    NOT_ORG,         // nie jestem organizatorem
    BEATED,
    CHANGE_GROUP,
    GUIDE_REQ,
    GUIDE_RESP,      // zgoda na przydzielenie przewodnika
    TRIP_END
};

typedef struct {
    int timestamp;
    MsgType type;
    int info_val;
} packet;
