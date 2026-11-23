CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2

all: fire fire4 fire_safe

fire: fire.c
	$(CC) $(CFLAGS) -o fire fire.c

fire4: fire4.c
	$(CC) $(CFLAGS) -o fire4 fire4.c

fire_safe: fire_safe.c
	$(CC) $(CFLAGS) -o fire_safe fire_safe.c

clean:
	rm -f fire fire4 fire_safe results.txt

