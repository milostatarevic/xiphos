
CC = gcc
CFLAGS = -O3 -flto -Wall
LIBS = -lm -lpthread

TARGET = xiphos
SRCS = src/*.c

sse:
	$(CC) $(CFLAGS) -msse $(SRCS) -o $(TARGET)-sse $(LIBS)

bmi2:
	$(CC) $(CFLAGS) -D_BMI2 -mbmi2 $(SRCS) -o $(TARGET)-bmi2 $(LIBS)

clean:
	rm $(TARGET)
