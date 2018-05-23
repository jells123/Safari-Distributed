#ifndef PACKETH
#define PACKETH

#include <mpi.h>
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <math.h>
#include <csignal>

enum MsgType {

    INVITE, // zaproszenie do grupy     
    ACCEPT, // przyjęcie zaproszenia 
    REJECT_HASGROUP, // odmowa - mam już grupę
    REJECT_ISORG, // odmowa - jestem organizatorem

    NOT_ORG, // nie jestem organizatorem = szukam grupy
    BEATED, // pobicie         
    CHANGE_GROUP, // rozwiązanie zakleszczenia - turysta zmienia grupę do której dołączył
    GUIDE_REQ, // request o przewodnika
    GUIDE_RESP, // zgoda na przydzielenie przewodnika
    TRIP_END // koniec wycieczki -> zwolnienie zasobu przewodnika

};

typedef struct {

    int timestamp;
    MsgType type;
    int info_val;

} packet;

extern MPI_Datatype MPI_PAKIET_T;

#endif
