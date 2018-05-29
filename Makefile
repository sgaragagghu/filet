CC = gcc
CFLAGS = -g -Wall -Wextra -O2
SRC = noise.o filet_server.o filet_thread_server.o filet_client.o filet_thread_client.o echo_io.o list.o protocollo.o utils.o
OBJ = $(SRC:.c=.o)
LIBS ?= -lpthread

all: $(OBJ) 
	${CC} ${CFLAGS} noise.o utils.o filet_server.o echo_io.o  list.o protocollo.o filet_thread_server.o $(LIBS) -o filet_server
	${CC} ${CFLAGS} noise.o utils.o filet_client.o echo_io.o filet_thread_client.o list.o protocollo.o $(LIBS) -o filet_client

noise.c:noise.h
utils.o:utils.h
echo_io.o: echo_io.h
list.o: list.h
protocollo.o: protocollo.h
filet_thread_server.o: filet_thread_server.h
filet_thread_client.o: filet_thread_client.h




clean:
	rm -f *.o core 

cleanall:
	rm -f *.o core filet_server filet_client
