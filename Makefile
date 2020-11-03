all: programes	# Aquesta regla compila tots els codis.

LDFLAGS += -lRTIMULib	# Per tal de compilar el codi orientacio.cpp

programes: tensio.o orientacio


tensio.o: tensio.c
	gcc tensio.c -o tensio


orientacio : orientacio.cpp


clean:
	rm -f programes *.o
