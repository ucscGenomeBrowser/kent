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
	${A}$(EXE) origin/v223_branch origin/v224_branch 2010-01-19 2010-02-02 v224 /cluster/bin/build/buildrepo ${HOME}/public_html/git-reports branch
	# note can use -verbose=2



