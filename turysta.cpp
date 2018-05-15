#include <stdlib.h> 
#include <stdio.h> 
#include <mpi.h> 
#include <time.h> 
#include <iostream> 
#include <cstdlib> 
#include "packet.h"

#define ROOT 0
#define MSG_TAG 100

using namespace std;

MPI_Datatype MPI_PAKIET_T;
MPI_Status status;

enum Role {
    Org,
    Tur
};

typedef struct info {
    int type;
    int value;
}
Info;

int T = 10;
int G = 2;
int P = 3;

int zegar = 0;

int main(int argc, char * * argv) {
    int size, tid;

    MPI_Init( & argc, & argv);

    MPI_Comm_size(MPI_COMM_WORLD, & size);
    MPI_Comm_rank(MPI_COMM_WORLD, & tid);

    if (argc == 4) {
        T = atoi(argv[1]);
        G = atoi(argv[2]);
        P = atoi(argv[3]);
    }

    srand(time(NULL));

    Info tab[T]; // T == size??
    int permissions[G];
    int queue[(int) T / G];

    packet msg;

    const int nitems = 3;
    int blocklengths[3] = {
        1,
        1,
        1
    };
    MPI_Datatype typy[3] = {
        MPI_INT,
        MPI_INT,
        MPI_INT
    };
    MPI_Aint offsets[3];

    offsets[0] = offsetof(packet, timestamp);
    offsets[1] = offsetof(packet, type);
    offsets[2] = offsetof(packet, info_val);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, & MPI_PAKIET_T);
    MPI_Type_commit( & MPI_PAKIET_T);

    cout << "Liczba turystow: " << T << " Wielkosc gr: " << G << " Liczba przewodnikow: " << P << endl;

    while (true) {
        int czy_organizator = rand() % 1;

        zegar++;
        msg.timestamp = zegar;

        if (czy_organizator == 0) {
            msg.type = NOT_ORG;
            msg.info_val = 0;

            for (int i = 0; i < size; i++) {
                if (i != tid)
                    MPI_Send( & msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            }

        } else if (czy_organizator == 1) {
            //msg.type = MsgType.
        }

        for (int i = 0; i < size; i++) {
            if (i != tid)
                MPI_Recv( & msg, 1, MPI_PAKIET_T, i, MPI_ANY_TAG, MPI_COMM_WORLD, & status);
            cout << msg.timestamp << " " << msg.type << " " << msg.info_val << endl;
        }
    }

    MPI_Finalize();
}