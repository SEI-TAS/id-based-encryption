#Makefile for IBE System
#Ben Lynn
#
#Copyright (C) 2001 Benjamin Lynn (blynn@cs.stanford.edu)
#
#This file is part of the Stanford IBE system.
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

ALLFILES=*.c *.h Makefile README NEWS *.cnf *.html LICENSE firstmessage.txt instructions.txt ibe_help.txt testscript testscript2

SOFTLINKFILES=benchmark.[ch] netstuff.[ch] mm.[ch] get_time.c

ifdef WIN32
#for cross-compiling the Debian GNU/Linux mingw package
CC=i586-mingw32msvc-gcc
CXX=i586-mingw32msvc-g++
SSL_I=/home/ben/cross/ssl/include
SSL_L=/home/ben/cross/ssl/lib
GMP_I=/home/ben/cross/gmp/include
GMP_L=/home/ben/cross/gmp/lib

WIN_LIBS=-lwsock32 -lgdi32
CFLAGS= -DNDEBUG -pipe -O2 -march=i386 -Wall -I$(GMP_I) -I$(SSL_I)
BINARIES=gen.exe ibe.exe #pkghtml.exe
BINDISTFILES=$(BINARIES) gen.cnf ibe.cnf ibe_help.txt
OSNAME=win32
else
#for Linux

#gcc-3.0 seems faster
CC=gcc
SSL_I=/usr/include/openssl
SSL_L=/usr/lib/x86_64-linux-gnu
GMP_I=/usr/include
GMP_L=/usr/lib/x86_64-linux-gnu

CFLAGS= -DNDEBUG -pipe -O3 -march=x86-64 -Wall -I$(GMP_I) -I$(SSL_I) \
-fomit-frame-pointer -ffast-math -funroll-loops
BINARIES=gen pkghtml ibe infect
TESTBINS=bs_test fp2_test curve_test ibe_test bls_test sig_test torture
OSNAME=linux
endif

ifdef MM
MMSUFFIX=true
OPT_LIBS=mm.o
else
MMSUFFIX=void
endif

ifdef BENCHMARK
BMSUFFIX=true
OPT_LIBS=benchmark.o get_time.o
else
BMSUFFIX=void
endif

CRYPTO_LIBS=-L$(SSL_L) -lcrypto
SSL_LIBS=-L$(SSL_L) -lssl -lcrypto
GMP_LIBS=-L$(GMP_L) -lgmp

IBE_LIBS=ibe_lib.o curve.o fp2.o crypto.o byte_string.o $(OPT_LIBS)
FMT_LIBS=$(IBE_LIBS) format.o
IBE_PROGS=encrypt.o decrypt.o request.o netstuff.o combine.o \
    imratio.o get_time.o debug_ibe.o certify.o sign.o verify.o

.PHONY: target test dist clean

target: mm.h benchmark.h $(BINARIES)

test: mm.h benchmark.h $(TESTBINS)

netstuff.c : netstuff.$(OSNAME).c
	-ln -s $^ $@

netstuff.h : netstuff.$(OSNAME).h
	-ln -s $^ $@

byte_string.o : byte_string.c byte_string.h

config.o : config.c config.h

crypto.o : crypto.c crypto.h

ifdef MM
mm.c : mm.$(MMSUFFIX).c
	-ln -s $^ $@
mm.o : mm.c mm.h
endif

mm.h : mm.$(MMSUFFIX).h
	-ln -s $^ $@

ifdef BENCHMARK
benchmark.c : benchmark.$(BMSUFFIX).c
	-ln -s $^ $@

benchmark.o : benchmark.c benchmark.h

get_time.o : get_time.c get_time.h
endif

benchmark.h : benchmark.$(BMSUFFIX).h
	-ln -s $^ $@

get_time.c : get_time.$(OSNAME).c
	-ln -s $^ $@

ibe_lib.o: ibe_lib.c ibe.h version.h benchmark.h

ibe.o: ibe.c ibe.h
decrypt.o: decrypt.c ibe.h
encrypt.o: encrypt.c ibe.h
request.o: request.c ibe.h netstuff.h
netstuff.o : netstuff.c netstuff.h
combine.o : combine.c ibe.h
imratio.o : imratio.c ibe.h
debug_ibe.o: debug_ibe.c ibe.h
certify.o: certify.c ibe.h
sign.o: sign.c ibe.h
verify.o: verify.c ibe.h

format.o: format.c format.h

gen.o : gen.c ibe.h

pkghtml.o : pkghtml.c ibe.h netstuff.h

bls_test.o : bls_test.c

sig_test.o : sig_test.c

bs_test.o : bs_test.c

ibe_test.o : ibe_test.c ibe.h

torture.o : torture.c

curve_test.o : curve_test.c

fp2_test.o : fp2_test.c

curve.o: curve.c curve.h

fp2.o: fp2.c fp2.h

gen: gen.o $(FMT_LIBS) config.o
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(CRYPTO_LIBS)

bs_test: bs_test.o byte_string.o $(OPT_LIBS)
	$(CC) $(CFLAGS) -o $@ $^

bls_test: bls_test.o $(IBE_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(CRYPTO_LIBS)

sig_test: sig_test.o $(IBE_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(CRYPTO_LIBS)

torture: torture.o $(IBE_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(CRYPTO_LIBS) -lpthread

ibe_test: ibe_test.o $(IBE_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(CRYPTO_LIBS)

infect: infect.o $(FMT_LIBS) config.o
	$(CC) $(CFLAGS) -o $@ $^ $(SSL_LIBS) $(GMP_LIBS)

pkghtml: pkghtml.o $(FMT_LIBS) config.o netstuff.o
	    $(CC) $(CFLAGS) -o $@ $^ -lpthread $(SSL_LIBS) $(GMP_LIBS)

ibe: ibe.o $(FMT_LIBS) $(IBE_PROGS) config.o
	$(CC) $(CFLAGS) -o $@ $^ $(SSL_LIBS) $(GMP_LIBS)

fp2_test: fp2_test.o fp2.o $(OPT_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS)

curve_test: curve_test.o curve.o fp2.o $(OPT_LIBS)
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS)

gen.exe: gen.o $(FMT_LIBS) config.o
	$(CC) $(CFLAGS) -o $@ $^ $(GMP_LIBS) $(SSL_LIBS) $(WIN_LIBS)

ibe.exe: ibe.o $(FMT_LIBS) $(IBE_PROGS) config.o
	$(CC) $(CFLAGS) -o $@ $^ $(SSL_LIBS) $(GMP_LIBS) $(WIN_LIBS)

projname := $(shell awk '/IBE_VERSION/ { print $$3 }' version.h )

dist: $(ALLFILES)
	-rm -f $(SOFTLINKFILES)
	-rm -rf $(projname)
	mkdir $(projname)
	cp -rl --parents $(ALLFILES) $(projname)
	tar chfz $(projname).tgz $(projname)
	-rm -rf $(projname)

clean:
	-rm -rf *.o $(BINARIES) $(TESTBINS) $(SOFTLINKFILES)

ifdef WIN32
bindist: $(BINDISTFILES)
	zip $(projname)-win.zip $(BINDISTFILES)
endif
