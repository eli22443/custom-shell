CC = gcc
CFLAGS = -m32 -Wall -g

all: mypipeline myshell

mypipeline: mypipeline.c
	$(CC) $(CFLAGS) mypipeline.c -o mypipeline

myshell: myshell.c LineParser.c LineParser.h
	$(CC) $(CFLAGS) myshell.c LineParser.c -o myshell

looper: looper.c
	$(CC) $(CFLAGS) looper.c -o looper

clean:
	rm -f mypipeline myshell