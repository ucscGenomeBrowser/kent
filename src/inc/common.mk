CC=gcc
# to build on sundance: CC=gcc -mcpu=v9 -m64
ifeq (${COPT},)
    COPT=-O -g
endif
ifeq (${CFLAGS},)
    CFLAGS=
endif
HG_DEFS=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -DMACHTYPE_${MACHTYPE}
HG_INC=-I../inc -I../../inc -I../../../inc -I../../../../inc -I../../../../../inc

#global external libraries 
L=

# pthreads is required
L+=-pthread

# autodetect if openssl is installed
ifeq (${SSLDIR},)
  SSLDIR = /usr/include/openssl
endif
ifeq (${USE_SSL},)
  ifneq ($(wildcard ${SSLDIR}),)
     USE_SSL=1
  endif
endif


# libssl: disabled by default
ifeq (${USE_SSL},1)
    L+=-lssl -lcrypto
    HG_DEFS+=-DUSE_SSL
endif

# autodetect where png is installed
ifeq (${PNGLIB},)
  ifneq ($(wildcard /usr/lib64/libpng.a),)
      PNGLIB=/usr/lib64/libpng.a
  endif
endif
ifeq (${PNGLIB},)
  ifneq ($(wildcard /usr/lib/libpng.a),)
      PNGLIB=/usr/lib/libpng.a
  endif
endif
ifeq (${PNGLIB},)
  PNGLIB=-lpng
endif

L+=${PNGLIB}
HG_INC+=${PNGINCL}

# 32-bit color enabled by default
ifneq (${COLOR32},0)
    HG_DEFS+=-DCOLOR32
endif

# autodetect UCSC installation of samtabix:
ifeq (${SAMTABIXDIR},)
    SAMTABIXDIR = /hive/data/outside/samtabix/${MACHTYPE}
    ifneq ($(wildcard ${SAMTABIXDIR}),)
        ifeq (${USE_SAMTABIX},)
          USE_SAMTABIX=1
        endif
    endif
endif

# libsamtabix (samtools + tabix + Angie's KNETFILE_HOOKS extension to it): disabled by default
ifeq (${USE_SAMTABIX},1)
    KNETFILE_HOOKS=1
    USE_BAM=1
    USE_TABIX=1
    ifeq (${SAMTABIXINC},)
        SAMTABIXINC = ${SAMTABIXDIR}
    endif
    ifeq (${SAMTABIXLIB},)
        SAMTABIXLIB = ${SAMTABIXDIR}/libsamtabix.a
    endif
    HG_INC += -I${SAMTABIXINC}
    L+=${SAMTABIXLIB} -lz
    HG_DEFS+=-DUSE_SAMTABIX -DUSE_BAM -DUSE_TABIX -DKNETFILE_HOOKS
else
  # Deprecated but supported for mirrors, for now: independent samtools and tabix libs

  # libbam (samtools, and Angie's KNETFILE_HOOKS extension to it): disabled by default
  ifeq (${USE_BAM},1)
      ifeq (${SAMINC},)
          SAMINC = ${SAMDIR}
      endif
      ifeq (${SAMLIB},)
          SAMLIB = ${SAMDIR}/libbam.a
      endif
      HG_INC += -I${SAMINC}
      L+=${SAMLIB}
      HG_DEFS+=-DUSE_BAM
      ifeq (${KNETFILE_HOOKS},1)
          HG_DEFS+=-DKNETFILE_HOOKS
      endif
  endif

  # libtabix and Angie's KNETFILE_HOOKS extension to it: disabled by default
  ifeq (${USE_TABIX},1)
      ifeq (${TABIXINC},)
          TABIXINC = ${TABIXDIR}
      endif
      ifeq (${TABIXLIB},)
          TABIXLIB = ${TABIXDIR}/libtabix.a
      endif
      HG_INC += -I${TABIXINC}
      L+=${TABIXLIB} -lz
      HG_DEFS+=-DUSE_TABIX
      ifeq (${KNETFILE_HOOKS},1)
	HG_DEFS+=-DKNETFILE_HOOKS
      endif
  endif
endif

SYS = $(shell uname -s)
FULLWARN = $(shell uname -n)

ifeq (${HG_WARN},)
  ifeq (${SYS},Darwin)
      HG_WARN = -Wall -Wno-unused-variable
      HG_WARN_UNINIT=
  else
    ifeq (${SYS},SunOS)
      HG_WARN = -Wall -Wformat -Wimplicit -Wreturn-type
      HG_WARN_UNINIT=-Wuninitialized
    else
      ifeq (${FULLWARN},hgwdev)
        HG_WARN = -Wall -Werror -Wformat -Wimplicit -Wreturn-type
        HG_WARN_UNINIT=-Wuninitialized
      else
        HG_WARN = -Wall -Wformat -Wimplicit -Wreturn-type
        HG_WARN_UNINIT=-Wuninitialized
      endif
    endif
  endif
  # -Wuninitialized generates a warning without optimization
  ifeq ($(findstring -O,${COPT}),-O)
     HG_WARN += ${HG_WARN_UNINIT}
  endif
endif

# this is to hack around many make files not including HG_WARN in
# the link line
CFLAGS += ${HG_WARN}

ifeq (${SCRIPTS},)
    SCRIPTS=${HOME}/bin/scripts
endif
ifeq (${CGI_BIN},)
    CGI_BIN=/usr/local/apache/cgi-bin
endif
ifeq (${DOCUMENTROOT},)
    DOCUMENTROOT=/usr/local/apache/htdocs
endif
ifeq (${BINDIR},)
    BINDIR = ${HOME}/bin/${MACHTYPE}
endif
ifeq (${ENCODE_PIPELINE_BIN},)
    ENCODE_PIPELINE_BIN=/cluster/data/encode/pipeline/bin
endif

MKDIR=mkdir -p
ifeq (${STRIP},)
   STRIP=strip
endif
CVS=cvs
GIT=git

# portable naming of compiled executables: add ".exe" if compiled on 
# Windows (with cygwin).
ifeq (${OS}, Windows_NT)
  AOUT=a
  EXE=.exe
else
  AOUT=a.out
  EXE=
endif

# location of stringify program
STRINGIFY = ${DESTDIR}${BINDIR}/stringify

#Lowelab defines
#The lowelab specific code will be included in compilation if the following conditions are satistied
LOWELAB_WIKI_DEF=
LOWELAB_DEF=
ifdef LOWELAB
    LOWELAB_WIKI=1
    LOWELAB_DEF=-DLOWELAB
endif
ifdef LOWELAB_WIKI
    LOWELAB_WIKI_DEF=-DLOWELAB_WIKI
endif
LOWELAB_DEFS=${LOWELAB_DEF} ${LOWELAB_WIKI_DEF}

ifdef LOWELAB
    ifeq (${SCRIPTS},/cluster/bin/scripts)
        SCRIPTS=${HOME}/scripts
    endif
    ifeq (${CGI_BIN},/usr/local/apache/cgi-bin)
        CGI_BIN=/www/cgi-bin
    endif
    ifeq (${DOCUMENTROOT},/usr/local/apache/htdocs)
        DOCUMENTROOT=/www/browser-docs
    endif
endif

#ENCODE COMMON VARIABLES
CONFIG_FILES = \
	fields.ra \
	labs.ra
CV = cv.ra
CVDIR=${HOME}/kent/src/hg/makeDb/trackDb/cv/alpha
PIPELINE_PATH=/hive/groups/encode/dcc/pipeline
CONFIG_DIR = ${PIPELINE_PATH}/${PIPELINE_DIR}/config
ENCODEDCC_DIR = ${PIPELINE_PATH}/downloads/encodeDCC

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${LOWELAB_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

