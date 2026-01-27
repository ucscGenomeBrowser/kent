# if CC is undefined, set it to gcc
CC?=gcc
# allow the somewhat more modern C syntax, e.g. 'for (int i=5; i<10, i++)'
CFLAGS += -std=c99

# This is required to get the cgiLoader.mk compile target to work.  for some
# reason, make's %.o: %.c overrides the rule below, cause the compiles to fail
# due to lack of -I flags in rule.  Running with make -r to not use built-in
# rules fixes, however MAKEFLAGS += -r doesn't do the trick, this does.
# make is a very mysterious fellow traveler.
GNUMAKEFLAGS += -r

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
# to check for differences in Linux OS version
KERNEL_REL := $(shell uname -r)
# to check for builds on hgwdev
HOSTNAME = $(shell uname -n)

# Semi-static builds, normally done in Docker
#
# use make SEMI_STATIC=yes to enable
#  
# These use static libraries except for -ldl, -lm, and -lc
# which must be dynamic.
#
ifeq (${SEMI_STATIC},yes)
    # switch to static libraries
    STATIC_PRE = -Wl,-Bstatic
endif
L = ${STATIC_PRE}

ifeq (${HOSTNAME},hgwdev)
  IS_HGWDEV = yes
  OURSTUFF = /cluster/software/r9
else
  ifeq (${HOSTNAME},hgwdev-old.gi.ucsc.edu)
    IS_HGWDEV = yes
    OURSTUFF = /cluster/software
  else
     IS_HGWDEV = no
  endif
endif

ifeq (${ZLIB},)
  ifneq ($(wildcard /lib64/libz.a),)
    ZLIB=/lib64/libz.a
  else
    ZLIB=-lz
  endif
endif

# for Darwin (Mac OSX), use static libs when they can be found
ifeq ($(UNAME_S),Darwin)
  ifneq ($(wildcard /opt/local/include/openssl/ssl.h),)
    HG_INC += -I/opt/local/include
  endif
  # on M1, the directory changed
  ifneq ($(wildcard /opt/homebrew/include/openssl/ssl.h),)
    HG_INC += -I/opt/homebrew/include
    L += -L/opt/homebrew/lib/
  endif
  ifneq ($(wildcard /opt/local/lib/libz.a),)
    ZLIB = /opt/local/lib/libz.a
  endif
  ifneq ($(wildcard /opt/local/lib/libpng.a),)
    PNGLIB = /opt/local/lib/libpng.a
  endif
  ifeq (${BZ2LIB},)
    ifneq ($(wildcard /opt/local/lib/libbz2.a),)
      BZ2LIB=/opt/local/lib/libbz2.a
    endif
  endif
  ifneq ($(wildcard /opt/local/lib/libfreetype.a),)
    FREETYPELIBS = /opt/local/lib/libfreetype.a
  endif
  ifneq ($(wildcard /opt/local/lib/libbrotlidec.a),)
    FREETYPELIBS += /opt/local/lib/libbrotlidec.a /opt/local/lib/libbrotlicommon.a
  else
    ifneq ($(wildcard /usr/local/Cellar/brotli/1.1.0/lib/libbrotlidec.a),)
      FREETYPELIBS += /usr/local/Cellar/brotli/1.1.0/lib/libbrotlidec.a /usr/local/Cellar/brotli/1.1.0/lib/libbrotlicommon.a
    endif
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
  ifneq ($(wildcard /usr/local/Cellar/mariadb/11.6.2/lib/libmariadbclient.a),)
      MYSQLLIBS = /usr/local/Cellar/mariadb/11.6.2/lib/libmariadbclient.a
  else
    ifneq ($(wildcard /opt/homebrew/lib/libmysqlclient.a),)
      MYSQLLIBS = /opt/homebrew/lib/libmysqlclient.a
    endif
  endif
endif

ifeq (${HOSTNAME},cirm-01)
  FULLWARN = yes
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /data/home/galt/lib64/libmariadb.a),)
      MYSQLLIBS = /data/home/galt/lib64/libmariadb.a
    endif
  endif
  ifeq (${SSLLIB},)
    ifneq ($(wildcard /data/home/galt/lib64/libssl.a),)
      SSLLIB = /data/home/galt/lib64/libssl.a
    endif
  endif
  ifeq (${CRYPTOLIB},)
    ifneq ($(wildcard /data/home/galt//lib64/libcrypto.a),)
      CRYPTOLIB = /data/home/galt/lib64/libcrypto.a
    endif
  endif
  HG_INC += -I/data/home/galt/include
  HG_INC += -I/data/home/galt/include/mariadb
endif

ifeq (${SSLLIB},)
  ifneq ($(wildcard /usr/lib64/libssl.a),)
    SSLLIB = /usr/lib64/libssl.a
  endif
endif
ifeq (${CRYPTOLIB},)
  ifneq ($(wildcard /usr/lib64/libcrypto.a),)
    CRYPTOLIB = /usr/lib64/libcrypto.a
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

ifeq (${PTHREADLIB},)
  PTHREADLIB=-lpthread
endif

ifneq ($(UNAME_S),Darwin)
  L+=${PTHREADLIB}
  ifneq ($(filter 3.%, ${KERNEL_REL}),)
     # older linux needed libconv
    XXXICONVLIB=-liconv
  endif
else
  ifeq (${ICONVLIB},)
    ICONVLIB=-liconv
  endif
endif

# autodetect UCSC installation of hal:
ifeq (${HALDIR},)
    # ONLY on hgwdev, not any other machine here (i.e. hgcompute-01)
    ifeq (${IS_HGWDEV},yes)
      HALDIR = /hive/groups/browser/hal/build/hal.2024-12-12
      #HALDIR = /hive/groups/browser/hal/build/rocky9/hal.2024-12-18
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
    HALLIBS=${HALDIR}/hal/lib/libHalBlockViz.a ${HALDIR}/hal/lib/libHalMaf.a ${HALDIR}/hal/lib/libHalLiftover.a ${HALDIR}/hal/lib/libHalLod.a ${HALDIR}/hal/lib/libHal.a ${HALDIR}/sonLib/lib/sonLib.a ${HDF5LIBS} ${ZLIB}
    ifeq (${HOSTNAME},hgwdev)
        HALLIBS += /usr/lib/gcc/x86_64-redhat-linux/11/libstdc++.a
    else
      ifeq (${HOSTNAME},hgwdev-old.gi.ucsc.edu)
          HALLIBS += /usr/lib/gcc/x86_64-redhat-linux/4.8.5/libstdc++.a
      else
          HALLIBS += -lstdc++
      endif
    endif
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
    # newer distros do not have the mysql_config symlink anymore
    ifeq (, $(shell which mysql_config))
        MYSQLCONFIG := mariadb_config
    else
        MYSQLCONFIG := mysql_config
    endif
	
    MYSQLINC := $(shell ${MYSQLCONFIG} --include | sed -e 's/-I//' || true)
        # $(info using mysql_config to set MYSQLINC: ${MYSQLINC})
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
      # mysql_config --libs includes -lm, however libm must be a dynamic library
      # so to handle SEMI_STATIC it is removed here and will be added at the end
      MYSQLLIBS := $(shell ${MYSQLCONFIG} --libs | sed 's/-lm$$//' || true)
#        $(info using mysql_config to set MYSQLLIBS: ${MYSQLLIBS})
    endif
  endif
  ifeq ($(findstring src/hg/,${CURDIR}),src/hg/)
    ifeq (${MYSQLINC},)
        $(error can not find installed mysql development system)
    endif
  endif
  ifeq (${MYSQLLIBS},)
    ifneq ($(wildcard /usr/lib64/libmysqlclient.a),)
      MYSQLLIBS = /usr/lib64/libmysqlclient.a
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

ifeq (${BZ2LIB},)
  ifneq ($(wildcard /lib64/libbz2.a),)
    BZ2LIB=/lib64/libbz2.a
   else
    BZ2LIB=-lbz2
   endif
endif

# on hgwdev, use the static libraries
ifeq (${IS_HGWDEV},yes)
   HG_INC += -I${OURSTUFF}/include
   HG_INC += -I${OURSTUFF}/include/mariadb 
   FULLWARN = yes
   L+=/hive/groups/browser/freetype/freetype-2.10.0/objs/.libs/libfreetype.a
   L+=${OURSTUFF}/lib/libcurl.a
   L+=${OURSTUFF}/lib64/libssl.a ${OURSTUFF}/lib64/libcrypto.a

   ifeq (${HOSTNAME},hgwdev)
       PNGLIB=${OURSTUFF}/lib/libpng.a
       PNGINCL=-I${OURSTUFF}/include/libpng16
   else
       PNGLIB=/usr/lib64/libpng.a
       PNGINCL=-I/usr/include/libpng15
   endif

   MYSQLINC=/usr/include/mysql
   MYSQLLIBS=${OURSTUFF}/lib64/libmariadbclient.a ${OURSTUFF}/lib64/libssl.a ${OURSTUFF}/lib64/libcrypto.a ${ZLIB}

   ifeq (${HOSTNAME},hgwdev)
       MYSQLLIBS += /usr/lib/gcc/x86_64-redhat-linux/11/libstdc++.a /usr/lib64/librt.a
   else
       MYSQLLIBS += /usr/lib/gcc/x86_64-redhat-linux/4.8.5/libstdc++.a /usr/lib64/librt.a
   endif

else
   L+=-lcurl
   ifeq (${CONDA_BUILD},1)
       L+=${PREFIX}/lib/libssl.so ${PREFIX}/lib/libcrypto.so
   else
       ifneq (${SSLLIB},)
          L+=${SSLLIB}
       else
          L+=-lssl
       endif
       ifneq (${CRYPTOLIB},)
          L+=${CRYPTOLIB}
       else
          L+=-lcrypto
       endif
   endif
endif

#global external libraries
L += $(kentSrc)/htslib/libhts.a
L+=${PNGLIB} ${MLIB} ${ZLIB} ${BZ2LIB} ${ICONVLIB}
HG_INC+=${PNGINCL}

# NOTE: these must be last libraries and must be dynamic.
# We switched by to dynamic with SEMI_STATIC
ifeq (${SEMI_STATIC},yes)
    # switch back to dynamic libraries
    DYNAMIC_PRE = -Wl,-Bdynamic
endif
DYNAMIC_LIBS = ${DYNAMIC_PRE} -ldl -lm -lc

L+= ${DYNAMIC_LIBS}


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

# location of interperted version of stringify program
STRINGIFY = ${kentSrc}/utils/stringify/stringifyEz

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
	${CC} ${CC_PROG_OPTS} -o $@ -c $<

# autodetect UCSC installation of node.js:
ifeq (${NODEBIN},)
    NODEBIN = /cluster/software/src/node-v22.19.0-linux-x64/bin
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

# OSX does not have the --remove-destination option. 
# We use this option to make sure that the username and date of files lets us figure out
# who was the one that built the current version of a file
ifeq ($(UNAME_S),Darwin)
    CPREMDESTOPT=-f
else
    CPREMDESTOPT=--remove-destination
endif

ifeq (${LOWELAB}, 1)
  O += lowelabPfamHit.o lowelabArkinOperonScore.o lowelabTIGROperonScore.o loweutils.o \
  geneTree.o cogsxra.o jgiGene.o snoRNAs.o sargassoSeaXra.o minGeneInfo.o gbRNAs.o easyGene.o arcogdesc.o arCOGs.o cddDesc.o cddInfo.o \
  megablastInfo.o alignInfo.o ec.o codeBlastScore.o ecAttribute.o tRNAs.o
endif

