CC = gcc
LDFLAGS = `pkg-config --libs gstreamer-1.0`
CFLAGS += -g -Wall `pkg-config --cflags gstreamer-1.0`

all: multi-src multi-sink

multi-src: multi-src.o
	$(CC) $^ -o $@ $(LDFLAGS)

multi-sink: multi-sink.o
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean

clean:
	rm multi-src multi-sink *.o
