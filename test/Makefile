all: a.out client.out server.out


a.out: test.c
	gcc -Wall test.c -o a.out 

client.out: client.c test.h
	gcc -Wall client.c -o client.out

server.out: server.c test.h
	gcc -Wall -lpthread server.c -o server.out


clean:
	rm *.out
