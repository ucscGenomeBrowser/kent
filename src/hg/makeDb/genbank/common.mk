#Optimized for performance or debugging
#OPT= -ggdb -O
OPT= -ggdb

KENT = ${GBROOT}/../kent/src
export MYSQLINC=/usr/include/mysql
export MYSQLLIBS=-lmysqlclient
ifeq ($(wildcard ${MYSQLINC}/*.h),)
    export MYSQLINC=/projects/hg2/usr/markd/genefind/mysql/include/mysql
    export MYSQLLIBS=/projects/hg2/usr/markd/genefind/mysql/lib/mysql/libmysqlclient.a
endif

INCL = -I${GBROOT}/src/inc -I${KENT}/inc -I${KENT}/hg/inc -I$(MYSQLINC)
CFLAGS = ${OPT} -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wall -Werror ${INCL}

BINDIR = ${GBROOT}/bin
BINARCH = ${BINDIR}/${MACHTYPE}
LIBDIR = ${GBROOT}/lib
LIBARCH = ${LIBDIR}/${MACHTYPE}

LIBGENBANK = $(LIBARCH)/libgenbank.a

MYLIBDIR = ${KENT}/lib/$(MACHTYPE)
LIBS = $(LIBGENBANK)  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a $(MYSQLLIBS) -lm

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
