kentSrc = ../../..
include ../../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

ifeq (${FIND},)
    INSIDEGIT := $(shell git rev-parse --is-inside-work-tree 2>/dev/null)
    ifeq (${INSIDEGIT},true)
       FIND = gitFiles
    else
       FIND = find
    endif
endif

CSS_FILES=HGStyle.css assemblySearch.css autocomplete.css blueStyle.css \
    encodeProject.css facets.css floretStyle.css gar.css \
    hgHubConnect.css hgMyData.css jquery-ui.css jquery.contextmenu.css \
    jquery.ui.autocomplete.css jquery.ui.colorPicker.css \
    jquery.treetable.css jquery.treetable.theme.default.css  \
    jWest.css jWest.afterNiceMenu.css mouseOver.css \
    theme-modern.css ui.dropdownchecklist.css nice_menu.css \
    encodeDataMatrix.css encodeDataSummary.css userAccounts.css \
    encode3.css encode3Ucsc.css bootstrap.min.css \
    gbStatic.css gb.css gbAfterMenu.css hgGtexTrackSettings.css \
    spectrum.min.css font-awesome.min.css \
    theme-classic.css theme-dark.css theme-modern.css \
    bootstrap-3-3-7.min.css shepherd.css

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/style; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}-beta/style extra=-forceVersionNumbers

clean:

compile:

# Run rsync. Care is taken to preserve times and modes on files.  rsync is
# smart about recreating files under the current users so that times and
# permissions may be modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
rsyncOpts = --whole-file --times --recursive --omit-dir-times --relative --cvs-exclude --exclude='makefile' --exclude='cpAndLinkToResourceFiles.pl' --exclude='style/*'

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/style

doInstall:
	@mkdir -p ${DEST}/
	../../utils/cpAndLinkToResourceFiles.pl ${extra} -destDir=${DEST} -versionFile=../../inc/versionInfo.h ${CSS_FILES}
	umask 002; rsync ${rsyncOpts} `$(FIND) images` ${DEST}
