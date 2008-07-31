CC=gcc
ifeq (${COPT},)
    COPT=-O
endif
CFLAGS=
HG_DEFS=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_GNU_SOURCE -DMACHTYPE_${MACHTYPE}
HG_INC=-I../inc -I../../inc -I../../../inc -I../../../../inc -I../../../../../inc

ifeq (${HG_WARN},)
  ifeq (darwin,$(findstring darwin,${OSTYPE}))
      HG_WARN = -Wall -Werror -Wno-unused-variable -Wno-long-double
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

ifeq (${SCRIPTS},)
    SCRIPTS=/cluster/bin/scripts
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
MKDIR=mkdir -p
ifeq (${STRIP},)
   STRIP=strip
endif
CVS=cvs

# portable naming of compiled executables: add ".exe" if compiled on 
# Windows (with cygwin).
ifeq (${OS}, Windows_NT)
  AOUT=a.exe
  EXE=.exe
else
  AOUT=a.out
  EXE=
endif

# location of stringify program
STRINGIFY = ${BINDIR}/stringify

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<
