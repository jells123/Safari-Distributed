#include "handlers.h"
#include "turysta.h"

std::vector<messageHandler> handlers;

void addMessageHandler(MsgType type, void (*handler)(packet*, int)) {
    messageHandler newHandler = {
        type,
        handler
    };

    handlers.push_back(newHandler);
    return;
}

void omg_deadlockHandler(packet *pkt, int src) {

	if (currentRole == ORG) {
		pthread_mutex_lock(&deadlock_mtx);

	    deadlocks++;
	    if (lonelyOrgs == 0)
	    	lonelyOrgs = pkt->info_val;
	    // println("Deadlock Message from %d, %d/%d\n", src, deadlocks, lonelyOrgs);

	    if (deadlocks >= lonelyOrgs) {
	        pthread_cond_signal(&deadlock_cond);
	        deadlocks = 0;
	        lonelyOrgs = 0;
	    }

	    pthread_mutex_unlock(&deadlock_mtx);
	}
	else {
		// println("LOL I'm not even an ORG. xd\n");
	}

}


void inviteHandler(packet *pkt, int src) {

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;

    if (beated) {
        packet msg = { timestamp, I_WAS_BEATED, 0 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
    }
    else if (currentRole == TUR && myGroup.empty()) {
        myGroup.push_back(src);
        packet msg = { timestamp, ACCEPT, 0 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
        // tab[src].value--; //?
        println("Yay! Invitation from %d! ACCEPT :) (value %d)\n", src, pkt->info_val);
    }
    else if (currentRole == TUR && !myGroup.empty()) {
        packet msg = { timestamp, REJECT_HASGROUP, myGroup[0] };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
        println("Yay! Invitation from %d! REJECT_HASGROUP[%d] (value %d)\n", src, myGroup[0], pkt->info_val);
    }
    else if (currentRole == ORG) {
        packet msg = { timestamp, REJECT_ISORG, (int) (G - 1 - myGroup.size()) };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
        println("Yay! Invitation from %d! REJECT_ISORG[%d] (value %d)\n", src, (int) (G - 1 - myGroup.size()), pkt->info_val);
        
    }
    else {
        println("InviteHandler - none of these above? Role %d\n", currentRole);
    }

}


void change_groupHandler(packet *pkt, int src) {

	/*
		To trzeba natychmiast przestać organizować!
	*/

	if (currentRole == ORG) {
        return;
        
		println("I was org but I have to cancel it :< \n");

		for (size_t i = 0; i < myGroup.size(); i++) {
			if (tab[myGroup[i]].role != ORG)
				tab[myGroup[i]].value = -1;
		}

	}

	currentRole = TUR;
    tab[tid].role = TUR;
    tab[tid].value = pkt->info_val;

    if (myGroup.size() > 0) {
    	if (myGroup[0] != pkt->info_val) {
	        println("Group change! From %d to %d (%d told me!)\n", myGroup[0], pkt->info_val, src);
    	}
    	else {
    		// println("Group change - i'm already in that group...\n");
    	}
    }
    else {
    	// println("Got group change (from %d, change to %d) but I have no group? [size is %d] \n", src, pkt->info_val, (int) myGroup.size());
    }

	myGroup.clear();
    myGroup.push_back(pkt->info_val);
    tab[pkt->info_val].role = ORG;

	// }

}


void not_orgHandler(packet *pkt, int src) {

	if (tab[src].role == ORG) {
		// jakiś organizator zrezygnował...
		for (int i = 0; i < T; i++) {
			if (tab[i].value == src && tab[i].role == TUR) {
				tab[i].value = -1;
			}
		}
	}

    tab[src].role = TUR;
    tab[src].value = -1;

    int touristsCount = 0;
    for (int i = 0; i < size; i++) {
        if (tab[i].role == TUR)
            touristsCount++;
    }


    // orgsNumber = countOgrs();
    int maxOrgs = countMaxOrgs();
    println("%d says it's not ORG... (touristsCount: %d, maxOrgs: %d)\n", src, touristsCount, maxOrgs);


    if (currentRole == TUR
        && T - touristsCount < maxOrgs
        && tid <= maxOrgs
        // && maxOrgs - (T - touristsCount) >= tid
        && myGroup.empty() ) 
    {
        currentRole = ORG;
        println("I became ORG! Because I could.\n");

    }

}


void reject_isorgHandler(packet *pkt, int src) {

    println("%d says it's org too (value %d)\n", src, pkt->info_val);

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;
    inviteResponses++;

}


void reject_hasgroupHandler(packet *pkt, int src) {

    println("%d says it has a group [%d]\n", src, pkt->info_val);

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;

    tab[pkt->info_val].role = ORG;
    inviteResponses++;

    // println("%d already has group -> %d :/\n", src, pkt->info_val);
    // if (pkt->info_val == tid && currentRole == ORG) {
    // 	// trochę wiksa...
    // 	size_t i;
    // 	for (i = 0; i < myGroup.size(); i++) {
    // 		if (myGroup[i] == src) {
    // 			break;
    // 		}
    // 	}
    // 	if (i == myGroup.size())
    // 		myGroup.push_back(src);
    // }
}

void i_was_beatedHandler(packet *pkt, int src) {

    tab[src].role = BEATED;
    tab[src].value = -666;
    inviteResponses++;

}


void acceptHandler(packet *pkt, int src) {

    myGroup.push_back(src);

    tab[src].role = TUR;
    tab[src].value = tid;

    println("%d joining my group!", src);
    inviteResponses++;

}


void guide_reqHandler(packet *pkt, int src) {
    tab[src].role = ORG;

    if(currentRole == ORG) {
        if (imOnTrip == false) {
            size_t groupSize = G-1;

            if(myGroup.size() != groupSize) {
                packet msg = { timestamp, GUIDE_RESP, -1 };
                MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
                println("Guide Request from %d - not interested\n", src);

            } else if(myGroup.size() == groupSize
                && (pkt->timestamp < timestamp || (pkt->timestamp == timestamp
                    && src < tid))) {
                println("Guide Request from %d - request OK\n", src);

                packet msg = { timestamp, GUIDE_RESP, 0 };
                MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);            

            } else {

                println("Guide Request from %d - add to queue\n", src);

                orgInfo hisInfo = { pkt->timestamp, src };
                queue.push_back(hisInfo);
            }

        } else {

            println("Guide Request from %d - I'm trippin'\n", src);

            orgInfo hisInfo = { pkt->timestamp, src };
            queue.push_back(hisInfo);
        }
    }
}

void guide_respHandler(packet *pkt, int src) {
    tab[src].role = ORG;

    if(imOnTrip == false) {
        if(currentRole == ORG && (pkt->timestamp >= lastReqTimestamp)) {
            // orgsNumber = countOgrs();

            // if(pkt->info_val == 0) {
            permissions++;
            println("Got permission from [%d]\n", src);

            // } else if(pkt->info_val == -1) {
                // println("Got it but %d not interested...\n", src);
                // notInterestedOgrs++;
            // }

            // println("Number of ogrs: %d, number of not interested: %d, my permissions: %d\n", orgsNumber, notInterestedOgrs, permissions);

        } else {
            println("Response out of date, timestamp: %d, request timestamp: %d \n", pkt->timestamp, lastReqTimestamp);
        }
    }
}


void trip_endHandler(packet *pkt, int src) {

    println("Trip End message from %d!\n", src);

    for (int i = 0; i < size; i++) {
        if ( (tab[i].role == TUR && tab[i].value == src)
                || (i == src) ) {
            tab[i].role = UNKNOWN;
            tab[i].value = -1;
        }
    }
    if(currentRole == ORG) {
        deleteFromQueue(src);
    }

    // if (src == tid) {
    //     myGroup.clear();
    //     decideIfBeated();
    //     currentRole = UNKNOWN;
    //     // println("End of my own trip notification. \n");
    //     randomRole();
    // }
    // else 
    if (currentRole == TUR 
        && !myGroup.empty() 
        && myGroup[0] == src) 
    {
        myGroup.clear();
        decideIfBeated();
        currentRole = UNKNOWN;
        // println("End of %ds trip notification, which I belong to (TUR) \n", src);
        randomRole();
	}
	else {
    	// println("End of %ds trip notification. \n", src);
	}

}
