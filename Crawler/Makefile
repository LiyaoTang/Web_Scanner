# Makefile for comp3310 tcp/udp lab

PROGS=crawler

all: $(PROGS)

%: %.c
	gcc -Wall -o $* $*.c -lm # -std=gnu11 -D_XOPEN_SOURCE=700
run:
	./${PROGS} 

clean:
	rm -f $(PROGS)
