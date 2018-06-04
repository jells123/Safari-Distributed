#include "turysta.h"
#include "handlers.h"

// PYTANIA
// T == size?
// decyduje o tym czy pobity dopiero po powiadomieniu innych ze koniec wycieczki, czy blokuje kolejne?

using namespace std;

int T = 6; // liczba turystow
int G = 2; // rozmiar grupy
int P = 3; // liczba przewodnikow

int MAX_ORGS;

int ROOT = 0;
int MSG_TAG = 100;

int ORG_PROBABILITY = 75;
int GUIDE_BEATED_PROBABILITY = 10;
int BEATED_PROBABILITY = 30;
int TIME_BEATED = 3;
int GUIDE_TIME_BEATED = 5;
int lastReqTimestamp;

int lonelyOrgs = 0, deadlocks = 0;

volatile sig_atomic_t FORCE_END = 0;

bool beated = false;
bool deadlock_trouble = false;

pthread_mutex_t tab_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inviteResponses_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t myGroup_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timestamp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t permission_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t beated_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t state_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t deadlock_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t inviteResponses_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t permission_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t deadlock_cond = PTHREAD_COND_INITIALIZER;

MPI_Status status;
Role currentRole;

int inviteResponses, missing, permissions;
vector<orgInfo> queue;
vector<int> reqPermissions, myGroup, invitations;
vector<processInfo> tab;


const char* rolesNames[] = {
    "Nieznana",
    "Organizator",
    "Turysta",
    "Pobity"
};


void interruptHandler(int s) {
    printf("Caught signal %d!", s);
    FORCE_END = 1;
}


void clearResources() {

    queue.clear();
    vector<orgInfo>().swap(queue);

    reqPermissions.clear();
    vector<int>().swap(reqPermissions);

    myGroup.clear();
    vector<int>().swap(myGroup);

    invitations.clear();
    vector<int>().swap(invitations);

    tab.clear();
    vector<processInfo>().swap(tab);

    pthread_mutex_destroy(&tab_mtx);
    pthread_mutex_destroy(&inviteResponses_mtx);
    pthread_mutex_destroy(&myGroup_mtx);
    pthread_mutex_destroy(&timestamp_mtx);
    pthread_mutex_destroy(&queue_mtx);
    pthread_mutex_destroy(&permission_mtx);

}


void *receiveMessages(void *ptr) {

    packet pkt;
    while ( FORCE_END == 0 ) {

        // pthread_mutex_lock(&state_mtx);
        // if (currentRole == UNKNOWN && !beated) {
        //     randomRole();
        // }
        // pthread_mutex_unlock(&state_mtx);

        // else {
            MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            pthread_mutex_lock(&state_mtx);
            timestamp = max(timestamp, pkt.timestamp);

            for (size_t i = 0; i < handlers.size(); i++) {
                if (handlers[i].msgType == pkt.type) {
                    handlers[i].handler( &pkt, status.MPI_SOURCE );
                }
            }
            pthread_mutex_unlock(&state_mtx);

        // }
    }

    clearResources();
    return (void *)0;
}


void deleteFromQueue(int id) {
    pthread_mutex_lock(&queue_mtx);
    for(size_t i = 0; i < queue.size(); i++) {
        if(queue[i].tid == id) {
            println("Removing %d from queue\n", id);
            queue.erase(queue.begin() + i);
            pthread_mutex_unlock(&queue_mtx);
            return;
        }
    }
    if (!queue.empty())
        println("%d wasn't in my queue\n", id);         // moze sie zdarzyc ze nie bedzie jesli nie wyslal do nas req
    pthread_mutex_unlock(&queue_mtx);                   // (inne mu wystarczyly do zarezerwowania przewodnika), wiec to nie blad jak sie pojawi to czasem chyba ;)
}


void reserveGuide() {
    println("GIMME GUIDE!\n");
    pthread_mutex_lock(&timestamp_mtx);
    packet msg = { ++timestamp, GUIDE_REQ, 0 };
    orgInfo myInfo = { timestamp, tid };
    lastReqTimestamp = timestamp;

    pthread_mutex_lock(&queue_mtx);
    queue.push_back(myInfo);
    pthread_mutex_unlock(&queue_mtx);

    permissions = 0;
    notInterestedOgrs = 0;

    for (int i = 0; i < size; i++) {
        if (tab[i].role != TUR && i != tid) {
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            println("(1) Sending req to [%d]", i);
            reqPermissions.push_back(i);
        }
        if(permissions >= (MAX_ORGS - P))
            break;
    }

    pthread_mutex_lock(&permission_mtx);
    if (permissions < (MAX_ORGS - P)) {
        int i = 0;
        size_t reqSize = MAX_ORGS - 1;
        while (reqPermissions.size() < reqSize) {
            if(i != tid && find(reqPermissions.begin(), reqPermissions.end(), i) == reqPermissions.end()) {
                reqPermissions.push_back(i);
                MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
                println("(2) Sending req to [%d]", i);
            }
            i++;
            if(permissions >= (MAX_ORGS - P))
                break;
        }
        pthread_mutex_unlock(&timestamp_mtx);
        while (permissions < (MAX_ORGS - P))
            pthread_cond_wait(&permission_cond, &permission_mtx);
    }
    else
        pthread_mutex_unlock(&timestamp_mtx);
    pthread_mutex_unlock(&permission_mtx);

    println("Got a Guide!\n");
}


void *waitForTripEnd(void *ptr) {

    int trip_time = rand() % 10;
    // trip_time = 10;
    println("Trip started!\n");
    sleep(trip_time);

    pthread_mutex_lock(&deadlock_mtx);
    while (deadlocks > 0 && deadlocks < lonelyOrgs) {
        pthread_cond_wait(&deadlock_cond, &deadlock_mtx);
    }
    pthread_mutex_unlock(&deadlock_mtx);
    deadlocks = 0; lonelyOrgs = 0;

    sleep(5);

    int czy_pobity_guide = rand() % 100;
    if (czy_pobity_guide < GUIDE_BEATED_PROBABILITY) {
        println("Guide beated!\n");
        sleep(TIME_BEATED);
    }

    pthread_mutex_lock(&state_mtx);
    timestamp++;
    packet msg = { timestamp, TRIP_END, -1 };
    for (int i = 0; i < size; i++)
        //if (i != tid)
        MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
    println("TRIP END - everyone notified.\n");
    pthread_mutex_unlock(&state_mtx);

    return (void *)0;
}


void *gotBeated(void *ptr) {

    sleep(TIME_BEATED);


    println("Ok, I'm fine.\n");

    pthread_mutex_lock(&state_mtx);
    beated = false;
    pthread_mutex_unlock(&state_mtx);

    return (void *)0;
}


void decideIfBeated() {
    int czy_pobity = rand() % 100;

    if (czy_pobity < BEATED_PROBABILITY) {

        beated = true;
        //timestamp++;
        println("I got beated! Waiting for being healed.\n");
        pthread_create( &beated_th, NULL, gotBeated, 0);
    } else {
        beated = false;
    }
    
    // pthread_join(beated_th, NULL);
    // beated = false;
}


void goForTrip() {
    println("Going on a trip! :)\n");
    pthread_create( &trip_th, NULL, waitForTripEnd, 0);
    pthread_join(trip_th, NULL);
    println("I'm back from a trip :d.\n");
}

int countBeated() {
    int result = 0;
    for (int i = 0; i < size; i++) {
        if (tab[i].role == BEATED)
            result++;
    }
    return result;
}


int tabSummary() {
    int participants, countOrgs = 0;
    tab[tid].role = currentRole;

    for (int i = 0; i < T; i++) {
        if (tab[i].role == ORG) {
            countOrgs++;
            participants = 0;
            for (int j = 0; j < T; j++) {
                if (tab[j].role == TUR && tab[j].value == i)
                    participants++;
            }
            tab[i].value = G - participants - 1;
            if (tab[i].value < 0) {
                println("Uh oh, my group is bigger than it should? (value is %d, pid %d) \n", tab[i].value, i);
            }
        }
    }

    return countOrgs;
}


void comeBack() {
    println("Ended trip\n");

    deleteFromQueue(tid);
    pthread_mutex_lock(&queue_mtx);

    size_t i;

    if(!queue.empty()) {
        println("Sending overdue responses:\n");
        orgInfo orgSorted[queue.size()];

        for (i = 0; i < queue.size(); i++) {
            orgSorted[i].timestamp = queue[i].timestamp;
            orgSorted[i].tid = queue[i].tid;
        }

        int j, timePom, tidPom;

        for (i = 1; i < queue.size(); i++) {
            timePom = orgSorted[i].timestamp;
            tidPom = orgSorted[i].tid;

            j = i - 1;
            while ( j >= 0 &&
                ((timePom < orgSorted[j].timestamp)
                    || (timePom == orgSorted[j].timestamp
                        && tidPom < orgSorted[j].tid))) {

                    orgSorted[j+1].timestamp = orgSorted[j].timestamp;
                    orgSorted[j+1].tid = orgSorted[j].tid;
                    j--;

            }
            orgSorted[j+1].timestamp = timePom;
            orgSorted[j+1].tid = tidPom;
        }

        for (i = 0; i < queue.size(); i++) {
            println("%d: %d\n", orgSorted[i].tid, orgSorted[i].timestamp);
        }

        println("[comeBack] Nailed it!\n");

        pthread_mutex_lock(&timestamp_mtx);
        packet msg = { ++timestamp, GUIDE_RESP, 0 };
        pthread_mutex_unlock(&timestamp_mtx);

        for (i = 0; i < queue.size(); i++) {
            if (orgSorted[i].tid != tid) {
                MPI_Send( &msg, 1, MPI_PAKIET_T, orgSorted[i].tid, MSG_TAG, MPI_COMM_WORLD);
                println("Ok, I let you [%d] reserve a guide\n", orgSorted[i].tid);
            } else
                println("Why am I still in the queue?\n");
        }
        pthread_mutex_unlock(&queue_mtx);
    }
}


void orgsDeadlockProcess() {

    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses = 0;
    pthread_mutex_unlock(&inviteResponses_mtx);
    // println("invite responses mtx passed\n");

    // int myTursLeft = G-1 - myGroup.size();
    // packet msg = { timestamp, INVITE, missing };
    // // println("timestamp mtx passed\n");

    // // pthread_mutex_lock(&tab_mtx);
    // int countOrgs = tabSummary();
    // missing = 0;

    // for (int i = 0; i < T; i++) {
    //     if (i != tid) {
    //         println("Checking %d [%s, val %d]...\n", i, rolesNames[tab[i].role], tab[i].value);
    //         if (myTursLeft > 0 && 
    //             (tab[i].role == UNKNOWN // || tab[i].role == BEATED
    //             || (tab[i].role == TUR && tab[i].value == -1) )   
    //             ) {
    //             MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
    //             println("[unknown fix] %d invited again... \n", i);
    //             missing++;
    //             myTursLeft--;
    //         }
    //         else 
    //             if (tab[i].role == BEATED) {
    //             // if (i != tid) {
    //                 MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
    //                 println("[beated ping] %d invited again... \n", i);
    //                 missing++;
    //             // }
    //         }
    //         // }
    //         else if (tab[i].role == ORG && tab[i].value < 0) {
    //             println("[groups fix] found wrong pid: %d\n", i);
    //             for (int j = 0; j < T; j++) {
    //                 if (tab[j].role == TUR && tab[j].value == i) {
    //                     MPI_Send( &msg, 1, MPI_PAKIET_T, j, MSG_TAG, MPI_COMM_WORLD);
    //                     println("[groups fix] %d invited again... \n", j);
    //                     missing++;
    //                 }
    //             }
    //         }  
    //     }
    // }
    // // }
    // // pthread_mutex_unlock(&tab_mtx);
    // // println("tab mtx passed\n");

    // pthread_mutex_unlock(&state_mtx);

    // pthread_mutex_lock(&inviteResponses_mtx);
    // while (inviteResponses < missing)
    //     pthread_cond_wait(&inviteResponses_cond, &inviteResponses_mtx);
    // pthread_mutex_unlock(&inviteResponses_mtx);

    // pthread_mutex_lock(&state_mtx);
    // if (myGroup.size() + 1 == G) {
    //     println("Yay! Somehow.\n");
    //     return;
    // }

    int countOrgs = tabSummary();
    // deadlocks = 0;

    lonelyOrgs = countOrgs;
    for (int i = 0; i < size; i++) {
        if (tab[i].role == ORG && tab[i].value == 0)
            lonelyOrgs--;
    }

    int maxOrgs = (T - countBeated()) / G;
    // if (size - countOrgs - countBeated() <= 1) {
    //     println("Really shitty stuff going on here. (max Orgs : %d,  count Beated : %d)\n", maxOrgs, countBeated());
    // }
    packet msg = { timestamp, OMG_DEADLOCK, lonelyOrgs };
    for (int i = 0; i < size; i++) {
        if (tab[i].role == ORG)// && i != tid)
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
    }

    deadlock_trouble = true;
    pthread_mutex_unlock(&state_mtx);

    countOrgs = tabSummary();
    if(countOrgs < maxOrgs) {
        int roznica = maxOrgs - countOrgs;
        int u = 0;
        while(roznica > 0 || u < T-1) {
            if(tab[u].role != ORG) {
                tab[u].role = ORG;
                tab[u].value = -1;
                roznica--;
                u++;
            }
        }
    }
    int indices[T];
    processInfo procSorted[T];
    for (int i = 0; i < T; i ++) {
        indices[i] = i;
        procSorted[i].value = tab[i].value;
        procSorted[i].role = tab[i].role;
    }

    println("[deadlock] Awaiting deadlocks.\n");

    pthread_mutex_lock(&deadlock_mtx);
    while (deadlocks < lonelyOrgs)
        pthread_cond_wait(&deadlock_cond, &deadlock_mtx);
    pthread_mutex_lock(&state_mtx);
    pthread_mutex_unlock(&deadlock_mtx);

    println("[deadlock] Info gathered, proceeding to sort.\n");


    deadlocks = 0;
    lonelyOrgs = 0;

    // pthread_mutex_lock(&tab_mtx);

    int i, j, val, idx;
    Role role;

    for (i = 1; i < T; i++) {
        val = procSorted[i].value;
        role = procSorted[i].role;
        idx = indices[i];

        j = i-1;
        while (j >= 0 &&
                    ( (role == ORG && procSorted[j].role != ORG)
                    || (role == ORG && procSorted[j].role == ORG && val < procSorted[j].value)
                    || (role == ORG && procSorted[j].role == ORG && val == procSorted[j].value && idx < indices[j] )
                    || (role == TUR && procSorted[j].role == BEATED )
                    )
                ) {
            procSorted[j+1].role = procSorted[j].role;
            procSorted[j+1].value = procSorted[j].value;
            indices[j+1] = indices[j];
            j--;
        }
        procSorted[j+1].role = role;
        procSorted[j+1].value = val;
        indices[j+1] = idx;
    }

    // for (int i = 0; i < T; i++) {
    //     println("id %d: role %s, value %d\n", i, rolesNames[tab[i].role], tab[i].value);
    // }

    for (i = 0; i < T; i++) {
        println("%d. id %d: %s, %d\n", i, indices[i], rolesNames[procSorted[i].role], procSorted[i].value);
    }

    // END OF INSERTION SORT
    println("[deadlock] Sorted tab...\n");


    // if (countOrgs > MAX_ORGS) {
    if (countOrgs > maxOrgs) {
        //println("wchodzę, zaraz zobaczę\n");

        // nadmiarowi organizatorzy rezygnują i mogą dołączać
        for (i = maxOrgs; i < T; i ++) {
            if (procSorted[i].role == ORG) {
                procSorted[i].role = TUR;
                procSorted[i].value = -1;
                for (j = 0; j < T; j++) {
                    if (procSorted[j].role == TUR && procSorted[j].value == indices[i]) {
                        procSorted[j].value = -1;
                    }
                }
                if (indices[i] == tid) {

                    println("I'm not ORG anymore. :/\n");
                    myGroup.clear();
                    currentRole = TUR;

                    packet msg = { ++timestamp, NOT_ORG, -1 };
                    for (int i = 0; i < size; i++)
                        if (i != tid)
                            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
                }
            }
        }

        for (i = 0; i < maxOrgs; i++) {
            int turNeeded = procSorted[i].value;
            if (turNeeded == 0) continue;

            for (j = maxOrgs; j < T; j++) {
                if (procSorted[j].role == TUR && procSorted[j].value == indices[i])
                    continue;
                else if (procSorted[j].role == TUR && procSorted[j].value == -1) {
                    procSorted[j].value = indices[i];
                    if (indices[j] != tid) {
                        packet msg = { timestamp, CHANGE_GROUP, indices[i] };
                        MPI_Send( &msg, 1, MPI_PAKIET_T, indices[j], MSG_TAG, MPI_COMM_WORLD);
                    }
                    else if (indices[j] == tid) {
                        myGroup.clear();
                        myGroup.push_back(indices[i]);
                        currentRole = TUR;
                    }
                    turNeeded--;
                }
                if (turNeeded == 0) {
                    procSorted[i].value = turNeeded;
                    println("%d joining %d.\n", indices[j], indices[i]);
                    break;
                }
            }
        }
    }

    if (currentRole == ORG) {
        myGroup.clear();
        for (i = 0; i < T; i++) {
            if (procSorted[i].role == TUR && procSorted[i].value == tid) {
                myGroup.push_back(indices[i]);
            }
        }
    }

    deadlock_trouble = false;
    // pthread_mutex_unlock(&state_mtx);
}


void *orgThreadFunction(void *ptr) {
    println("Starting role as an ORG :)\n");

    if (beated)
        pthread_join(beated_th, NULL);
    // while (beated) ;

    size_t groupSize = G-1;
    size_t numberOfTurists = T-1;

    pthread_mutex_lock(&state_mtx);
    timestamp++; 

    tab[tid].role = ORG;
    tab[tid].value = groupSize;

    lonelyOrgs = 0;
    deadlocks = 0;

    while (myGroup.size() != groupSize && currentRole == ORG) {

        if (FORCE_END) {
            clearResources();
            return (void *)0;
        }

        if (invitations.size() == numberOfTurists) {

            println("Oh no, deadlock occured.\n");
            orgsDeadlockProcess();
            invitations.clear();

            if (currentRole == TUR) {
                println("[deadlock solved] I'm not ORG anymore...\n");
                pthread_mutex_unlock(&state_mtx);
                return (void *)0;
            }

            if (myGroup.size() == groupSize)  {
                println("[deadlock solved] My group is %d now.\n", (int) myGroup.size());
            }
            else {
                println("[deadlock ERROR] LOL MY GROUP SIZE IS %d !@#$\n", (int) myGroup.size());
            }
            pthread_mutex_unlock(&state_mtx);

        }

        else {

            vector<int>::iterator it;
            int choice;
            missing = G-1 - myGroup.size();

            if (invitations.size() + missing > numberOfTurists) {
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

            packet msg = { timestamp, INVITE, missing };

            for (int i = 0; i < missing; ++i) {
                int idx = invitations.size() - 1 - i;
                MPI_Send( &msg, 1, MPI_PAKIET_T, invitations[idx], MSG_TAG, MPI_COMM_WORLD);
                println("%d invited :) \n", invitations[idx]);
            }

            pthread_mutex_unlock(&state_mtx);

            pthread_mutex_lock(&inviteResponses_mtx);
            while (inviteResponses != missing)
                pthread_cond_wait(&inviteResponses_cond, &inviteResponses_mtx);
            pthread_mutex_unlock(&inviteResponses_mtx);

            pthread_mutex_lock(&state_mtx);
       }

    }

    invitations.clear();

    if (currentRole == ORG) {
        pthread_mutex_unlock(&state_mtx);
        println("I've got a group! size is %d :) \n", (int) myGroup.size());
        reserveGuide();
        waitForTripEnd(NULL);
        comeBack();
    }
    else {
        pthread_mutex_unlock(&state_mtx);
    }

    return (void *)0;

}


void randomRole() {
    
    Role prevRole = currentRole;

    myGroup.clear();
    invitations.clear();

    int czy_organizator = rand() % 100;
    if (czy_organizator < ORG_PROBABILITY)
        currentRole = ORG;
    else
        currentRole = TUR;

/////////////////////////////
    // currentRole = TUR;

    tab[tid].role = currentRole;

    if (currentRole == TUR)
        tab[tid].value = -1;
    else if (currentRole == ORG)
        tab[tid].value = G-1;

    if (prevRole != currentRole) {
        println("Setting new role: %s\n", rolesNames[currentRole]);

        if (currentRole == TUR) {

            packet msg = { ++timestamp, NOT_ORG, -1 };
            for (int i = 0; i < size; i++)
                if (i != tid)
                    MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        }
    }
    else {
        println("My role now is %s\n", rolesNames[currentRole]);
    }

    if (currentRole == ORG)
        pthread_create( &sender_th, NULL, orgThreadFunction, 0 );

}


void prepare() {

    FORCE_END = 0;
    tab.reserve(T);
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
    addMessageHandler(GUIDE_REQ, guide_reqHandler);
    addMessageHandler(GUIDE_RESP, guide_respHandler);
    addMessageHandler(TRIP_END, trip_endHandler);
    addMessageHandler(CHANGE_GROUP, change_groupHandler);
    addMessageHandler(I_WAS_BEATED, i_was_beatedHandler);
    addMessageHandler(OMG_DEADLOCK, omg_deadlockHandler);


}

int main(int argc, char **argv) {

    signal(SIGINT, &interruptHandler);  // to niestety nie działa :/

    if (argc == 3) {
        G = atoi(argv[1]);
        P = atoi(argv[2]);
    }

    init(&argc, &argv);
    T = size;

    MAX_ORGS = T / G;

    srand(time(NULL) + tid);
    prepare();

    pthread_create( &receiver_th, NULL, receiveMessages, 0);
    pthread_join( receiver_th, NULL );

    MPI_Finalize();
}
