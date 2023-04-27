CFLAGS = -std=c11 -Wall -Wextra
LDFLAGS = -lwayland-client -lrt -lm

.PHONY: clean all exec

all: exec

exec: Run
	./Run

Run: src/main.c src/xdg-shell-client-protocol.h
	cc $(CFLAGS) -o Run src/main.c src/xdg-shell-protocol.c $(LDFLAGS)
