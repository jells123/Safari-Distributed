all: safari

safari: stack.o turysta.o inits.o handlers.o
	mpic++ turysta.o stack.o inits.o handlers.o -o safari

turysta.o: turysta.cpp turysta.h
	mpic++ turysta.cpp -c -Wall -pthread

inits.o: inits.cpp inits.h
	mpic++ inits.cpp -c -Wall -pthread

stack.o: stack.cpp stack.h
	mpic++ stack.cpp -c -Wall -pthread

handlers.o : handlers.cpp handlers.h
	mpic++ handlers.cpp -c -Wall -pthread


clear: 
	rm *.o safari
