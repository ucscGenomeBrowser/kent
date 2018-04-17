kentSrc = ../..
A = git-reports
include $(kentSrc)/inc/userApp.mk
L += -lm ${SOCKETLIB}

install::
	${MAKE} BINDIR=/cluster/bin/${MACHTYPE}


# This is meant to help the developer,
# not to be an automatically-run test of correct output
mytest:
	rm -fr ${HOME}/public_html/git-reports
	mkdir ${HOME}/public_html/git-reports
	${A}$(EXE) v251_base v252_preview 2011-05-10 2011-05-17 v252 /hive/groups/browser/newBuild/kent ${HOME}/public_html/git-reports review
	# note can use -verbose=2
