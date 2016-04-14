# install.mk - user/alpha/beta installation rules shared by all js dirs.

user:
	@if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/js; \
	else \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}/js/${USER}; \
	fi

top:
	@if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstallTop DEST=${DOCUMENTROOT}-${USER}/js; \
	else \
	    ${MAKE} doInstallTop DEST=${DOCUMENTROOT}/js/${USER}; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

topAlpha:
	${GITUP}
	${MAKE} doInstallTop DEST=${DOCUMENTROOT}/js

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}-beta/js extra=-forceVersionNumbers

clean:

compile:

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js
