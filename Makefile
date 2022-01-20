# User options

CC = gcc
CFLAGS = -Wall -g

INSTALL_BIN_DIR=$(HOME)/bin
INSTALL_LIB_DIR=$(HOME)/lib
INSTALL_INC_DIR=$(HOME)/include

# End of user options

LIBCKPTOBJS =\
	ckpt.o\
	init.o\
	mem.o\
	signals.o\
	uri.o\
	remote.o\
	sockaddr.o\
	util.o\
	config.o\
	fd.o\
	elfckpt.o\
	wraprestart.o
LIBRESTARTOBJS = safe.o
RESTARTOBJS =\
	restart.o\
	mem.o\
	util.o\
	uri.o\
	remote.o\
	sockaddr.o\
	wrapsafe.o\
	elfrestart.o
CKPTOBJS =\
	ckptmain.o\
	wrapckpt.o\
	hijack.o\
	refun.o\
	util.o
CKPTSRVOBJS =\
	ckptsrv.o\
	util.o\
	sockaddr.o
BTOCOBJS = btoc.o
TESTOBJS = foo.o
HELPERS = librestart.so restart btoc

all: $(HELPERS) libckpt.so ckpt ckptsrv foo
-include depend

.c.o:
	$(CC) $(CFLAGS) $(OPTS) -c $<

libckpt.so: $(LIBCKPTOBJS)
	$(CC) -shared -nostartfiles -o libckpt.so $(LIBCKPTOBJS) -ldl

librestart.so: $(LIBRESTARTOBJS) 
	$(CC) -shared -nostartfiles -Xlinker -Bsymbolic \
           -o librestart.so $(LIBRESTARTOBJS) /usr/lib/libc.a

# FIXME: -ldl is only linking uncalled functions in util.o
ckpt: $(CKPTOBJS)
	$(CC) -o ckpt $(CKPTOBJS) -ldl

btoc: $(BTOCOBJS)
	$(CC) -o btoc $(BTOCOBJS)

restart: $(RESTARTOBJS)
	$(CC) -Xlinker --script=restart.script -o restart $(RESTARTOBJS) -ldl

wrapsafe.c: btoc librestart.so
	./btoc librestart < librestart.so > wrapsafe.c

wraprestart.c: btoc restart
	./btoc restartbin < restart > wraprestart.c

wrapckpt.c: btoc libckpt.so
	./btoc libckpt < libckpt.so > wrapckpt.c

ckptsrv: $(CKPTSRVOBJS)
	$(CC) -o ckptsrv $(CKPTSRVOBJS) -ldl

foo: $(TESTOBJS) sockaddr.o libckpt.so 
	$(CC) -g -o foo $(TESTOBJS) sockaddr.o
#	$(CC) -g -o foo $(TESTOBJS) sockaddr.o -L. -lckpt

install: all
	install ckpt $(INSTALL_BIN_DIR)/ckpt
	install ckptsrv $(INSTALL_BIN_DIR)/ckptsrv
	install libckpt.so $(INSTALL_LIB_DIR)/libckpt.so
	install ckpt.h $(INSTALL_INC_DIR)/ckpt.h

clean:
	rm -f core *~ *.o *.rsync depend wrap*.c $(HELPERS)

depend:
	gcc -MM *.c > depend
