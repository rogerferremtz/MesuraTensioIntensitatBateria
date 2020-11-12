all: mesurafont.o
	gcc mesurafont.c -lsqlite3 -o mesurafont

clean:
	rm -f mesurafont.o
