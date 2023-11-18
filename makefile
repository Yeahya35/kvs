CC=gcc
CFLAGS=-pthread -lrt

all: serverk clientk

serverk: serverk.o
	$(CC) serverk.o -o serverk $(CFLAGS)

clientk: clientk.o
	$(CC) clientk.o -o clientk $(CFLAGS)

serverk.o: serverk.c hashtable.h requestQueue.h request.h KeyValuePair.h
	$(CC) -c serverk.c $(CFLAGS)

clientk.o: clientk.c request.h
	$(CC) -c clientk.c $(CFLAGS)

clean:
	rm -f serverk clientk *.o
