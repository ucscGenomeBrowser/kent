include ../../inc/common.mk

# recursive target to do install to DEST, with extra excludes EXCL.

# UCSC js files must pass jshint
UCSC_JS_FILES=ajax.js alleles.js autocomplete.js edwPersona.js \
    hgCustom.js hgGateway.js hgHubConnect.js hgTracks.js \
    hgVarAnnogrator.js hui.js utils.js ddcl.js subCfg.js \
    encodeProject.js encodeDataSummary.js encodeDataMatrix.js encodeChipMatrix.js encodeMatrix.js
    
# STATIC files must pass jshint
STATIC_FILES=staticDoc.js

# JQUERY files and other third pary files which have NOT BEEN TOUCHED by UCSC
JQUERY_FILES_UNTOUCHED=jquery.js jquery.tablednd.js \
    jquery.imgareaselect.js jquery-ui.js jquery.contextmenu.js \
    ui.dropdownchecklist.js \
    jquery.cookie.js jquery.watermarkinput.js \
    jquery.floatheader.js jquery.rotateTableCellContent.js jquery.plugins.js \
    jquery.validate.min.js jquery.history.js

# JQUERY or other third pary files which HAVE BEEN MODIFIED by UCSC
JQUERY_FILES_MODIFIED=jquery.tablednd.js ui.dropdownchecklist.js

JQUERY_FILES=${JQUERY_FILES_UNTOUCHED} ${JQUERY_FILES_MODIFIED}

# SYMLINKED files are include in CGIs via a version or timestamped symlink
#           to force client side updates
SYMLINKED_FILES=${UCSC_JS_FILES} ${JQUERY_FILES}

ifdef LOWELAB
    # LOWE lab files should be forced through jshint (but currently aren't)
    #UCSC_JS_FILES += lowetooltip.js loweautocomplete.js
    SYMLINKED_FILES += lowetooltip.js loweautocomplete.js
endif

user:
	if test -d ${DOCUMENTROOT}-${USER}; then \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}-${USER}/js; \
	else \
	    ${MAKE} doInstall DEST=${DOCUMENTROOT}/js/${USER}; \
	fi

alpha:
	${GITUP}
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

beta:
	${GITUP} 
	${MAKE} doInstall DEST=${DOCUMENTROOT}-beta/js extra=-forceVersionNumbers

clean:

compile:

jshint:
ifdef JSHINTL
	${JSHINT} --exclude-path=jshint.exclude *.js
else
	@echo Need JSHINT environment variable defined first.
endif

install:
	${MAKE} doInstall DEST=${DOCUMENTROOT}/js

doInstall:
ifdef JSHINT
	${JSHINT} ${UCSC_JS_FILES} ${STATIC_FILES}
endif
	@mkdir -p ${DEST}/
	../utils/cpAndLinkToResourceFiles.pl ${extra} -exclude='makefile' \
			-versionFile=../inc/versionInfo.h -destDir=${DEST} ${SYMLINKED_FILES}
	for file in ${STATIC_FILES}; do \
	    rsync -a $$file ${DEST}/; \
	done
