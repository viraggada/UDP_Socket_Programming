CC=gcc
CFLAGS=-Wall -Werror -g -O0
LFLAGS=-Wl -Map=main.map

CFOLDER=clientf
SFOLDER=serverf
SSRCS=$(SFOLDER)/server.c
CSRCS=$(CFOLDER)/client.c

PROGRAMS=$(SFOLDER)/server $(CFOLDER)/client

#Default make command builds executable file
all: $(PROGRAMS)

$(SFOLDER)/server: $(SSRCS)
	$(CC) $(CFLAGS) $^ -o $@

$(CFOLDER)/client: $(CSRCS)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -rf $(PROGRAMS)
