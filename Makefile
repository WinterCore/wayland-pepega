CFLAGS = -std=c11 -Wall -Wextra
LDFLAGS = -lwayland-client -lrt

.PHONY: clean all executable

all: executable

executable: Run
	./Run

Run: src/main.c src/xdg-shell-client-protocol.h
	cc $(CFLAGS) -o Run src/main.c src/xdg-shell-protocol.c $(LDFLAGS)
