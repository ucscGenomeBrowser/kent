#Optimized for performance or debugging
#OPT= -ggdb -O
OPT= -ggdb

KENT = ${GBROOT}/../kent/src
ifneq ($(wildcard ${GBROOT}/extern/lib/libmysqlclient.a),)
    export MYSQLINC=${GBROOT}/extern/include/mysql
    ABSGBROOT=$(shell cd ${GBROOT} && pwd)
    export MYSQLLIBS=${ABSGBROOT}/extern/lib/libmysqlclient.a
else
    export MYSQLINC=/usr/include/mysql
    export MYSQLLIBS=-lmysqlclient
endif

INCL = -I${GBROOT}/src/inc -I${KENT}/inc -I${KENT}/hg/inc -I$(MYSQLINC)
CFLAGS = ${OPT} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wall -Werror ${INCL}

BINDIR = ${GBROOT}/bin
BINARCH = ${BINDIR}/${MACHTYPE}
LIBDIR = ${GBROOT}/lib
LIBARCH = ${LIBDIR}/${MACHTYPE}

LIBGENBANK = $(LIBARCH)/libgenbank.a

# FIXME: not implemented yet:
# MYSQLLIBS are not included by default, as most programs don't need them
# and on linux if one uses shared libraries, they endup being required
# even if no symbol is refereneced.
MYLIBDIR = ${KENT}/lib/$(MACHTYPE)
LIBS = $(LIBGENBANK) $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a ${MYSQLLIBS} -lm

TESTBIN = ${GBROOT}/tests/bin
TESTBINARCH = ${TESTBIN}/$(MACHTYPE)


%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

$(BINARCH)/%: ${O} makefile ${LIBGENBANK}
	@mkdir -p ${BINARCH}
	gcc -O -o $@ $O $(LIBS)

${BINDIR}/%: %
	mkdir -p ${BINDIR}
	cp -f $< $@
	chmod a-w,a+rx $@
