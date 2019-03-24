#CFLAGS=-O3 -Wall
CFLAGS=-g -ggdb -Wall
#LDLIBS=-l

all: pacextractor

clean:
	rm -f pacextractor

.PHONY:	clean all