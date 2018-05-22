#include "handlers.h"
//#include "turysta.h"

std::vector<messageHandler> handlers;

void addMessageHandler(MsgType type, void (*handler)(packet*, int)) {
    messageHandler newHandler = {
        type,
        handler
    };

    handlers.push_back(newHandler);
    return;
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
    if (myGroup.size() > 0 && myGroup[0] != src) {
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
    pthread_mutex_lock(&inviteResponses_mtx);
    inviteResponses++;

    tab[src].role = TUR;
    tab[src].value = pkt->info_val;
    tab[pkt->info_val].role = ORG;

    println("%d already has group :/\n", src);

    if (inviteResponses >= missing || FORCE_END == 1) {
        pthread_cond_signal(&inviteResponses_cond);
    }

    pthread_mutex_unlock(&inviteResponses_mtx);

}

void acceptHandler(packet *pkt, int src) {
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

        //if(givenResp < P &&
        if(myGroup.size() != G-1 || (myGroup.size() == G-1
            && (pkt->timestamp < timestamp || (pkt->timestamp == timestamp
                && src < tid)))) {

            pthread_mutex_lock(&timestamp_mtx);
            timestamp++;
            pthread_mutex_unlock(&timestamp_mtx);

            packet msg = { timestamp, GUIDE_RESP, 0 };
            MPI_Send( &msg, 1, MPI_PAKIET_T, src, MSG_TAG, MPI_COMM_WORLD);
            println("Ok, I let you [%d] reserve a guide\n", src);

            //givenResp++;

        } else {
            //println("I won't let you [%d] reserve a guide! For now.. My timestamp: %d, his: %d\n", src, timestamp, pkt->timestamp);
            println("I won't let you [%d] reserve a guide! For now..\n", src);

        }


    } else {
        println("I'm not an ogr, sth went wrong...\n");
    }
}

void guide_respHandler(packet *pkt, int src) {
    pthread_mutex_lock(&permission_mtx);
    permissions++;
    if (permissions >= (MAX_ORGS - P) || FORCE_END == 1) {
    	pthread_cond_signal(&permission_cond);
    }
    println("Got permission from [%d]\n", src);
    pthread_mutex_unlock(&permission_mtx);
}

void trip_endHandler(packet *pkt, int src) {
  println("End of %ds trip notification. \n", src);
	tab[src].role = UNKNOWN;
	tab[src].value = -1;

	for (int i = 0; i < size; i++) {
		if (tab[i].role == TUR && tab[i].value == src) {
			tab[i].role = UNKNOWN;
			tab[i].value = -1;
		}
	}

  if (currentRole == TUR && !myGroup.empty() && myGroup[0] == src) {
    myGroup.clear();
    currentRole = UNKNOWN;
  }
	// ...
  deleteFromQueue(src);
}
