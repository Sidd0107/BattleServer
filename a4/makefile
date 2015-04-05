CC = gcc
PORT=17147
CFLAGS = -DPORT=$(PORT) -g -Wall

all: battle

battle: battleserver.c
	${CC} ${CFLAGS} -o $@ battleserver.c  

