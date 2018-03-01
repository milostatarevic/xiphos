
CC = gcc
CFLAGS = -O3 -flto -Wall
LIBS = -lm -lpthread

TARGET = xiphos
SRCS = src/*.c

bmi2:
	$(CC) $(CFLAGS) $(SRCS) -mbmi2 -o $(TARGET)-bmi2 $(LIBS)

magic:
	$(CC) $(CFLAGS) $(SRCS) -msse -D_MAGIC -o $(TARGET)-magic $(LIBS)

clean:
	rm $(TARGET)
