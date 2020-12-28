ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

OBJECTS := avs2yuv.o
BIN := avs2yuv

CC := $(CC)

CCFLAGS := -I. -std=gnu99 -Wall -O3 -msse2 -mfpmath=sse -ffast-math -fno-math-errno -flto -fomit-frame-pointer
LDFLAGS := -Wl,--no-as-needed

ifeq (${OS},Windows_NT)
	CCFLAGS += -I"${AVISYNTH_SDK_PATH}\include"
else
	LDFLAGS += -ldl
	CCFLAGS += $(shell pkg-config --cflags avisynth)
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
