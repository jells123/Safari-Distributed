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
pthread_mutex_t timestamp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t permission_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t inviteResponses_cond = PTHREAD_COND_INITIALIZER;

vector<processInfo> tab; // T == size??

int inviteResponses, missing;
void *receiveMessages(void *ptr) {

    packet pkt;
    while ( true ) {

        //println("czekam na wiadomości...\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        pthread_mutex_lock(&timestamp_mtx);                 // jakis mtx tutaj?
        timestamp = max(timestamp, pkt.timestamp);
        pthread_mutex_unlock(&timestamp_mtx);

        //if (pkt.type != NOT_ORG)
        //    println("wiadomość: %s od %d\n", msgTypes[pkt.type], status.MPI_SOURCE);
        for (size_t i = 0; i < handlers.size(); i++) {
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

typedef struct orgInfo {
    int timestamp;
    int tid;
} orgInfo;

int permissions;
vector<orgInfo> queue;
vector<int> reqPermissions; 

void deleteFromQueue(int tid) {
    for(size_t i = 0; i < queue.size(); i++) {
        if(queue[i].tid == tid) {
            queue.erase(queue.begin() + i);
            return;
        }
    }
}

void reserveGuide() {
    pthread_mutex_lock(&timestamp_mtx);             // jakis mtx tutaj?
    timestamp++;
    packet msg = { timestamp, GUIDE_REQ, 0 };
    orgInfo myInfo = { timestamp, tid };
    pthread_mutex_unlock(&timestamp_mtx);

    queue.push_back(myInfo);

    permissions = 0;

    for (int i = 0; i < size; i++) {
        if (tab[i].role != TUR && i != tid) {
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            println("Sending req to [%d]", i);
            reqPermissions.push_back(i);
        }
        if(permissions >= (MAX_ORGS - P))
            break;
    }

    if(permissions < (MAX_ORGS - P)) {
        int i = 0;
        while(reqPermissions.size() < MAX_ORGS - 1) {
            if(i != tid && find(reqPermissions.begin(), reqPermissions.end(), i) == reqPermissions.end()) {
                reqPermissions.push_back(i);
                MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
                println("Sending req to [%d]", i);
            }
            i++;
            if(permissions >= (MAX_ORGS - P))
                break;
        }

        while(permissions < (MAX_ORGS - P));
    }

    deleteFromQueue(tid);
    println("Got a Guide!\n");
}

int tabSummary() {
    int participants, countOrgs = 0;
    for (int i = 0; i < T; i++) {
        if (tab[i].role == ORG) {
            countOrgs++;
            participants = 0;
            for (int j = 0; j < T; j++) {
                if (tab[j].role == TUR && tab[j].value == i)
                    participants++;
            }
            tab[i].value = G - participants - 1;
        }
    }
    return countOrgs;
}

vector<int> myGroup;
vector<int> invitations;

void orgsDeadlockProcess() {

    int countOrgs = tabSummary();

    vector<int> indices; vector<processInfo> procSorted;
    indices.push_back(0); procSorted.push_back(tab[0]);
    vector<int>::iterator intIt;
    vector<processInfo>::iterator procIt;

    // PROCESS INFO INSERTION SORT :)
    size_t j;
    for (int i = 1; i < T; i++) {
        processInfo current = tab[i];

        for (j = 0; j < procSorted.size(); j++) {

            if ( (current.role == ORG && procSorted[j].role != ORG) 
                || (current.role == ORG && procSorted[j].role == ORG && current.value < procSorted[j].value) 
                || (current.role == ORG && procSorted[j].role == ORG && current.value == procSorted[j].value && i < indices[j])) {

                intIt = indices.begin(); procIt = procSorted.begin();
                indices.insert(intIt + j, i); procSorted.insert(procIt + j, current);

                continue;
            }
        }
        if (j == procSorted.size()) {
            indices.push_back(i);
            procSorted.push_back(current);
        }
    }
    // END OF INSERTION SORT

    if (countOrgs > MAX_ORGS) {
        // nadmiarowi organizatorzy rezygnują i mogą dołączać
        for (int i = MAX_ORGS; i < procSorted.size(); i++) {
            if (procSorted[i].role == ORG) {
                procSorted[i].role = TUR;
                procSorted[i].value = -1;

                if (indices[i] == tid) {
                    myGroup.clear();
                    currentRole = TUR;
                }
            }
        }

        // dla tych którzy pozostali organizatorami przydzielamy turystów
        for (int i = 0; i < MAX_ORGS; i++) {
            int missing = procSorted[i].value;
            int j = MAX_ORGS;
            while (missing > 0 && j < procSorted.size()) {
                if (procSorted[j].role == TUR && procSorted[j].value != indices[i]) {
                    procSorted[j].value = indices[i];

                    if (indices[j] != tid) {
                        packet msg = { timestamp, CHANGE_GROUP, indices[i] };
                        MPI_Send( &msg, 1, MPI_PAKIET_T, indices[j], MSG_TAG, MPI_COMM_WORLD);                                         
                    }
                    else {
                        myGroup.push_back(indices[i]);
                    }

                    missing--;

                    if (indices[i] == tid) {
                        myGroup.push_back(indices[j]);
                    }

                }
                j++;
            }
            if (missing > 0 && j >= procSorted.size()) 
                println("Ja nie mogę, jakiś przypał.\n");
        }
    }
}

void *orgThreadFunction(void *ptr) {

    packet pkt;
    tab[tid].role = ORG;

    invitations.clear();
    while (myGroup.size() != G-1) {

        if (invitations.size() == T-1) {

            orgsDeadlockProcess();
            invitations.clear();
            if (currentRole == TUR) {
                println("I'm not ORG anymore...\n");
                return (void *)0;
            }
        }

        else {
            vector<int>::iterator it;
            int choice;
            missing = G-1 - myGroup.size();

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

            pthread_mutex_lock(&timestamp_mtx);
            timestamp++;
            packet msg = { timestamp, INVITE, missing };                // jakis mtx?
            pthread_mutex_unlock(&timestamp_mtx);

            for (int i = 0; i < missing; ++i) {
                int idx = invitations.size() - 1 - i;
                MPI_Send( &msg, 1, MPI_PAKIET_T, invitations[idx], MSG_TAG, MPI_COMM_WORLD);
                println("%d invited :) \n", invitations[idx]);
            }

            pthread_mutex_lock(&inviteResponses_mtx);
            while (inviteResponses != missing)
                pthread_cond_wait(&inviteResponses_cond, &inviteResponses_mtx);
            pthread_mutex_unlock(&inviteResponses_mtx);
  
       }

    }
    println("I've got a group!\n");
    //while (true) ;
    reserveGuide();

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

            pthread_mutex_lock(&timestamp_mtx);
            timestamp++;
            packet msg = { timestamp, NOT_ORG, 0 };               // jakis mtx?
            pthread_mutex_unlock(&timestamp_mtx);

            for (int i = 0; i < size; i++)
                if (i != tid)
                    MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        }
    }

    if (currentRole == ORG)
        pthread_create( &sender_th, NULL, orgThreadFunction, 0 );

    tab[tid].role = currentRole;

}

void inviteHandler(packet *pkt, int src) {
    pthread_mutex_lock(&myGroup_mtx);                   // juz jest w mtx?
    timestamp++;

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

    tab[src].role = ORG;

    pthread_mutex_unlock(&myGroup_mtx);
}

void change_groupHandler(packet *pkt, int src) {
    tab[tid].role = TUR;
    tab[tid].value = src;
    if (myGroup.size() > 0) {
        println("Group change! From %d to %d\n", myGroup[0], src);
        myGroup.clear();
    }
    myGroup.push_back(src);

    tab[src].role = ORG;

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

    tab[src].role = TUR;
    tab[src].value = tid;

    println("%d joining my group!", src);

    if (inviteResponses == missing) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);
}

void reject_hasgroupHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;
    tab[pkt->info_val].role = ORG;

    println("%d already has group :/\n", src);

    if (inviteResponses == missing) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);

}

void reject_isorgHandler(packet *pkt, int src) {
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;

    println("%d is org too. \n", src);

    if (inviteResponses == missing) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);
}

void response_guideReqHandler(packet *pkt, int src) {                   // osobny watek do odpowiadania na req o przewodnika?
    if(currentRole == ORG) {
        orgInfo hisInfo = { pkt->timestamp, src };
        queue.push_back(hisInfo);

        if(queue.size() <= P &&
            (myGroup.size() != G-1 || (myGroup.size() == G-1 
                && (pkt->timestamp < timestamp || (pkt->timestamp == timestamp 
                    && src < tid))))){
                
            pthread_mutex_lock(&timestamp_mtx);
            timestamp++;
            pthread_mutex_unlock(&timestamp_mtx);

            packet msg = { timestamp, GUIDE_RESP, 0 };
            MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
            println("Ok, I let you [%d] reserve a guide\n", src);
            
        } else {
            println("I won't let you [%d] reserve a guide! For now.. My timestamp: %d, his: %d\n", src, timestamp, pkt->timestamp);
        }


    } else {
        println("I'm not an ogr, sth went wrong...\n");
    }
}

void got_guideRespHandler(packet *pkt, int src) {
    pthread_mutex_lock(&permission_mtx);
    permissions++;
    pthread_mutex_unlock(&permission_mtx);
    println("Got permission from [%d]\n", src);
}

void prepare() {

    tab.reserve(T);
    //permissions.reserve(G);
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
    addMessageHandler(GUIDE_REQ, response_guideReqHandler);
    addMessageHandler(GUIDE_RESP, got_guideRespHandler);

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
