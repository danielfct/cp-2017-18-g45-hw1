CC=gcc
CFLAGS=-Wall
LIBS=-lpthread

TARGET=othello

all: $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) -o $@ $< $(LIBS)

clean:
	rm -f *.o $(TARGET)
