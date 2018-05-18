#include "inits.h"

pthread_t sender_th, receiver_th;
int size, tid, timestamp;

void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE:
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
    	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
    	    MPI_Finalize();
    	    exit(-1);
	        break;
        case MPI_THREAD_FUNNELED:
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	        break;
        case MPI_THREAD_SERIALIZED:
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	        break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
	        break;
        default: 
            printf("Nikt nic nie wie\n");
    }
}

MPI_Datatype MPI_PAKIET_T;
void init(int *argc, char ***argv)
{
    int provided;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    const int nitems=3;
    int blocklengths[3] = {1,1,1};
    MPI_Datatype typy[3] = {MPI_INT, MPI_INT, MPI_INT};
    MPI_Aint offsets[3];

    offsets[0] = offsetof(packet, timestamp);
    offsets[1] = offsetof(packet, type);
    offsets[2] = offsetof(packet, info_val);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &tid);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(tid);
}
