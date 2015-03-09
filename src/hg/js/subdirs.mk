# subdirs.mk -- subdir recursion defaults.  Include this after setting SUBDIRS
# (and after including jsInstall.mk).

%.clean:
	cd $* && echo $* && ${MAKE} clean

clean: ${SUBDIRS:%=%.clean}

%.jshint:
	cd $* && echo $* && ${MAKE} jshint

jshint: ${SUBDIRS:%=%.jshint}

%.compile:
	cd $* && echo $* && ${MAKE} compile

compile: ${SUBDIRS:%=%.compile}

%.install:
	cd $* && echo $* && ${MAKE} doInstall DEST=${DOCUMENTROOT}/js

install: ${SUBDIRS:%=%.install}

%.doInstall:
	cd $* && echo $* && ${MAKE} doInstall

doInstall: ${SUBDIRS:%=%.doInstall}
