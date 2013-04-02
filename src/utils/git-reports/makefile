kentSrc = ../..
A = git-reports
include $(kentSrc)/inc/userApp.mk
L += -lm ${SOCKETLIB}

install::
	${MAKE} BINDIR=/cluster/bin/${MACHTYPE}

test: ${A}$(EXE)
	rm -fr ${HOME}/public_html/git-reports
	mkdir ${HOME}/public_html/git-reports
	${A}$(EXE) v251_base v252_preview 2011-05-10 2011-05-17 v252 /cluster/bin/build/build-kent ${HOME}/public_html/git-reports review
	# note can use -verbose=2
