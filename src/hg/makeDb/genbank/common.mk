#Optimized for performance or debugging
OPT= -ggdb
#OPT= -ggdb -O3
#OPT= -ggdb -O
#OPT= -ggdb -O3 -pg

KENT = ${GBROOT}/../../..

# FIXME: for now, need to link statically on RH7 or a warning is written
# to stdout on RH9, which breaks the program trying to read the output. 
# Gag me...
ifneq ($(wildcard ${GBROOT}/extern/lib/libmysqlclient.a),)
MYSQLLIBS=${GBROOT}/extern/lib/libmysqlclient.a
STATIC = -static
endif

ifeq (${MYSQLLIBS},)
$(error must set MYSQLLIBS env var)
endif

INCL = -I${GBROOT}/src/inc -I${KENT}/inc -I${KENT}/hg/inc
CFLAGS = ${OPT} ${STATIC} -DJK_WARN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -Wall -Werror ${INCL}

BINDIR = ${GBROOT}/bin
BINARCH = ${BINDIR}/${MACHTYPE}
LIBDIR = ${GBROOT}/lib
LIBARCH = ${LIBDIR}/${MACHTYPE}

LIBGENBANK = $(LIBARCH)/libgenbank.a

MYLIBDIR = ${KENT}/lib/$(MACHTYPE)
JKLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
LIBS = $(LIBGENBANK) ${JKLIBS}  ${MYSQLLIBS} -lm

TESTBIN = ${GBROOT}/tests/bin
TESTBINARCH = ${TESTBIN}/$(MACHTYPE)

MKDIR = mkdir -p

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

$(BINARCH)/%: ${O} makefile ${LIBGENBANK}
	@${MKDIR} -p ${BINARCH}
	gcc ${CFLAGS} -o $@ $O $(LIBS)

${BINDIR}/%: %
	@${MKDIR} -p ${BINDIR}
	cp -f $< $@
	chmod a-w,a+rx $@
