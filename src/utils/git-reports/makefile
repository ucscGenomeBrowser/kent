include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkweb.a 

A = git-reports

O = $(A).o

$(A): ${O} ${MYLIBS}
	@${MKDIR} "${DESTDIR}${BINDIR}"
	${CC} ${COPT} ${CFLAGS} -o ${DESTDIR}${BINDIR}/${A}${EXE} $O ${MYLIBS} ${L}
	${STRIP} ${DESTDIR}${BINDIR}/${A}${EXE}

clean::
	rm -f ${O} *.tmp

install:
	${MAKE} BINDIR=/cluster/bin/${MACHTYPE}

test: ${A}$(EXE)
	rm -fr ${HOME}/public_html/git-reports
	mkdir ${HOME}/public_html/git-reports
	${A}$(EXE) v251_base v252_preview 2011-05-10 2011-05-17 v252 /cluster/bin/build/build-kent ${HOME}/public_html/git-reports review
	# note can use -verbose=2



