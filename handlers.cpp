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
		println("LOL I'm not even an ORG. xd\n");
	}

}


void inviteHandler(packet *pkt, int src) {
	// println("invite handler\n");

    if (beated) {
        packet msg = { timestamp, I_WAS_BEATED, 0 };
        sleep(1);
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
    	// println("Responding with I was beated to %d\n", src);
    }
    else if (currentRole == TUR && myGroup.empty()) {
        myGroup.push_back(src);
        packet msg = { timestamp, ACCEPT, 0 };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
    	// println("Sending ACCEPT for %d\n", src);
    }
    else if (currentRole == TUR && !myGroup.empty()) {
        packet msg = { timestamp, REJECT_HASGROUP, myGroup[0] };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
    	// println("Sending 'i have a group (%d) ' to %d\n", myGroup[0], src);
    }
    else if (currentRole == ORG) {
        packet msg = { timestamp, REJECT_ISORG, (int) (G - myGroup.size() - 1) };
        MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
    	// println("Sending 'i am org too' to %d\n", src);
    }
    else {
    	// println("InviteHandler - responded with nothing? Role %d\n", currentRole);
    }

    tab[src].role = ORG;
}


void change_groupHandler(packet *pkt, int src) {
	// println("change_group handler\n");

	/*
		To trzeba natychmiast przestać organizować!
	*/

	if (currentRole == ORG) {
		println("I was org but I have to cancel it :< \n");

		for (size_t i = 0; i < myGroup.size(); i++) {
			if(tab[myGroup[i]].role != ORG)
				tab[myGroup[i]].value = -1;
		}

	}
	// else {

	currentRole = TUR;

    tab[tid].role = TUR;
    tab[tid].value = pkt->info_val;

    if (myGroup.size() > 0) {
    	if (myGroup[0] != pkt->info_val) {
	        println("Group change! From %d to %d\n", myGroup[0], pkt->info_val);
	        // myGroup.clear();
    	}
    	else {
    		println("Group change - i'm already in that group...\n");
    	}
    } 
    else {
    	println("Got group change (from %d, change to %d) but I have no group? [size is %d] \n", src, pkt->info_val, (int) myGroup.size());
    }

	myGroup.clear();
    myGroup.push_back(pkt->info_val);
    tab[pkt->info_val].role = ORG;

	// }
	
}


void not_orgHandler(packet *pkt, int src) {

    println("Oczymaem od [%d]\n", src);

	if (tab[src].role == ORG) {
		// jakiś organizator zrezygnował...
		for (int i = 0; i < size; i++) {
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

    int maxOrgs = (T - countBeated()) / G;

    println("Turystow jest: %d, jestem %d, myGroup.size: %d\n", touristsCount, currentRole, myGroup.size());

    if (currentRole == TUR
        && T - touristsCount < maxOrgs
        && maxOrgs - (T - touristsCount) >= tid
        && myGroup.empty() ) {
        currentRole = ORG;
        println("I became ORG! Because I could.\n");

        if (currentRole == ORG)
            pthread_create( &sender_th, NULL, orgThreadFunction, 0 );
    }

}


void reject_isorgHandler(packet *pkt, int src) {

    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = ORG;
    tab[src].value = pkt->info_val;

    println("%d is org too. \n", src);

    if (inviteResponses >= missing || FORCE_END == 1) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);
}


void reject_hasgroupHandler(packet *pkt, int src) {
	// println("reject_hasgroup handler\n");

    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;
    tab[pkt->info_val].role = ORG;

    println("%d already has group -> %d :/\n", src, pkt->info_val);
    if (pkt->info_val == tid && currentRole == ORG) {
    	// trochę wiksa...
    	size_t i;
    	for (i = 0; i < myGroup.size(); i++) {
    		if (myGroup[i] == src) {
    			break;
    		}
    	}
    	if (i == myGroup.size())
    		myGroup.push_back(src);
    }

    if (inviteResponses >= missing || FORCE_END == 1) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);

}

void i_was_beatedHandler(packet *pkt, int src) {
	// println("beated handler\n");

    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = BEATED;
    tab[src].value = -666;

    println("%d was beated :ooo\n", src);

    if (inviteResponses >= missing || FORCE_END == 1) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);
}


void acceptHandler(packet *pkt, int src) {
	// println("accept handler\n");

	pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;
    myGroup.push_back(src);

    tab[src].role = TUR;
    tab[src].value = tid;

    println("%d joining my group!", src);

    if (inviteResponses >= missing || FORCE_END == 1) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);
}


void guide_reqHandler(packet *pkt, int src) {
	tab[src].role = ORG;

    if (currentRole == ORG) {

        orgInfo hisInfo = { pkt->timestamp, src };

        pthread_mutex_lock(&queue_mtx);
        queue.push_back(hisInfo);
        pthread_mutex_unlock(&queue_mtx);

        size_t groupSize = G-1;

        if(myGroup.size() != groupSize || (myGroup.size() == groupSize
            && (pkt->timestamp < timestamp || (pkt->timestamp == timestamp
                && src < tid)))) {

            pthread_mutex_lock(&timestamp_mtx);
            packet msg = { ++timestamp, GUIDE_RESP, 0 };
            pthread_mutex_unlock(&timestamp_mtx);


            MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
            println("Ok, I let you [%d] reserve a guide\n", src);


        } else {
            println("I won't let you [%d] reserve a guide! For now..\n", src);
        }

    } else {
        println("I'm not an ogr anymore...\n");
    }
}

void guide_respHandler(packet *pkt, int src) {
    if(currentRole == ORG && (pkt->timestamp >= lastReqTimestamp)) {
        pthread_mutex_lock(&permission_mtx);
        permissions++;
        if (permissions >= (MAX_ORGS - P) || FORCE_END == 1) {
        	pthread_cond_signal(&permission_cond);
        }
        println("Got permission from [%d]\n", src);
        pthread_mutex_unlock(&permission_mtx);
    } else {
        println("Response out of date, timestamp: %d, request timestamp: %d \n", pkt->timestamp, lastReqTimestamp);
    }
}


void trip_endHandler(packet *pkt, int src) {


    for (int i = 0; i < size; i++) {
        if ( (tab[i].role == TUR && tab[i].value == src)
                || (i == src) ) {
            tab[i].role = UNKNOWN;
            tab[i].value = -1;
        }
    }

    if(currentRole == ORG && src != tid)
        deleteFromQueue(src);
    
    if (src == tid) {
        decideIfBeated();
        currentRole = TUR;
        println("End of my own trip notification. \n"); 
        myGroup.clear();
        packet msg = { ++timestamp, NOT_ORG, 0 };
        for (int i = 0; i < size; i++)
            if (i != tid)
                MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
        //randomRole();
        println("Wrocile, jeste turysta\n");
    }
    else if (currentRole == TUR && !myGroup.empty() && myGroup[0] == src) {
        myGroup.clear();
        //currentRole = UNKNOWN;
        decideIfBeated();
        println("End of %ds trip notification, which I belong to (TUR) \n", src);   
        packet msg = { ++timestamp, NOT_ORG, 0 };
        for (int i = 0; i < size; i++)
            if (i != tid)
                MPI_Send( &msg, 1, MPI_PAKIET_T, i, MSG_TAG, MPI_COMM_WORLD);
		//randomRole();
        println("Wrocile, jeste turysta\n");
	}
	else {
    	println("End of %ds trip notification. \n", src);	
	}

}
