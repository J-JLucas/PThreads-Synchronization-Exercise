# ------------------------------------------------------------
# Makefile for Synchronization Exercise 
# by Josh Lucas
# Note: time was included to verify my program timer correct
# ------------------------------------------------------------

synchro: synchro.c
	gcc -Wall -pthread -o synchro synchro.c 

seq: seq.c
	gcc -Wall -pthread -o seq seq.c 

runSimulation:
	time ./synchro test.dat 200 20

runSimulation2:
	time ./synchro test2.dat 200 20

runSimulationNasty:
	time ./synchro nasty.dat 200 20

runSimulationNasty2:
	time ./synchro nasty2.dat 200 20

runSeq:
	time ./seq test.dat 200 5

runSeqNasty:
	time ./seq nasty.dat 200 20

runSeqNasty2:
	time ./seq nasty2.dat 200 20

runSimulationVal:
	valgrind --fair-sched=yes --leak-check=full ./synchro test.dat 200 20

clean:
	rm *~ out.* *.o
