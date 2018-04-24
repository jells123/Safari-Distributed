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
    REJECT_ORG,
    // ...?
};

typedef struct {
    int timestamp;
    MsgType type;
    int info_val;
} packet;