
CC = gcc
CFLAGS = -O3 -flto -Wall
LIBS = -lm -lpthread

TARGET = xiphos
SRCS = src/*.c

bmi2:
	$(CC) $(CFLAGS) -mbmi2 $(SRCS) -o $(TARGET)-bmi2 $(LIBS)

magic:
	$(CC) $(CFLAGS) -msse -D_MAGIC $(SRCS) -o $(TARGET)-magic $(LIBS)

clean:
	rm $(TARGET)
