# HOLLOW MOON — build with: `make` ; run: `./hollow_moon`
CC      ?= cc
CFLAGS  ?= -O2 -Wall
LDLIBS   = -lm

hollow_moon: hollow_moon.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

run: hollow_moon
	./hollow_moon

clean:
	rm -f hollow_moon hollow_moon.avi

.PHONY: run clean
