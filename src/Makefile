CFLAGS?=

CFLAGS+=-O2 -flto -ffunction-sections -fdata-sections -Wl,-gc-sections
WFLAGS:=-Wall -Werror

#CFLAGS+=-O0 -g -ggdb
#WFLAGS:=


SFLAGS:=-std=gnu11
LDFLAGS?=
LDFLAGS+=
WISHFUL_LINKLIBS:=-lpthread -ldl -lzmq -ljson-c
WISHFUL_BINARY:=wishagent
LIBS:=

all: $(WISHFUL_BINARY)

$(WISHFUL_BINARY):	*.c \
					Tools/Timer/*.c

	$(CC) -I. $(CFLAGS) $(SFLAGS) $(WFLAGS) $(LDFLAGS) -o $@ $+ $(WISHFUL_LINKLIBS)
	cp $@ $@.symbols
	$(STRIP) $@


clean:
	rm -f $(WISHFUL_BINARY)
