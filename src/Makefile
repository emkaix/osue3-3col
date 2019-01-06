#------------------------------------------------------------
# @file Makefile
# @author Klaus Hahnenkamp <e11775823@student.tuwien.ac.at>
# @date 06.01.2019
# program: generator, supervisor
#------------------------------------------------------------

params = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L

all: generator supervisor

generator: generator.o
	gcc $(params) -g -o generator generator.o

generator.o: generator.c
	gcc $(params) -g -o generator.o -c generator.c

supervisor: supervisor.o
	gcc $(params) -g -o supervisor supervisor.o

supervisor.o: supervisor.c
	gcc $(params) -g -o supervisor.o -c supervisor.c


clean:
	rm -rf *.o intmul
