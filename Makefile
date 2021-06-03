.PHONY: clean

OUT=bin/fpv-video-out
IN=main.c
CFLAGS = -std=gnu11 -Wall -Wextra -Werror #-I/usr/include/libusb-1.0
LIBS = -lusb-1.0 -lpthread #-L/usr/lib/x86_64-linux-gnu/

$(OUT): $(IN)
	$(CC) $(CFLAGS) $(IN) $(LIBS) -o $(OUT) 
