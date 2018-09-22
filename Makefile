# This currently builds a user space program and not a useful library

USBCFLAGS = `pkg-config --cflags libusb-1.0`
USBLDFLAGS = `pkg-config --libs libusb-1.0`
CFLAGS=	-Wall -g -std=c99 -pedantic $(USBCFLAGS)
LDFLAGS= -L. -lmsr $(USBLDFLAGS)

LIB=	libmsr.a
LIBSRCS=	libmsr.c usbio.c msr206.c makstripe.c
LIBOBJS=	$(LIBSRCS:.c=.o)

DAB=	dab
DABSRCS=	dab.c
DABOBJS=	$(DABSRCS:.c=.o)

DMSB=	dmsb
DMSBSRCS=	dmsb.c
DMSBOBJS=	$(DMSBSRCS:.c=.o)

SUBDIRS=utils

all:	$(LIB)
	for subdir in $(SUBDIRS); do \
	  (cd $$subdir && $(MAKE) all); \
	done

$(LIB): $(LIBOBJS)
	ar rcs $(LIB) $(LIBOBJS)

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

install: 
	install -m644 -D $(LIB) $(DESTDIR)/usr/$(LIB)
	for subdir in $(SUBDIRS); do \
	  (cd $$subdir && $(MAKE) install); \
	done

AUDIOLDFLAGS=-lsndfile

$(DAB): $(DABOBJS)
	$(CC) -o $(DAB) $(DABOBJS) $(AUDIOLDFLAGS)
$(DMSB): $(DMSBOBJS)
	$(CC) -o $(DMSB) $(DMSBOBJS) $(AUDIOLDFLAGS)

audio: $(DAB) $(DMSB)

clean:
	rm -rf *.o *~ $(LIB) $(DAB) $(DMSB)
	for subdir in $(SUBDIRS); do \
	  (cd $$subdir && $(MAKE) clean); \
	done
