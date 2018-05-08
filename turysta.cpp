#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include <time.h>
#include <iostream>
#include <cstdlib>

#define ROOT 0
#define MSG_TAG 100

using namespace std;

enum Role { O, T };

typedef struct info {
  int type;
  int value;
} Info;

int T = 10;
int G = 2;
int P = 3;

int main(int argc,char **argv)
{
    int size,tid;

    if(argc == 4) {
      T = atoi(argv[1]);
      G = atoi(argv[2]);
      P = atoi(argv[3]);
    }

    Info tab[T];

    cout << T << " " << G << " " << P << endl;

    MPI_Init(&argc, &argv);

    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &tid );

		//MPI_Recv( &res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		//MPI_Send( &res, 1, MPI_INT, ROOT, MSG_TAG, MPI_COMM_WORLD );

    //while(true) {

    //}

    MPI_Finalize();
}
