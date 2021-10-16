ifeq ($(OS),Windows_NT)
	ifeq ($(MINGW_CHOST),i686-w64-mingw32)
		LIBARCH := _32
	endif
	ifeq ($(MINGW_CHOST),x86_64-w64-mingw32)
		LIBARCH := _64
	endif
	LIBARCH ?=
	CC := gcc.exe
	CFLAGS := -DWINDOWS -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-parameter -Ilibusb
	LDFLAGS := -Llibusb -lusb-1.0$(LIBARCH) -lWs2_32 -lmsvcrt
	OBJS := src/main.o src/getopt.o src/stlink.o src/crypto.o tiny-AES-c/aes.o
else
	CFLAGS := -Wall -Wextra -Werror -Wno-unused-parameter -Wno-error=unused-parameter $(shell pkg-config --cflags libusb-1.0) -g -Og
	LDFLAGS := $(shell pkg-config --libs libusb-1.0)
	OBJS := src/main.o src/stlink.o src/crypto.o tiny-AES-c/aes.o
endif

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

stlink-tool: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f src/*.o
	rm -f tiny-AES-c/*.o
	rm -f stlink-tool
