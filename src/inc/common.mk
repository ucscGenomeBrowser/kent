# if CC is undefined, set it to gcc
CC?=gcc
# allow the somewhat more modern C syntax, e.g. 'for (int i=5; i<10, i++)'
CFLAGS += -std=c99

# add additional library paths
L += ${LDFLAGS}

# to build on sundance: CC=gcc -mcpu=v9 -m64
ifeq (${COPT},)
    COPT=-O -g
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
HG_INC+=-I../inc -I../../inc -I../../../inc -I../../../../inc -I../../../../../inc -I$(kentSrc)/htslib

# to check for Mac OSX Darwin specifics:
UNAME_S := $(shell uname -s)
# to check for builds on hgwdev
HOSTNAME = $(shell uname -n)

ifeq (${HOSTNAME},hgwdev)
  IS_HGWDEV = yes
else
  IS_HGWDEV = no
endif

# for Darwin (Mac OSX), use static libs when they can be found
ifeq ($(UNAME_S),Darwin)
  ifneq ($(wildcard /opt/local/include/openssl/ssl.h),)
    HG_INC += -I/opt/local/include
  endif
  ifneq ($(wildcard /opt/local/lib/libz.a),)
    ZLIB = /opt/local/lib/libz.a
  endif
  ifneq ($(wildcard /opt/local/lib/libpng.a),)
    PNGLIB = /opt/local/lib/libpng.a
  endif
  ifneq ($(wildcard /opt/local/lib/libfreetype.a),)
    FREETYPELIBS = /opt/local/lib/libfreetype.a /opt/local/lib/libbz2.a /opt/local/lib/libbrotlidec.a /opt/local/lib/libbrotlicommon.a
  endif
  ifneq ($(wildcard /usr/local/opt/openssl@3/lib/libssl.a),)
    SSLLIB = /usr/local/opt/openssl@3/lib/libssl.a
  else
    ifneq ($(wildcard /opt/local/libexec/openssl3/lib/libssl.a),)
      SSLLIB = /opt/local/libexec/openssl3/lib/libssl.a
    endif
  endif
  ifneq ($(wildcard /usr/local/opt/openssl@3/lib/libcrypto.a),)
    CRYPTOLIB = /usr/local/opt/openssl@3/lib/libcrypto.a
  else
    ifneq ($(wildcard /opt/local/libexec/openssl3/lib/libcrypto.a),)
      CRYPTOLIB = /opt/local/libexec/openssl3/lib/libcrypto.a
    endif
  endif
  ifneq ($(wildcard /opt/local/lib/libiconv.a),)
    ICONVLIB = /opt/local/lib/libiconv.a
  endif
  ifneq ($(wildcard /opt/homebrew/lib/libmysqlclient.a),)
    MYSQLLIBS = /opt/homebrew/lib/libmysqlclient.a
  endif
endif

# Skip freetype for conda build; not needed for utils, and the Mac build environment has
# freetype installed but we don't want to use the system libraries because they can be
# for a newer OSX version than the conda build target, and can be incompatible.
ifneq (${CONDA_BUILD},1)
  FREETYPECFLAGS = $(shell freetype-config --cflags  2> /dev/null)
  # Ubuntu does not have freetype-config anymore
  ifeq (${FREETYPEFLAGS},)
     FREETYPECFLAGS =  $(shell pkg-config freetype2 --cflags 2> /dev/null )
  endif

  # use the static library on OSX
  ifeq (${IS_HGWDEV},no)
    ifeq (${FREETYPELIBS},)
      ifeq ($(UNAME_S),Darwin)
        FREETYPELIBS =  $(shell freetype-config --libs --static 2> /dev/null )
      else
        FREETYPELIBS =  $(shell freetype-config --libs 2> /dev/null )
        # Ubuntu does not have freetype-config anymore
        ifeq (${FREETYPELIBS},)
          FREETYPELIBS =  $(shell pkg-config freetype2 --libs 2> /dev/null )
        endif
      endif
    endif
  endif

  ifneq (${FREETYPECFLAGS},)
  FREETYPECFLAGS += -DUSE_FREETYPE
  endif

  HG_INC += ${FREETYPECFLAGS}
  L += ${FREETYPELIBS}
endif

ifeq (${HOSTNAME},cirm-01)
  FULLWARN = yes
endif

ifeq (${PTHREADLIB},)
  PTHREADLIB=-lpthread
endif

ifneq ($(UNAME_S),Darwin)
  L+=${PTHREADLIB}
else
  ifeq (${ICONVLIB},)
    ICONVLIB=-liconv
  endif
endif

# autodetect UCSC installation of hal:
ifeq (${HALDIR},)
    # ONLY on hgwdev, not any other machine here (i.e. hgcompute-01)
    ifeq (${IS_HGWDEV},yes)
      HALDIR = /hive/groups/browser/hal/build/hal.2024-08-20
      ifneq ($(wildcard ${HALDIR}),)
        ifeq (${USE_HAL},)
          USE_HAL=1
        endif
      endif
    endif
endif

ifeq (${USE_HAL},1)
    # force static libraries to keep programs portable
    HDF5DIR=/hive/groups/browser/hal/build/hdf5-1.12.0
    HDF5LIBDIR=${HDF5DIR}/local/lib
    HDF5LIBS=${HDF5LIBDIR}/libhdf5_cpp.a ${HDF5LIBDIR}/libhdf5.a ${HDF5LIBDIR}/libhdf5_hl.a
    HALLIBS=${HALDIR}/hal/lib/libHalBlockViz.a ${HALDIR}/hal/lib/libHalMaf.a ${HALDIR}/hal/lib/libHalLiftover.a ${HALDIR}/hal/lib/libHalLod.a ${HALDIR}/hal/lib/libHal.a ${HALDIR}/sonLib/lib/sonLib.a ${HDF5LIBS} -lcurl -lstdc++
    HG_DEFS+=-DUSE_HAL
    HG_INC+=-I${HALDIR}/inc -I${HALDIR}/hal/blockViz/inc
endif
# on hgwdev, include HAL by defaults
ifeq (${IS_HGWDEV},yes)
   L+=${HALLIBS}
endif

ifeq (${USE_HIC},)
    USE_HIC=1
endif

ifeq (${USE_HIC},1)
    HG_DEFS+=-DUSE_HIC
endif

# autodetect where libm is installed
ifeq (${MLIB},)
  MLIB=-lm
endif

# autodetect where png is installed
ifeq (${PNGLIB},)
      PNGLIB := $(shell libpng-config --ldflags  || true)
endif
ifeq (${PNGLIB},)
  PNGLIB=-lpng
endif
ifeq (${PNGINCL},)
  PNGINCL := $(shell libpng-config --I_opts  || true)
#       $(info using libpng-config to set PNGINCL: ${PNGINCL})
endif

# autodetect where mysql includes and libraries are installed
# do not need to do this during 'clean' target (this is very slow for 'clean')
ifneq ($(MAKECMDGOALS),clean)

  # set MYSQL include path
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

  # this does *not* work on Mac OSX with the dynamic libraries
  ifneq ($(UNAME_S),Darwin)
    ifeq (${MYSQLLIBS},)
      MYSQLLIBS := $(shell mysql_config --libs || true)
#        $(info using mysql_config to set MYSQLLIBS: ${MYSQLLIBS})
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
ifeq (${IS_HGWDEV},no)
  ifeq ($(UNAME_S),Darwin)
    MYSQLLIBS += -lstdc++
  else
    MYSQLLIBS += -lstdc++ -lrt
  endif
endif

ifeq (${ZLIB},)
  ZLIB=-lz
endif

# on hgwdev, use the static libraries
ifeq (${IS_HGWDEV},yes)
   FULLWARN = yes
   L+=/hive/groups/browser/freetype/freetype-2.10.0/objs/.libs/libfreetype.a -lbz2
   L+=/usr/lib64/libssl.a /usr/lib64/libcrypto.a -lkrb5 -lk5crypto -ldl
   PNGLIB=/usr/lib64/libpng.a
   PNGINCL=-I/usr/include/libpng15
   MYSQLINC=/usr/include/mysql
   MYSQLLIBS=/usr/lib64/libmysqlclient.a /usr/lib64/libssl.a /usr/lib64/libcrypto.a -lkrb5 -ldl -lz
   MYSQLLIBS += /usr/lib/gcc/x86_64-redhat-linux/4.8.5/libstdc++.a /usr/lib64/librt.a
else
   ifeq (${CONDA_BUILD},1)
       L+=${PREFIX}/lib/libssl.a ${PREFIX}/lib/libcrypto.a -ldl
   else
       ifneq (${SSLLIB},)
          L+=${SSLLIB}
       else
          L+=-lssl
       endif
       ifneq (${CRYPTOLIB},)
          L+=${CRYPTOLIB}
       else
          L+=-lcrypto -ldl
       endif
       ifeq (${DLLIB},)
          L+=-ldl
       endif
   endif
endif

#global external libraries
L += $(kentSrc)/htslib/libhts.a

L+=${PNGLIB} ${MLIB} ${ZLIB} ${ICONVLIB}
HG_INC+=${PNGINCL}

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
      ifeq (${FULLWARN},yes)
        HG_WARN = -Wall -Werror -Wformat -Wformat-security -Wimplicit -Wreturn-type -Wempty-body -Wunused-but-set-variable -fno-common
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


CC_PROG_OPTS = ${COPT} ${CFLAGS} ${HG_DEFS} ${LOWELAB_DEFS} ${HG_WARN} ${HG_INC} ${XINC}
%.o: %.c
	${CC} ${CC_PROG_OPTS}  -o $@ -c $<

# autodetect UCSC installation of node.js:
ifeq (${NODEBIN},)
    NODEBIN = /cluster/software/src/node-v0.10.24-linux-x64/bin
    ifeq ($(wildcard ${NODEBIN}),)
	NODEBIN=
    endif
endif

# node.js tools: jshint, jsx, jsxhint, uglifyjs, ...
ifeq (${JSHINT},)
    JSHINT=${NODEBIN}/jshint
    ifeq ($(wildcard ${JSHINT}),)
	    JSHINT=true
    endif
endif
ifeq (${JSXHINT},)
    JSXHINT=${NODEBIN}/jsxhint
    ifeq ($(wildcard ${JSXHINT}),)
        JSXHINT=true
    endif
endif
ifeq (${JSX},)
    JSX=${NODEBIN}/jsx
    ifeq ($(wildcard ${JSX}),)
        JSX=true
    endif
endif
ifeq (${UGLIFYJS},)
    UGLIFYJS=${NODEBIN}/uglifyjs
    ifeq ($(wildcard ${UGLIFYJS}),)
        UGLIFYJS=true
    endif
endif
