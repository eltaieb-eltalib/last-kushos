# Cross-compile kushagent.exe with mingw-w64 (Linux host) or natively with MinGW on Windows.
CC64 ?= x86_64-w64-mingw32-gcc
CC32 ?= i686-w64-mingw32-gcc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -std=c99
LIBS = -luser32 -lkernel32

all: kushagent64.exe

kushagent64.exe: kushagent.c protocol.c protocol.h
	$(CC64) $(CFLAGS) -s -static -o $@ kushagent.c protocol.c $(LIBS)

kushagent32.exe: kushagent.c protocol.c protocol.h
	$(CC32) $(CFLAGS) -s -static -o $@ kushagent.c protocol.c $(LIBS)

# Host-side unit test of the portable protocol code
test: test_protocol
	./test_protocol

test_protocol: test_protocol.c protocol.c protocol.h
	cc $(CFLAGS) -o $@ test_protocol.c protocol.c

clean:
	rm -f kushagent64.exe kushagent32.exe test_protocol
