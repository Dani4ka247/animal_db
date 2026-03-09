CC=clang
CFLAGS=-Wall -Wextra -std=c99 -O2
LDLIBS=-ledit

SOURCES=main.c cli.c animal.c db.c commands.c tokens.c
OBJECTS=$(SOURCES:.c=.o)

all: shelter

shelter: $(OBJECTS)
    $(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

clean:
    rm -f *.o shelter

.PHONY: all clean
