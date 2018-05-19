#include <stdlib.h> 
#include <stdio.h> 
#include <mpi.h> 

#include <time.h> 
#include <iostream> 
#include <cstdlib> 
#include <vector>
#include <set>
#include <algorithm>

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

pthread_mutex_t tab_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inviteResponses_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t myGroup_mtx = PTHREAD_MUTEX_INITIALIZER;

vector<processInfo> tab; // T == size??
vector<int> permissions, queue;

int inviteResponses;
void *receiveMessages(void *ptr) {

    packet pkt;
    while ( true ) {

        //println("czekam na wiadomości...\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        timestamp = max(timestamp, pkt.timestamp);

        //if (pkt.type != NOT_ORG)
        //    println("wiadomość: %s od %d\n", msgTypes[pkt.type], status.MPI_SOURCE);
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

vector<int> myGroup;
vector<int> invitations;
void *orgThreadFunction(void *ptr) {

    packet pkt;
    //while ( true ) {

        //if (currentRole == ORG) {
            invitations.clear();

            //pthread_mutex_lock(&myGroup_mtx);
            //int groupSize = myGroup.size(); 

            while (myGroup.size() != G-1) {

                if (invitations.size() == T-1) {

                    for (int i = 0; i < tab.size(); i++) {
                        if (tab[i].role == ORG) {
                            int participants = 0;
                            for (int j = 0; j < tab.size(); j++) {
                                if (tab[j].role == TUR && tab[j].value == i)
                                    participants++;
                            }
                            tab[i].value = G - participants - 1;
                        }
                    }

                    vector<int> procSortedIndexes;
                    vector<processInfo> sortingHelper;

                    procSortedIndexes.push_back(0);
                    sortingHelper.push_back(tab[0]);

                    vector<int>::iterator intIt;
                    vector<processInfo>::iterator procInfoIt;

                    for (int i = 1; i < tab.size(); i++) {
                        processInfo current = tab[i];

                        for (int j = 0; j < sortingHelper.size(); j++) {
                            if ( (current.role == ORG && sortingHelper[j].role == TUR) 
                                || (current.role == ORG && sortingHelper[j].role == ORG && current.value < sortingHelper[j].value) 
                                || (current.role == ORG && sortingHelper[j].role == ORG && current.value == sortingHelper[j].value && i < procSortedIndexes[j])) {

                                intIt = procSortedIndexes.begin();
                                procInfoIt = sortingHelper.begin();

                                procSortedIndexes.insert(intIt + j, i);
                                sortingHelper.insert(procInfoIt + j, current);

                                continue;
                            }
                            else if (j == sortingHelper.size()) {
                                procSortedIndexes.push_back(i);
                                sortingHelper.push_back(current);
                            }
                        }

                    }
                    println("Nailed it!\n");
                    for (int i = 0; i < sortingHelper.size(); i++) {
                        printf("[tid %d, role %s, val %d] ", procSortedIndexes[i], rolesNames[sortingHelper[i].role], sortingHelper[i].value);
                    }

                }

                vector<int>::iterator it;
                int choice, missing = G-1 - myGroup.size();

                if (invitations.size() + missing > T-1) {
                    missing = T - invitations.size();
                }

                pthread_mutex_lock(&inviteResponses_mtx);
                inviteResponses = 0;
                pthread_mutex_unlock(&inviteResponses_mtx);

                for ( int i = 0; i < missing; ++i ) {
                    do {
                        choice = rand()%T;
                        it=find(invitations.begin(), invitations.end(), choice);
                    } while (choice == tid || it != invitations.end());
                    invitations.push_back(choice);
                }

                for (int i = 0; i < missing; ++i) {
                    int idx = invitations.size() - 1 - i;
                    packet msg = { timestamp, INVITE, missing };
                    MPI_Send( &msg, 1, MPI_PAKIET_T, invitations[idx], MSG_TAG, MPI_COMM_WORLD);
                    println("%d invited :) \n", invitations[idx]);
                }

                while (inviteResponses != missing) ;

            }
            println("I've got a group!\n");
            //while (true) ;
            
        //}

    //}

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
        println("new role: %s\n", rolesNames[currentRole]);

        if (currentRole == TUR) {
            packet msg = { timestamp, NOT_ORG, 0 };
            for (int i = 0; i < size; i++)
                if (i != tid)
                    MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        }
    }

    if (currentRole == ORG)
        pthread_create( &sender_th, NULL, orgThreadFunction, 0 );

}

void inviteHandler(packet *pkt, int src) {
    pthread_mutex_lock(&myGroup_mtx);

    if (currentRole == TUR && myGroup.empty()) {
        myGroup.push_back(src);
        packet msg = { timestamp, ACCEPT, 0 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);   
    }
    else if (currentRole == TUR && !myGroup.empty()) {
        packet msg = { timestamp, REJECT_HASGROUP, myGroup[0] };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);   
    }
    else if (currentRole == ORG) {
        packet msg = { timestamp, REJECT_ISORG, G - myGroup.size() - 1 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD); 
    }

    pthread_mutex_unlock(&myGroup_mtx);
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
        && MAX_ORGS - (T - touristsCount) >= tid ) { // o jeden za mało?
        currentRole = ORG;
        println("I became ORG! Because I could.\n");

        if (currentRole == ORG)
            pthread_create( &sender_th, NULL, orgThreadFunction, 0 );
    }
}

void acceptHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    myGroup.push_back(src);
    pthread_mutex_unlock(&inviteResponses_mtx);

    tab[src].role = TUR;
    tab[src].value = tid;

    println("%d joining my group!", src);
}

void reject_hasgroupHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    pthread_mutex_unlock(&inviteResponses_mtx);

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;

    println("%d already has group :/\n", src);

}

void reject_isorgHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    pthread_mutex_unlock(&inviteResponses_mtx);

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;

    println("%d is org too. \n", src);
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
        tab[i].value = -1;
    }

    addMessageHandler(NOT_ORG, not_orgHandler);
    addMessageHandler(ACCEPT, acceptHandler);
    addMessageHandler(REJECT_HASGROUP, reject_hasgroupHandler);
    addMessageHandler(REJECT_ISORG, reject_isorgHandler);
    addMessageHandler(INVITE, inviteHandler);

}

int main(int argc, char * * argv) {

    if (argc == 4) {
        T = atoi(argv[1]);
        G = atoi(argv[2]);
        P = atoi(argv[3]);
    }
    MAX_ORGS = T / G;

    init(&argc, &argv);
    srand(time(NULL) + tid);

    //cout << "Liczba turystow: " << T << " Wielkosc grupy: " << G << " Liczba przewodnikow: " << P << endl;

    prepare();

    // pthread_create( &sender_th, NULL, orgThreadFunction, 0);
    pthread_create( &receiver_th, NULL, receiveMessages, 0);

    packet msg;

    while (true) {
    }

    MPI_Finalize();
}