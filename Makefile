#Makefile de la Fita 4 pel codi que governa els sensors d'intensitat i tensió.
#Implementat l'ús de timers i de llibreries dinàmiques.

#CC = gcc
#OBJECTS = func.o libfunc.so

#all: $(OBJECTS) orientacio.o

#func.o:
#	$(CC) -fPIC -c -o func.o func.c

#libfunc.o
#	$(CC) -shared -fPIC -o libfunc.so func.o

#orientacio.o
#	$(CC) -c -o mesurafont.o mesurafont.c -l.
#	$(CC) -o mesurafont mesurafont.o -lsqlite3 -lrt -lpthread  -L. -lfunc
#	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/pi/Desktop/GIT/MesuraTensioIntensitatBateria
#	./mesurafont

#clean:
#	rm *.o


all:
	gcc -fPIC -c -o func.o func.c
	gcc -shared -fPIC -o libfunc.so func.o
	gcc -c -o mesurafont.o mesurafont.c -l.
	gcc -o mesurafont mesurafont.o -lsqlite3 -lrt -lpthread  -L. -lfunc
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/pi/Desktop/GIT/MesuraTensioIntensitatBateria

clean:
	rm *.o
