CFLAGS = -std=c17 -O2

SRCS = main.c

compile: $(SRCS)
	gcc $(CFLAGS) -g -o $@ $(SRCS) $(LDFLAGS) -lncurses

exec: compile
	./compile

clean:
	rm -f compile