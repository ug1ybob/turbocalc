CC = gcc
CFLAGS = -std=gnu2x -Wall -Wextra -O3 -lpthread
CFLAGS_PROD = -static
TARGET = turbocalc
SRC = turbocalc.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CFLAGS_PROD) -o $(TARGET) $(SRC)
	strip $(TARGET)
	upx --no-progress --preserve-build-id --best --lzma --force-overwrite -o $(TARGET) $(TARGET)

debug: $(SRC)
	$(CC) -g $(CFLAGS) -o $(TARGET).debug $(SRC)

clean:
	rm -f $(TARGET)
