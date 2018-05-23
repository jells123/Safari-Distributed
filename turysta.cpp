#include "turysta.h"

using namespace std;

int T = 6; // liczba turystow
int G = 2; // rozmiar grupy
int P = 2; // liczba przewodnikow

int MAX_ORGS;

int ROOT = 0;
int MSG_TAG = 100;

int ORG_PROBABILITY = 75;
int GUIDE_BEATED_PROBABILITY = 0;
int BEATED_PROBABILITY = 0;
int TIME_BEATED = 10;
int GUIDE_TIME_BEATED = 5;

bool tripLasts = false;
bool beated = false;

volatile sig_atomic_t FORCE_END = 0;

pthread_mutex_t tab_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inviteResponses_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t myGroup_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timestamp_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t permission_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tripend_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t beated_mtx = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t inviteResponses_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t permission_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t tripend_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t beated_cond = PTHREAD_COND_INITIALIZER;

MPI_Status status;
Role currentRole;

int inviteResponses, missing, permissions;
vector<orgInfo> queue;
vector<int> reqPermissions, myGroup, invitations;
vector<processInfo> tab; // T == size??

const char* rolesNames[] = {
    "Nieznana",
    "Organizator",
    "Turysta"
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

        if (currentRole == UNKNOWN) {
          randomRole();
        }

        //println("czekam na wiadomości...\n");
        MPI_Recv( &pkt, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        pthread_mutex_lock(&timestamp_mtx);
        timestamp++;
        timestamp = max(timestamp, pkt.timestamp);
        pthread_mutex_unlock(&timestamp_mtx);

        for (size_t i = 0; i < handlers.size(); i++) {
            if (handlers[i].msgType == pkt.type) {
                pthread_mutex_lock(&tab_mtx);
                handlers[i].handler( &pkt, status.MPI_SOURCE );
                pthread_mutex_unlock(&tab_mtx);
            }
        }
        /*
        packet *newpkt = (packet*) malloc(sizeof(packet));
        memcpy(newpkt, (const char *)&pkt, sizeof(packet));
        push_pkt(newpkt, status);
        free(newpkt);
        */
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
        println("%d wasn't in my queue:[\n", id);
    pthread_mutex_unlock(&queue_mtx);
}

void reserveGuide() {
    println("GIMME GUIDE!\n");
    pthread_mutex_lock(&timestamp_mtx);
    timestamp++;
    packet msg = { timestamp, GUIDE_REQ, 0 };
    orgInfo myInfo = { timestamp, tid };

    pthread_mutex_lock(&queue_mtx);
    queue.push_back(myInfo);
    pthread_mutex_unlock(&queue_mtx);

    permissions = 0;

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
        while (reqPermissions.size() < MAX_ORGS - 1) {
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

    //pthread_mutex_lock(&queue_mtx);
    //deleteFromQueue(tid);
    //pthread_mutex_unlock(&queue_mtx);

    println("Got a Guide!\n");
}

void *waitForTripEnd(void *ptr) {
    int trip_time = rand() % 10;

    // trip_time = 0;
    sleep(trip_time);
    // println("Waiting %d secs for trip to be finished!\n", trip_time);

    pthread_mutex_lock(&timestamp_mtx);
    timestamp++;
    packet msg = { timestamp, TRIP_END, -1 };
    for (int i = 0; i < size; i++)
        if (i != tid)
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
    pthread_mutex_unlock(&timestamp_mtx);
    println("TRIP END - everyone notified.\n");

    int czy_pobity_guide = rand() % 100;
    if (czy_pobity_guide < GUIDE_BEATED_PROBABILITY) {
        println("Guide beated!\n");
        sleep(TIME_BEATED);
    }

    // pthread_mutex_lock(&tripend_mtx);
    // tripLasts = false;
    // pthread_cond_signal(&tripend_cond);
    // pthread_mutex_unlock(&tripend_mtx);

    return (void *)0;
}

void *gotBeated(void *ptr) {
    int czy_pobity = rand() % 100;
    if (czy_pobity < BEATED_PROBABILITY) {
        println("I got beated! Waiting for being healed.\n");
        sleep(TIME_BEATED);
    }
    println("Ok, I'm fine.\n");
    // pthread_mutex_lock(&beated_mtx);
    // beated = false;
    // pthread_cond_signal(&beated_cond);
    // pthread_mutex_unlock(&beated_mtx);
    return (void *)0;
}

void decideIfBeated() {
    pthread_create( &beated_th, NULL, gotBeated, 0);
    // pthread_mutex_lock(&beated_mtx);
    // beated = true;
    // pthread_mutex_unlock(&beated_mtx);

    // while (beated)
    //     pthread_cond_wait(&beated_cond, &beated_mtx);
    pthread_join(beated_th, NULL);
}

void goForTrip() {
    println("Going on a trip! :)\n");
    pthread_create( &trip_th, NULL, waitForTripEnd, 0);
    // pthread_mutex_lock(&tripend_mtx);
    // tripLasts = true;
    // pthread_mutex_unlock(&tripend_mtx);

    // while (tripLasts)
    //     pthread_cond_wait(&tripend_cond, &tripend_mtx);
    pthread_join(trip_th, NULL);
    println("I'm back from a trip :d.\n");
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

void comeBack() {
    println("Ended trip\n");

    deleteFromQueue(tid);
    pthread_mutex_lock(&queue_mtx);

    if(!queue.empty()) {
        vector<orgInfo> orgSorted;
        orgSorted.push_back(queue[0]);
        vector<orgInfo>::iterator procIt;

        size_t j;
        for (int i = 1; i < queue.size(); i++) {
            orgInfo current = queue[i];

            for (j = 0; j < orgSorted.size(); j++) {

                if ( (current.timestamp < orgSorted[j].timestamp)
                    || (current.timestamp == orgSorted[j].timestamp && current.tid < orgSorted[j].tid)) {

                    procIt = orgSorted.begin();
                    orgSorted.insert(procIt + j, current);

                    continue;
                }
            }
            if (j == orgSorted.size()) {
                orgSorted.push_back(current);
            }

        }

        pthread_mutex_unlock(&queue_mtx);
        println("(comeBack) Nailed it!\n");

        pthread_mutex_lock(&timestamp_mtx);
        packet msg = { ++timestamp, GUIDE_RESP, 0 };
        pthread_mutex_unlock(&timestamp_mtx);

        for (int i = 0; i < orgSorted.size(); i++) {
            if (orgSorted[i].tid != tid) {
                MPI_Send( &msg, 1, MPI_PAKIET_T, orgSorted[i].tid, MSG_TAG, MPI_COMM_WORLD);
                println("Ok, I let you [%d] reserve a guide\n", orgSorted[i].tid);
            } else
                println("Why am I still in the queue?\n");
            //println("[timestamp: %d, tid: %d]\n", orgSorted[i].timestamp, orgSorted[i].tid);
        }
    }
}

void orgsDeadlockProcess() {


    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses = 0;
    pthread_mutex_unlock(&inviteResponses_mtx);

    pthread_mutex_lock(&timestamp_mtx);
    timestamp++;
    missing = G-1 - myGroup.size();
    packet msg = { timestamp, INVITE, missing };                // jakis mtx?
    pthread_mutex_unlock(&timestamp_mtx);

    pthread_mutex_lock(&tab_mtx);
    int countOrgs = tabSummary();
    missing = 0;
    for (int i = 0; i < T; i++) {
        if (tab[i].role == UNKNOWN) {
            MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
            println("%d invited again... \n", i);
            missing++;
        }
    }
    pthread_mutex_unlock(&tab_mtx);

    pthread_mutex_lock(&inviteResponses_mtx);
    while (inviteResponses != missing)
        pthread_cond_wait(&inviteResponses_cond, &inviteResponses_mtx);
    pthread_mutex_unlock(&inviteResponses_mtx);
    
    println("[deadlock] Proceeding to sort.\n");

    pthread_mutex_lock(&tab_mtx);
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

    for (i = 0; i < T; i++) {
        println("%d. id %d: %s, %d\n", i, indices[i], rolesNames[procSorted[i].role], procSorted[i].value);
    }
    // END OF INSERTION SORT
    println("[deadlock] Sorted tab.\n");

    if (countOrgs > MAX_ORGS) {

        // nadmiarowi organizatorzy rezygnują i mogą dołączać
        for (i = MAX_ORGS; i < T; i ++) {
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
                }
            }
        }

        for (i = 0; i < MAX_ORGS; i++) {
            int turNeeded = procSorted[i].value;
            if (turNeeded == 0) continue;

            for (j = MAX_ORGS; j < T; j++) {
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

        for (i = 0; i < T; i++)
            println("* %d. id %d: %s, %d\n", i, indices[i], rolesNames[procSorted[i].role], procSorted[i].value);

    }

    if (currentRole == ORG) {
        myGroup.clear();
        for (i = 0; i < T; i++) {
            if (procSorted[i].role == TUR && procSorted[i].value == tid) {
                myGroup.push_back(indices[i]);
            }
        }
    }

    pthread_mutex_unlock(&tab_mtx);
}

void *orgThreadFunction(void *ptr) {

    packet pkt;
    tab[tid].role = ORG;

    invitations.clear();
    myGroup.clear();

    while (myGroup.size() != G-1) {

        if (FORCE_END) {
          clearResources();
          return (void *)0;
        }

        if (invitations.size() == T-1) {

            println("Oh no, deadlock occured.\n");
            orgsDeadlockProcess();
            invitations.clear();
            if (currentRole == TUR) {
                println("[deadlock solved] I'm not ORG anymore...\n");
                return (void *)0;
            }
            println("[deadlock solved] My group is %d now.\n", myGroup.size());
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
    invitations.clear();
    println("I've got a group!\n");

    reserveGuide();
    goForTrip();
	comeBack();
    decideIfBeated();               // decyduje o tym dopiero po powiadomieniu innych ze koniec wycieczki, czy blokuje kolejne?
    
    currentRole = UNKNOWN;
    
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
        println("Setting new role: %s\n", rolesNames[currentRole]);

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

}

int main(int argc, char **argv) {

    signal(SIGINT, &interruptHandler); // to niestety nie działa :/
    
    if (argc == 4) {
        T = atoi(argv[1]);
        G = atoi(argv[2]);
        P = atoi(argv[3]);
    }
    MAX_ORGS = T / G;

    init(&argc, &argv);
    srand(time(NULL) + tid);
    prepare();

    pthread_create( &receiver_th, NULL, receiveMessages, 0);
    pthread_join( receiver_th, NULL );

    MPI_Finalize();
    // test
}
