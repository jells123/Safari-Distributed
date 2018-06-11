#ifndef TURYSTAH
#define TURYSTAH

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <mpi.h>

#include <time.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <set>
#include <algorithm>
#include <csignal>
#include <unistd.h>

#include "packet.h"
#include "inits.h"
//#include "constants.h"
#include "handlers.h"

using namespace std;

enum Role {
    UNKNOWN = 0,
    ORG = 1, // organizator
    TUR = 2, // turysta
    BEATED = 3
};

typedef struct processInfo {
    Role role;
    int value;
} processInfo;

typedef struct orgInfo {
    int timestamp;
    int tid;
} orgInfo;

extern Role currentRole;
extern MPI_Status status;
extern int T, G, P, MAX_ORGS, lastReqTimestamp;

extern int inviteResponses, awaitingResponsesCount;
extern int missing, permissions, lonelyOrgs, deadlocks, orgsNumber;

extern pthread_mutex_t tab_mtx, inviteResponses_mtx, myGroup_mtx, timestamp_mtx, queue_mtx, permission_mtx, beated_mtx, state_mtx, deadlock_mtx;
extern pthread_cond_t inviteResponses_cond, permission_cond, deadlock_cond;

extern vector<processInfo> tab;
extern vector<orgInfo> queue;
extern vector<int> reqPermissions, myGroup, invitations;

extern int ROOT, MSG_TAG, ORG_PROBABILITY, GUIDE_BEATED_PROBABILITY, BEATED_PROBABILITY, TIME_BEATED, GUIDE_TIME_BEATED;
extern volatile sig_atomic_t FORCE_END;

extern bool beated, deadlock_trouble, imOnTrip, reqSent;

void *receiveMessages(void *ptr);
void deleteFromQueue(int tid);
void reserveGuide();
int tabSummary();
void comeBack();

void orgsDeadlockProcess();
void *orgThreadFunction(void *ptr);
void randomRole();
void prepare();
void interruptHandler(int s);
void clearResources();

void *gotBeated(void *ptr);
void decideIfBeated();
int countBeated();
int countOgrs();

void doOrgWork();
vector<int> getPossibleInvitations();
void deadlockTrouble();
void tripFinito();
void *waitForTripEnd(void *ptr); 
int countMaxOrgs();

int main(int argc, char * * argv);

#endif
