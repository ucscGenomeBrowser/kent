#Optimized for performance or debugging
#OPT= -ggdb -O3
OPT= -ggdb

KENT = ${GBROOT}/../../..
ifeq (${MYSQLINC},)
$(error must set MYSQLINC env var)
endif
ifeq (${MYSQLLIBS},)
$(error must set MYSQLLIBS env var)
endif

INCL = -I${GBROOT}/src/inc -I${KENT}/inc -I${KENT}/hg/inc -I$(MYSQLINC)
CFLAGS = ${OPT} -DJK_WARN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wall -Werror ${INCL}

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
	gcc ${CFLAGS} -o $@ $O $(LIBS)

${BINDIR}/%: %
	mkdir -p ${BINDIR}
	cp -f $< $@
	chmod a-w,a+rx $@
