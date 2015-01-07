CC=gcc
# to build on sundance: CC=gcc -mcpu=v9 -m64
ifeq (${COPT},)
    COPT=-O -g
endif
ifeq (${CFLAGS},)
    CFLAGS=
endif
ifeq (${MACHTYPE},)
    MACHTYPE:=$(shell uname -m)
#    $(info MACHTYPE was empty, set to: ${MACHTYPE})
endif
ifneq (,$(findstring -,$(MACHTYPE)))
#    $(info MACHTYPE has - sign ${MACHTYPE})
    MACHTYPE:=$(shell uname -m)
#    $(info MACHTYPE has - sign set to: ${MACHTYPE})
endif

HG_DEFS=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -DMACHTYPE_${MACHTYPE}
HG_INC=-I../inc -I../../inc -I../../../inc -I../../../../inc -I../../../../../inc

# to check for Mac OSX Darwin specifics:
UNAME_S := $(shell uname -s)
# to check for builds on hgwdev
FULLWARN = $(shell uname -n)

#global external libraries 
L=

# pthreads is required
ifneq ($(UNAME_S),Darwin)
  L+=-pthread
endif

# autodetect if openssl is installed
ifeq (${SSLDIR},)
  SSLDIR = /usr/include/openssl
endif
ifeq (${USE_SSL},)
  ifneq ($(wildcard ${SSLDIR}),)
     USE_SSL=1
  endif
endif


# autodetect UCSC installation of hal:
ifeq (${HALDIR},)
    HALDIR = /hive/groups/browser/hal/halRelease
    ifneq ($(wildcard ${HALDIR}),)
        ifeq (${USE_HAL},)
          USE_HAL=1
        endif
    endif
endif

ifeq (${USE_HAL},1)
    HALLIBS=${HALDIR}/lib/halMaf.a ${HALDIR}/lib/halChain.a ${HALDIR}/lib/halMaf.a ${HALDIR}/lib/halLiftover.a ${HALDIR}/lib/halLod.a ${HALDIR}/lib/halLib.a ${HALDIR}/lib/sonLib.a ${HALDIR}/lib/libhdf5_cpp.a ${HALDIR}/lib/libhdf5.a ${HALDIR}/lib/libhdf5_hl.a 
    HG_DEFS+=-DUSE_HAL
    HG_INC+=-I${HALDIR}/inc
endif


# libssl: disabled by default
ifeq (${USE_SSL},1)
    ifneq (${SSL_DIR}, "/usr/include/openssl")
      ifneq ($(UNAME_S),Darwin)
        L+=-L${SSL_DIR}/lib
      endif
        HG_INC+=-I${SSL_DIR}/include
    endif
    # on hgwdev, already using the static library with mysqllient.
    ifeq (${FULLWARN},hgwdev)
       L+=/usr/lib64/libssl.a /usr/lib64/libcrypto.a -lkrb5
    else
       L+=-lssl -lcrypto
    endif
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
  ifneq ($(wildcard /opt/local/lib/libpng.a),)
      PNGLIB=/opt/local/lib/libpng.a
  endif
endif
ifeq (${PNGLIB},)
  ifneq ($(wildcard /usr/local/lib/libpng.a),)
      PNGLIB=/usr/local/lib/libpng.a
  endif
endif
ifeq (${PNGLIB},)
      PNGLIB := $(shell libpng-config --ldflags  || true)
endif
ifeq (${PNGLIB},)
  PNGLIB=-lpng
endif
ifeq (${PNGINCL},)
  ifneq ($(wildcard /opt/local/include/png.h),)
      PNGINCL=-I/opt/local/include
  else
      PNGINCL := $(shell libpng-config --I_opts  || true)
#       $(info using libpng-config to set PNGINCL: ${PNGINCL})
  endif
endif

# autodetect where mysql includes and libraries are installed
# do not need to do this during 'clean' target (this is very slow for 'clean')
ifneq ($(MAKECMDGOALS),clean)
  # on hgwdev, use the static library.
  ifeq (${FULLWARN},hgwdev)
    MYSQLINC=/usr/include/mysql
    MYSQLLIBS=/usr/lib64/libssl.a /usr/lib64/libcrypto.a /usr/lib64/mysql/libmysqlclient.a -lkrb5
  endif
  # this does *not* work on Mac OSX with the dynamic libraries
  ifneq ($(UNAME_S),Darwin)
    ifeq (${MYSQLLIBS},)
      MYSQLLIBS := $(shell mysql_config --libs || true)
#        $(info using mysql_config to set MYSQLLIBS: ${MYSQLLIBS})
    endif
  endif

  ifeq (${MYSQLINC},)
    MYSQLINC := $(shell mysql_config --include | sed -e 's/-I//' || true)
#        $(info using mysql_config to set MYSQLINC: ${MYSQLINC})
  endif
  ifeq (${MYSQLINC},)
    ifneq ($(wildcard /usr/local/mysql/include/mysql.h),)
	  MYSQLINC=/usr/local/mysql/include
    endif
  endif
  ifeq (${MYSQLINC},)
    ifneq ($(wildcard /usr/include/mysql/mysql.h),)
	  MYSQLINC=/usr/include/mysql
    endif
  endif
  ifeq (${MYSQLINC},)
    ifneq ($(wildcard /opt/local/include/mysql55/mysql/mysql.h),)
	  MYSQLINC=/opt/local/include/mysql55/mysql
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/lib64/mysql/libmysqlclient.a),)
	  MYSQLLIBS=/usr/lib64/mysql/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/local/mysql/lib/libmysqlclient.a),)
	  MYSQLLIBS=/usr/local/mysql/lib/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/local/mysql/lib/libmysqlclient.a),)
	  MYSQLLIBS=/usr/local/mysql/lib/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/lib64/mysql/libmysqlclient.so),)
	  MYSQLLIBS=/usr/lib64/mysql/libmysqlclient.so
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/lib/libmysqlclient.a),)
	  MYSQLLIBS=/usr/lib/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /opt/local/lib/mysql55/mysql/libmysqlclient.a),)
	  MYSQLLIBS=/opt/local/lib/mysql55/mysql/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/local/Cellar/mysql/5.6.19/lib/libmysqlclient.a),)
	  MYSQLLIBS=/usr/local/Cellar/mysql/5.6.19/lib/libmysqlclient.a
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/local/Cellar/mysql/5.6.16/lib/libmysqlclient.a),)
	  MYSQLLIBS=/usr/local/Cellar/mysql/5.6.16/lib/libmysqlclient.a
    endif
  endif
  ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
    ifeq (${MYSQLINC},)
        $(error can not find installed mysql development system)
    endif
  endif
    # last resort, hoping the compiler can find it in standard locations
  ifeq (${MYSQLLIBS},)
      MYSQLLIBS="-lmysqlclient"
  endif
endif

# $(info have MYSQLINC: ${MYSQLINC})
# $(info have MYSQLLIBS: ${MYSQLLIBS})

# OK to add -lstdc++ to all MYSQLLIBS just in case it is
#    MySQL version 5.6 libraries, but no 'librt' on Mac OSX
ifeq (${FULLWARN},hgwdev)
  MYSQLLIBS += /usr/lib/gcc/x86_64-redhat-linux/4.4.4/libstdc++.a /usr/lib/debug/usr/lib64/librt.a
else
  ifeq ($(UNAME_S),Darwin)
    MYSQLLIBS += -lstdc++
  else
    MYSQLLIBS += -lstdc++ -lrt
  endif
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

# pass through COREDUMP
ifneq (${COREDUMP},)
    HG_DEFS+=-DCOREDUMP
endif

# autodetect UCSC additional source code with password for some external tracks on gbib
GBIBDIR = /hive/groups/browser/gbib/
ifneq ($(wildcard ${GBIBDIR}/*.c),)
  HG_DEFS+=-DUSE_GBIB_PWD
  HG_INC += -I${GBIBDIR}
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

ifeq (${HG_WARN},)
  ifeq (${SYS},Darwin)
      HG_WARN = -Wall -Wno-unused-variable -Wno-deprecated-declarations
      HG_WARN_UNINIT=
  else
    ifeq (${SYS},SunOS)
      HG_WARN = -Wall -Wformat -Wimplicit -Wreturn-type
      HG_WARN_UNINIT=-Wuninitialized
    else
      ifeq (${FULLWARN},hgwdev)
        HG_WARN = -Wall -Werror -Wformat -Wformat-security -Wimplicit -Wreturn-type -Wempty-body
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

# avoid an extra leading slash when DESTDIR is empty
ifeq (${DESTDIR},)
  DESTBINDIR=${BINDIR}
else
  DESTBINDIR=${DESTDIR}/${BINDIR}
endif

# location of stringify program
STRINGIFY = ${DESTBINDIR}/stringify

MKDIR=mkdir -p
ifeq (${STRIP},)
   STRIP=true
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

# jshint: off unless JSHINT is already in environment
ifeq (${JSHINT},)
    JSHINT=true
endif
