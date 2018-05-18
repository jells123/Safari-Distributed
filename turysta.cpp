#include <stdlib.h> 
#include <stdio.h> 
#include <mpi.h> 

#include <time.h> 
#include <iostream> 
#include <cstdlib> 
#include <vector>

#include "packet.h"
#include "inits.h"
#include "constants.h"
#include "handlers.h"

using namespace std;

MPI_Status status;

enum Role {
    UNKNOWN,
    ORG, // organizator
    TUR // turysta
};
Role currentRole;

typedef struct processInfo {
    Role role;
    int value;
} processInfo;

int T = 10; // liczba turystow
int G = 2; // rozmiar grupy
int P = 3; // liczba przewodnikow

int MAX_ORGS;
pthread_mutex_t tab_mtx;

vector<processInfo> tab; // T == size??
vector<int> permissions, queue;

void *receiveMessages(void *ptr) {

    packet pkt;
    while ( true ) {

        //println("czekam na wiadomości...\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        //println("wiadomość: %s od %d\n", msgTypes[pkt.type], status.MPI_SOURCE);
        for (int i = 0; i < handlers.size(); i++) {
            if (handlers[i].msgType == pkt.type)
                handlers[i].handler( &pkt, status.MPI_SOURCE ); 
        }

        /*
        packet *newpkt = (packet*) malloc(sizeof(packet));
        memcpy(newpkt, (const char *)&pkt, sizeof(packet));
        push_pkt(newpkt, status);
        free(newpkt);
        */
    }

    return (void *)0;
}

void randomRole() {

    Role prevRole = currentRole;
    int czy_organizator = rand() % 100;
    if (czy_organizator < ORG_PROBABILITY)
        currentRole = ORG;
    else 
        currentRole = TUR;

    if (prevRole != currentRole) {
        println("nowa rola: %s\n", rolesNames[currentRole]);

        if (currentRole == TUR) {
            packet msg = { timestamp, NOT_ORG, 0 };
            for (int i = 0; i < size; i++)
                if (i != tid)
                    MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        }
    }
}

void *sendMessages(void *ptr) { // NOPE, MYLĄCA NAZWA FUKNCJI ;)

    packet pkt;
    while ( true ) {
        
        if (currentRole == ORG) {
            int invited = 0;
            
        }


    }

    return (void *)0;
}

void not_orgHandler(packet *pkt, int src) {
    tab[src].role = TUR;
    int touristsCount = 0;
    for (int i = 0; i < size; i++) {
        if (tab[i].role == TUR)
            touristsCount++;
    }
    if (currentRole == TUR
        && T - touristsCount < MAX_ORGS 
        && MAX_ORGS - (T - touristsCount) > tid ) { // o jeden za mało?
        currentRole = ORG;
        println("I became ORG!\n");
    }

    
}

void prepare() {

    tab.reserve(T);
    permissions.reserve(G);
    queue.reserve((int) T/G);
    timestamp = 0;

    currentRole = UNKNOWN;
    randomRole();

    for (int i = 0; i < T; i++) {
        tab[i].role = UNKNOWN;
        tab[i].value = 0;
    }

    addMessageHandler(NOT_ORG, not_orgHandler);

}

int main(int argc, char * * argv) {

    if (argc == 4) {
        T = atoi(argv[1]);
        G = atoi(argv[2]);
        P = atoi(argv[3]);
    }
    MAX_ORGS = T / G;

    init(&argc, &argv);
    //cout << "Liczba turystow: " << T << " Wielkosc grupy: " << G << " Liczba przewodnikow: " << P << endl;

    prepare();

    pthread_create( &sender_th, NULL, sendMessages, 0);
    pthread_create( &receiver_th, NULL, receiveMessages, 0);

    packet msg;

    while (true) {

        // timestamp++;
        // msg.timestamp = timestamp;

        // for (int i = 0; i < size; i++) {
        //     if (i != tid)
        //         MPI_Recv( & msg, 1, MPI_PAKIET_T, i, MPI_ANY_TAG, MPI_COMM_WORLD, & status);
        //     cout << msg.timestamp << " " << msg.type << " " << msg.info_val << endl;
        // }
    }

    MPI_Finalize();
}