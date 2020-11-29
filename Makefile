all: mesurafont.o
	gcc mesurafont.c -lsqlite3 -lrt -lpthread -o mesurafont

clean:
	rm -f mesurafont.o
