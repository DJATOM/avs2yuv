ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

OBJECTS := avs2yuv.o
BIN := avs2yuv

CC := gcc

CCFLAGS := -I. -std=gnu99 -Wall -O3 -msse2 -mfpmath=sse -ffast-math -fno-math-errno -flto -fomit-frame-pointer
LDFLAGS :=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -ldl
	CCFLAGS += -I/usr/local/include/avisynth
else ifeq (${OS},Windows_NT)
	CCFLAGS += -I"${AVISYNTH_SDK_PATH}\include"
endif
all: $(BIN)

%.o : %.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(BIN) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -s -o $(BIN)

install: avs2yuv
	install -m 755 avs2yuv $(PREFIX)/bin/

clean :
	rm -f $(OBJECTS) $(BIN)

avs2yuv.o: avs2yuv.c
