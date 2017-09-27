CC=gcc
CFLAGS=-Wall -fcilkplus
LIBS=-lcilkrts

TARGET=othello

all: $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) -o $@ $< $(LIBS)

# Implicit rule already exists
# %.o: %.c
#	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f *.o $(TARGET)
