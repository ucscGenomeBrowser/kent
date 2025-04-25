kentSrc = ../..
include ../../inc/common.mk

GIT = git
GITUP = ${GIT} pull

ifeq (${FIND},)
    INSIDEGIT := $(shell git rev-parse --is-inside-work-tree 2>/dev/null)
    ifeq (${INSIDEGIT},true)
	FIND = gitFiles
    else
	FIND = find
    endif
endif

# if you need to pick up files not checked-in yet for rsync,
# you can run it like this:
#
#   make FIND=find
#
# but be careful as it will rsync everything over including junk

# REMEMBER: HTDOCSEXTRA 
# htdocsExtra is a repository that contains large files that we
# do not wish to burden the main kent repo with.
# You can check it out and make there too.
# If you think there is a file missing, see if it is under the htdocsExtra repo.

ifeq (${DOCUMENTROOT_USER},)
    DOCUMENTROOT_USER=${DOCUMENTROOT}-${USER}
endif

user:
	if test -d ${DOCUMENTROOT_USER}; then \
	    ${MAKE} doInstall destDir=${DOCUMENTROOT_USER} ; \
	fi

alpha:
	${MAKE} doInstall destDir=${DOCUMENTROOT} 

beta:
	@if [ "${FIND}" = "find" ] ; then echo Error: FIND=find not allowed with make beta >& 2 ; exit 1 ; fi
	${GITUP}
	cp -p ${CPREMDESTOPT} ${DOCUMENTROOT}/assemblyRequest.html ${DOCUMENTROOT}-beta/assemblyRequest.html
	${MAKE} doInstall destDir=qateam@hgwbeta:${DOCUMENTROOT} 
	${MAKE} doInstall destDir=${DOCUMENTROOT}-beta
	rsync --chmod=Fu=rwx,Fg=rwx,Fo=rx --whole-file --times -a ${DOCUMENTROOT}-beta/assemblyRequest.html qateam@hgwbeta:${DOCUMENTROOT}/

# Run rsync. Care is taken to preserve times and modes on files.  rsync is
# smart about recreating files under the current users so that times and
# permissions may be modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
rsyncOpts = --whole-file --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' --exclude='cpAndLinkToResourceFiles.pl' --exclude='style/*'
doInstall:
	umask 002; rsync ${rsyncOpts} ${excl} ${incl} `$(FIND)` ${destDir}

# Other targets that may be invoked by ../makefile:
compile:

install: alpha

clean:
