CC=cc
CFLAGS=-O

UNZ_OBJS = miniunz.o unzip.o ioapi.o
ZIP_OBJS = minizip.o zip.o   ioapi.o
PYZIP_OBJS = pyminizip.o zip.o ioapi.o
PYUNZIP_OBJS = pyminiunzip.o unzip.o ioapi.o

.c.o:
	$(CC) $(CFLAGS) -c $*.c

all: pyminizip

miniunz:  $(UNZ_OBJS)
	$(CC) $(CFLAGS) -lz -o $@ $(UNZ_OBJS)

minizip:  $(ZIP_OBJS)
	$(CC) $(CFLAGS) -lz -o $@ $(ZIP_OBJS)

pyminizip: $(PYZIP_OBJS) $(PYUNZIP_OBJS)
	$(CC) $(CFLAGS) -lz -o $@ $(PYZIP_OBJS) $(PYUNZIP_OBJS)

test:	miniunz minizip
	./minizip Makefile.zip Makefile
	./miniunz -l Makefile.zip
	mv Makefile Makefile.old
	./miniunz Makefile.zip

clean:
	/bin/rm -f *.o *~ minizip miniunz pyminizip
