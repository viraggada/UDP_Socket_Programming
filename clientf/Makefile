CC=gcc
CFLAGS=-Wall -Werror -g -O0
LFLAGS=-Wl -Map=main.map

CSRCS=client.c

PROGRAMS=client

#Default make command builds executable file
all: $(PROGRAMS)

client: $(CSRCS)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -rf $(PROGRAMS)
