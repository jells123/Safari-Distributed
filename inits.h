#ifndef INITSH
#define INITSH

#include <mpi.h>
#include <stdlib.h>
#include <iostream>
#include <stddef.h>
#include <cstring>
#include <vector>
#include <csignal>

#include <pthread.h>
#include "packet.h"
#include "handlers.h"

// to debug or not to debug?
// #define DEBUG

// #ifdef DEBUG
// #define debug(FORMAT, ...) printf("%c[%d;%dm [L: %d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(tid/7))%2, 31+(6+tid)%7, timestamp, tid, ##__VA_ARGS__, 27,0,37);

// #else
// #define debug(...) ;
// #endif

#define DEBUG 0

#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

#define println(FORMAT, ...) printf("%c[%d;%dm [L: %d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(tid/7))%2, 31+(6+tid)%7, timestamp, tid, ##__VA_ARGS__, 27,0,37);

extern pthread_t sender_th, receiver_th, trip_th, beated_th;
extern int size, tid, timestamp;

// extern char** rolesNames, msgTypes;

void check_thread_support(int provided);
void init(int *argc, char ***argv);

#endif
