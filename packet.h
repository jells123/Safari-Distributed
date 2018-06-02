#ifndef PACKETH
#define PACKETH

#include <mpi.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <csignal>

enum MsgType {

    INVITE = 0, // zaproszenie do grupy
    ACCEPT = 1, // przyjęcie zaproszenia
    REJECT_HASGROUP = 2, // odmowa - mam już grupę
    REJECT_ISORG = 3, // odmowa - jestem organizatorem

    NOT_ORG = 4, // nie jestem organizatorem = szukam grupy
    I_WAS_BEATED = 5, // pobicie         
    CHANGE_GROUP = 6, // rozwiązanie zakleszczenia - turysta zmienia grupę do której dołączył
    GUIDE_REQ = 7, // request o przewodnika
    GUIDE_RESP = 8, // zgoda na przydzielenie przewodnika
    TRIP_END = 9 // koniec wycieczki -> zwolnienie zasobu przewodnika

};

typedef struct {

    int timestamp;
    MsgType type;
    int info_val;

} packet;

extern MPI_Datatype MPI_PAKIET_T;

#endif
