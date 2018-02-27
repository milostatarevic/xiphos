
CC = gcc
CFLAGS = -O3 -flto -mbmi2 -Wall -lm -lpthread

TARGET = xiphos
SRCS = src/*.c

$(TARGET):
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm $(TARGET)

