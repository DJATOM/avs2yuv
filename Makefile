ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

OBJECTS := avs2yuv.o
BIN := avs2yuv

CCFLAGS := -I. -std=gnu99 -Wall -O3 -ffast-math -fno-math-errno -fomit-frame-pointer
LDFLAGS := -Wl,--no-as-needed

UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),i686)
	CCFLAGS += -msse2 -mfpmath=sse
endif
ifeq ($(UNAME_M),x86_64)
	CCFLAGS += -msse2 -mfpmath=sse
endif

ifeq (${OS},Windows_NT)
	CCFLAGS += -I"${AVISYNTH_SDK_PATH}\include"
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Haiku)
	LDFLAGS += -lroot
else
	LDFLAGS += -ldl
endif
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
