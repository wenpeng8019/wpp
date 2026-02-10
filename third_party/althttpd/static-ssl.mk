# 2022-02-16:
# This makefile is used by the author (drh) to build versions of
# althttpd that are statically linked against OpenSSL.  The resulting
# binaries power sqlite.org, fossil-scm.org, and other machines.
#
# This is not a general-purpose makefile.  But perhaps you can adapt
# it to your specific needs.
#
# Build suggestion:
#
#    make -f static-ssl.mk OPENSSLDIR=<path-to-your-OpenSSL-build>
#
# To build an appropriate OpenSSL library for the above:
#
#    mkdir -p $HOME/openssl
#    cd $HOME/openssl
#    # Download a tarball of the latest OpenSSL version
#    # Unpack the tarball
#    # Rename the unpacked source tree from "openssl-X.Y.Z" to just "openssl"
#    cd openssl
#    ./config no-ssl3 no-weak-ssl-ciphers no-shared no-threads --openssldir=/etc/ssl
#    make CFLAGS=-Os
#
# If the OpenSSL build is in directory $HOME/openssl/openssl, then it is not
# necessary to include the OPENSSLDIR= option with "make" since that path is
# the default.
#

default: althttpd
OPENSSLDIR = $(HOME)/openssl/openssl
OPENSSLLIB = -L$(OPENSSLDIR) -lssl -lcrypto -ldl
CC=gcc
CFLAGS = -I$(OPENSSLDIR)/include -I. -DENABLE_TLS
CFLAGS += -Os -Wall -Wextra
LIBS = $(OPENSSLLIB)

VERSION.h:	VERSION manifest manifest.uuid mkversion.c
	$(CC) -o mkversion mkversion.c
	./mkversion manifest.uuid manifest VERSION >VERSION.h

althttpd:	althttpd.c VERSION.h
	$(CC) $(CFLAGS) -o althttpd althttpd.c $(LIBS)

sqlite3.o:	sqlite3.c
	$(CC) $(CFLAGS) -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -c -o sqlite3.o sqlite3.c

logtodb:	logtodb.c sqlite3.o VERSION.h
	$(CC) $(CFLAGS) -static -o logtodb logtodb.c sqlite3.o

clean:
	rm -f althttpd VERSION.h sqlite3.o logtodb
