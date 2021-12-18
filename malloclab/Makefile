#
# Students' Makefile for the Malloc Lab
#
STUID = 2018202143
# ! Change the STUID to your own student ID
VERSION = 1
# ! Update the VERSION everytime you handin
HANDINDIR = /home/handin-malloc

CC = gcc
#CFLAGS = -Wall -g3 -m32 -std=gnu99 -Dinline= -DDEBUG
CFLAGS = -Wall -Ofast -flto -std=gnu99

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

handin:
	chmod 600 mm.c
	add mm.c $(HANDINDIR)/$(STUID)-$(VERSION)-mm.c
clean:
	rm -f *~ *.o mdriver


