all: safari

safari: stack.o turysta.o inits.o
	mpic++ turysta.o stack.o inits.o -o safari

turysta.o: turysta.cpp
	mpic++ turysta.cpp -c -Wall

stack.o: stack.cpp stack.h
	mpic++ stack.cpp -c -Wall

inits.o: inits.cpp inits.h
	mpic++ inits.cpp -c -Wall

clear: 
	rm *.o safari
