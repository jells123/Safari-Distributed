#ifndef STACKH
#define STACKH

#include <stdlib.h> 
#include <iostream> 
#include <mpi.h> 
#include <pthread.h>

#include "packet.h"

struct stack {
    packet *pakiet;
    struct stack *prev;
    MPI_Status status;
};

extern struct stack *stack;
extern pthread_mutex_t stack_mtx;// = PTHREAD_MUTEX_INITIALIZER;

void push_pkt( packet *pakiet, MPI_Status status );
packet *pop_pkt( MPI_Status *status);

#endif