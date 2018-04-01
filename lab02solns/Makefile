# Makefile for comp3310 tcp/udp lab

PROGS=serverTCP serverUDP clientTCP clientUDP clientAddr

all: $(PROGS)

%: %.c
	gcc -Wall -o $* $*.c
%.class: %.java
	javac $*.java

clean:
	rm -f $(PROGS) *.class
