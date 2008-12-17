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

ifeq (${HG_WARN},)
  ifeq (darwin,$(findstring darwin,${OSTYPE}))
      HG_WARN = -Wall -Wno-unused-variable -Wno-long-double
      HG_WARN_UNINIT=
  else
    ifeq (solaris,$(findstring solaris,${OSTYPE}))
      HG_WARN = -Wall -Wformat -Wimplicit -Wreturn-type
      HG_WARN_UNINIT=-Wuninitialized
    else
      HG_WARN = -Wall -Werror -Wformat -Wimplicit -Wreturn-type
      HG_WARN_UNINIT=-Wuninitialized
    endif
  endif
  # -Wuninitialized generates a warning without optimization
  ifeq ($(findstring -O,${COPT}),-O)
     HG_WARN += ${HG_WARN_UNINIT}
  endif
endif

INCL = -I${GBROOT}/src/inc -I${KENT}/inc -I${KENT}/hg/inc
CFLAGS = ${COPT} ${STATIC} -DJK_WARN -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE ${HG_WARN} ${INCL}

GB_BINDIR = ${GBROOT}/bin
GB_BINARCH = ${GB_BINDIR}/${MACHTYPE}
GB_LIBDIR = ${GBROOT}/lib
GB_LIBARCH = ${GB_LIBDIR}/${MACHTYPE}

LIBGENBANK = $(GB_LIBARCH)/libgenbank.a

MYLIBDIR = ${KENT}/lib/$(MACHTYPE)
JKLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
LIBS = $(LIBGENBANK) ${JKLIBS}  ${MYSQLLIBS} -lm

TESTBIN = ${GBROOT}/tests/bin
TESTBINARCH = ${TESTBIN}/$(MACHTYPE)

MKDIR = mkdir -p
STRINGIFY = stringify

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $<

$(GB_BINARCH)/%: ${O} makefile ${LIBGENBANK}
	@${MKDIR} -p ${GB_BINARCH}
	gcc ${CFLAGS} -o $@ $O $(LIBS)

${GB_BINDIR}/%: %
	@${MKDIR} -p ${GB_BINDIR}
	cp -f $< $@
	chmod a-w,a+rx $@
