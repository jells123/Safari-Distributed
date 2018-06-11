#include "turysta.h"
#include "handlers.h"

using namespace std;

int T = 6; // liczba turystow
int G = 2; // rozmiar grupy
int P = 1; // liczba przewodnikow

int MAX_ORGS;

int ROOT = 0;
int MSG_TAG = 100;

int ORG_PROBABILITY = 50;
int GUIDE_BEATED_PROBABILITY = 50;
int BEATED_PROBABILITY = 50;
int TIME_BEATED = 3;
int GUIDE_TIME_BEATED = 5;

int reqNumber = 0, lonelyOrgs = 0, deadlocks = 0;

volatile sig_atomic_t FORCE_END = 0;

bool beated = false;
bool deadlock_trouble = false;
bool imOnTrip = false;
bool reqSent = false;
bool guideBeated = false;

pthread_mutex_t state_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t guide_beated_cond = PTHREAD_COND_INITIALIZER;

MPI_Status status;
Role currentRole;

int inviteResponses, awaitingResponsesCount;
int missing, permissions;
int lastReqTimestamp;

vector<orgInfo> queue, overdue;
vector<int> reqPermissions, myGroup; //, invitations;
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

/*
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
*/

}


void *receiveMessages(void *ptr) {

    packet pkt;
    while ( FORCE_END == 0 ) {

        pthread_mutex_lock(&state_mtx);
        if (currentRole == ORG) {
            doOrgWork();
        }
        else
            pthread_mutex_unlock(&state_mtx);

        if (DEBUG == 1) println("msg wait.\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (DEBUG == 1) println("msg recv!\n");

        pthread_mutex_lock(&state_mtx);

        timestamp = max(timestamp, pkt.timestamp);
        timestamp++;
        for (size_t i = 0; i < handlers.size(); i++) {
            if (handlers[i].msgType == pkt.type) {
                handlers[i].handler( &pkt, status.MPI_SOURCE );
            }
        }

        pthread_mutex_unlock(&state_mtx);

    }

    clearResources();
    return (void *)0;
}

void doOrgWork() {

    if (imOnTrip) {
        pthread_mutex_unlock(&state_mtx);
        return;
    }

    if (awaitingResponsesCount != inviteResponses) {
        pthread_mutex_unlock(&state_mtx);
        return;
    }

    if (myGroup.size() + 1 < G) {
        int tursNeeded = G - 1 - myGroup.size();
        vector<int> possibleInv = getPossibleInvitations();
        int countInvited = 0;

        timestamp++;
        packet msg = { timestamp, INVITE, tursNeeded };
        if (possibleInv.size() >= tursNeeded) {

            int beatedCheck = countBeated();
            if (possibleInv.size() == beatedCheck) {
                if (DEBUG == 1) println("Only beated pipl left, sleepin'...\n");
                sleep(1);
            }

            while (countInvited < tursNeeded) {
                int shoot = rand() % possibleInv.size();
                MPI_Send( &msg, 1, MPI_PAKIET_T, possibleInv[shoot], MSG_TAG, MPI_COMM_WORLD);
                if (DEBUG == 1) println("Inviting %d...\n", possibleInv[shoot]);
                possibleInv.erase(possibleInv.begin() + shoot);
                countInvited++;
            }
            if (DEBUG == 1) println("Invited %d processes to my group!\n", countInvited);
            awaitingResponsesCount = countInvited;
            inviteResponses = 0;
            pthread_mutex_unlock(&state_mtx);
        }
        else if (possibleInv.size() == 0) {
            // już nie mamy kogo zaprosić!
            if (DEBUG == 1) println("I don't have a group and there's no one left to invite. :<\n");
            deadlockTrouble();
            if ( !(myGroup.size() + 1 == G && currentRole == ORG) )
                pthread_mutex_unlock(&state_mtx);
        }
        else {
            // możemy zaprosić, ale mniej niż nam potrzeba, czyli pewnie zaraz deadlock
            int beatedCheck = countBeated();
            if (possibleInv.size() == beatedCheck) {
                if (DEBUG == 1) println("Only beated pipl left, sleepin'...\n");
                sleep(1);
            }

            for (int i = 0; i < possibleInv.size(); i++) {
                MPI_Send( &msg, 1, MPI_PAKIET_T, possibleInv[i], MSG_TAG, MPI_COMM_WORLD);
            }
            if (DEBUG == 1) println("Sent %d invitations, but it's not enough for my group...\n", possibleInv.size());
            awaitingResponsesCount = possibleInv.size();
            inviteResponses = 0;
            pthread_mutex_unlock(&state_mtx);
        }
        possibleInv.clear(); 
        // pthread_mutex_unlock(&state_mtx);
    }

    if (myGroup.size() + 1 == G && currentRole == ORG) {

        // sprawdzamy jeszcze raz, bo deadlock
        if (reqSent == false) {
            if (DEBUG == 1) {
                println("I've got a group! About to send requests. \n");
                for (int i = 0; i < myGroup.size(); i++) {
                        println("%d belongs to my group %)\n", myGroup[i]);
                }
            }
            reserveGuide();
            pthread_mutex_unlock(&state_mtx);
            return;
        } else {
            // if (permissions < reqNumber - P + 1) {          //+ 1) { ???
            if (permissions < T - P) {
                // if (DEBUG == 1) println("[STATUS] Need: %d, currently have %d permissions\n", reqNumber - P + 1, permissions);
                if (DEBUG == 1) println("[STATUS] Need: %d, currently have %d permissions\n", T - P, permissions);

                pthread_mutex_unlock(&state_mtx);
                return;
            } else {
                // for (int i = 0; i < myGroup.size(); i++) {
                //     println("%d belongs to my group %)\n", myGroup[i]);
                // }
                tripFinito();
            }
        }
    }

}

void tripFinito() {
    
    imOnTrip = true;
    awaitingResponsesCount = 0;
    inviteResponses = 0;
    reqSent = false;

    if (DEBUG == 1)
        if (guideBeated)
            println("Waiting for a Guide to heal ;)\n");


    while (guideBeated) {
        pthread_cond_wait(&guide_beated_cond, &state_mtx);
    }


    if (currentRole == ORG) {
        println("Got a Guide! Going for a trip.\n");
        pthread_create( &trip_th, NULL, waitForTripEnd, 0);
    }
    else {
        pthread_mutex_unlock(&state_mtx);
    }

}



int countMaxOrgs() {
    int maxOrgs = T / G;

    // int activeCount = 0;
    // for (int i = 0; i < T; i++) {
    //     if (tab[i].role != BEATED)
    //         activeCount ++;
    // }

    return maxOrgs;    
}

void deadlockTrouble() {
    int countOrgs = tabSummary();

    int indices[T];
    processInfo procSorted[T];
    for (int i = 0; i < T; i ++) {
        indices[i] = i;
        procSorted[i].value = tab[i].value;
        procSorted[i].role = tab[i].role;
    }

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

    int maxOrgs = countMaxOrgs();

    if (countOrgs > maxOrgs) {

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

                    println("[deadlock] I'm not ORG anymore. :/\n");
                    myGroup.clear();
                    currentRole = TUR;
                    // orgsNumber = countOgrs();

                    // packet msg = { ++timestamp, NOT_ORG, -1 };
                    // for (int i = 0; i < size; i++)
                    //     if (i != tid)
                    //         MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
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

                    // if (indices[j] != tid) {
                    if (indices[i] == tid) {
                        packet msg = { timestamp, CHANGE_GROUP, indices[i] };
                        MPI_Send( &msg, 1, MPI_PAKIET_T, indices[j], MSG_TAG, MPI_COMM_WORLD);
                    }
                    else if (indices[j] == tid) {
                        myGroup.clear();
                        myGroup.push_back(indices[i]);
                        currentRole = TUR;

                        tab[tid].role = currentRole;
                        tab[tid].value = indices[i];
                    }
                    turNeeded--;
                }
                if (turNeeded == 0) {
                    procSorted[i].value = turNeeded;
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
        tab[tid].role = currentRole;
        tab[tid].value = G - 1 - myGroup.size();
        println("[deadlock] Solved! My group size: %d\n", myGroup.size());
    }

}

vector<int> getPossibleInvitations() {
    vector<int> result;
    for (int i = 0; i < T; i++) {

        if (i == tid)
            continue;

        if ((tab[i].role == TUR && tab[i].value == -1)
            || tab[i].role == UNKNOWN
            || tab[i].role == BEATED)
            result.push_back(i);

    }
    return result;
}

void deleteFromQueue(int id) {
    for(size_t i = 0; i < queue.size(); i++) {
        if(queue[i].tid == id) {
            if (DEBUG == 1) println("Removing %d from queue\n", id);
            queue.erase(queue.begin() + i);
            return;
        }
    }
}

bool canInvite(int numberOfTurs) {
    int count = 0;
    for (int i = 0; i < T; i++) {
        if ((tab[i].role == TUR && tab[i].value == -1)
            || tab[i].role == UNKNOWN)
            count++;
    }
    if (count >= numberOfTurs)
        return true;
    else
        return false;
}

void reserveGuide() {
    timestamp++;
    packet msg = { timestamp, GUIDE_REQ, 0 };
    lastReqTimestamp = timestamp;

    if (DEBUG == 1) println("GIMME GUIDE!\n");

    permissions = 0;
    // reqNumber = 0;
    // reqPermissions.clear();

    for (int i = 0; i < size; i++) {
        if (
            // tab[i].role != TUR && 
            i != tid) {
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            // reqPermissions.push_back(i);
            if (DEBUG == 1) println("Sending request to %d\n", i);
            // reqNumber++;
        }
    }

    int i = 0;
    size_t reqSize = MAX_ORGS - 1;

    /*while (reqPermissions.size() < reqSize) {
        if(i != tid && find(reqPermissions.begin(), reqPermissions.end(), i) == reqPermissions.end()) {
            reqPermissions.push_back(i);
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            if (DEBUG == 1) println("Sending request to %d\n", i);
            reqNumber++;
        }
        i++;
    }*/
    reqSent = true;
    if (DEBUG == 1) println("I've got a group, requested %d processes for a Guide.\n", T - 1);
    // if (DEBUG == 1) println("I've got a group, requested %d processes for a Guide.\n", reqPermissions.size());

}

void *waitForTripEnd(void *ptr) {

    pthread_mutex_unlock(&state_mtx);

    int trip_time = rand() % 10;
    sleep(trip_time);

    pthread_mutex_lock(&state_mtx);
    timestamp++;
    packet msg = { timestamp, TRIP_END, -1 };
    for (int i = 0; i < size; i++)
        if (i != tid)
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);

    if (DEBUG == 1) println("TRIP END - everyone notified.\n");
    for (int i = 0; i < myGroup.size(); i++) {
        tab[myGroup[i]].role = UNKNOWN;
        tab[myGroup[i]].value = -1;
    }
    currentRole = UNKNOWN;
    tab[tid].role = currentRole;
    tab[tid].value = -1;

    myGroup.clear();
    println("Ended trip :)\n");

    imOnTrip = false;
    permissions = 0;

    int czy_pobity_guide = rand() % 100;
    if (czy_pobity_guide < GUIDE_BEATED_PROBABILITY) {
        println("Guide beated!\n");
        guideBeated = true;
    }

    decideIfBeated();
    randomRole();

    for (int i = 0; i < queue.size(); i++) {
        orgInfo copy = { queue[i].timestamp, queue[i].tid};
        overdue.push_back(copy);
    }
    queue.clear();
    pthread_mutex_unlock(&state_mtx);

    // int czy_pobity_guide = rand() % 100;
    if (czy_pobity_guide < GUIDE_BEATED_PROBABILITY) {
        sleep(TIME_BEATED);
        println("Guide healed <3\n");
    }
    pthread_mutex_lock(&state_mtx);
    comeBack();
    pthread_mutex_unlock(&state_mtx);

    return (void *)0;
}

void decideIfBeated() {
    int czy_pobity = rand() % 100;
    if (czy_pobity < BEATED_PROBABILITY) {
        beated = true;

        tab[tid].role = BEATED;
        tab[tid].value = -666;

        pthread_mutex_unlock(&state_mtx);
        println("I got beated! Waiting for being healed.\n");
        sleep(TIME_BEATED);
        pthread_mutex_lock(&state_mtx);
    }
    beated = false;
}

int countBeated() {
    int result = 0;
    for (int i = 0; i < size; i++) {
        if (i != tid && tab[i].role == BEATED)
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
            tab[i].value = G - 1 - participants;
            if (tab[i].value < 0) {
                if (DEBUG == 1) println("Uh oh, pid %d has more TURs than needed? (value is %d) \n", i, tab[i].value);
            }
        }

    }

    return countOrgs;
}

int countOgrs() {
    int countOrgs = 0;
    tab[tid].role = currentRole;

    for (int i = 0; i < T; i++) {
        if (tab[i].role != TUR)
            countOrgs++;
    }

    return countOrgs;
}


void comeBack() {

    size_t i;
    guideBeated = false;
    pthread_cond_signal(&guide_beated_cond);

    if(!overdue.empty()) {
        if (DEBUG == 1) println("Sending overdue responses...\n");
        packet msg = { timestamp, GUIDE_RESP, 0 };

        for (i = 0; i < overdue.size(); i++) {
            if (overdue[i].tid != tid) {
                MPI_Send( &msg, 1, MPI_PAKIET_T, overdue[i].tid, MSG_TAG, MPI_COMM_WORLD);
                if (DEBUG == 1) println("Ok, I let you [%d] reserve a guide\n", overdue[i].tid);
            }
        }
        overdue.clear();
    }
}

void randomRole() {

    Role prevRole = currentRole;
    timestamp++;

    myGroup.clear();
    // invitations.clear();

    int czy_organizator = rand() % 100;
    if (czy_organizator < ORG_PROBABILITY)
        currentRole = ORG;
    else
        currentRole = TUR;

    tab[tid].role = currentRole;

    if (currentRole == TUR)
        tab[tid].value = -1;
    else if (currentRole == ORG)
        tab[tid].value = G-1;

    println("My role now is: %s\n", rolesNames[currentRole]);

    if (currentRole == TUR) {

        packet msg = { timestamp, NOT_ORG, -1 };
        for (int i = 0; i < size; i++)
            if (i != tid)
                MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);

    }
}


void prepare() {

    FORCE_END = 0;
    tab.reserve(T);
    queue.reserve((int) T/G);
    timestamp = 0;
    imOnTrip = false;
    currentRole = UNKNOWN;

    inviteResponses = 0;
    awaitingResponsesCount = 0;

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
    // println("DEBUG SET: %d\n", DEBUG);

    pthread_create( &receiver_th, NULL, receiveMessages, 0);
    pthread_join( receiver_th, NULL );

    MPI_Finalize();
}