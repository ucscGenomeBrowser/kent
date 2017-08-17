kentSrc = ../..
include ../../inc/common.mk
include install.mk

SUBDIRS= external model react
include subdirs.mk

# UCSC js files must pass jshint
UCSC_JS_FILES=ajax.js alleles.js autocomplete.js edwPersona.js \
    hgCustom.js hgCollection.js  hgGateway.js hgGtexTrackSettings.js \
    hgHubConnect.js hgTracks.js \
    hgVai.js hui.js jsHelper.js utils.js ddcl.js subCfg.js \
    encodeProject.js encodeDataSummary.js encodeDataMatrix.js encodeChipMatrix.js encodeMatrix.js d3.dendrograms.js \
	cdwSummaryJs.js spectrum.min.js

# STATIC files must pass jshint
STATIC_FILES=staticDoc.js dbDbTaxonomy.js

# JQUERY files and other third pary files which have NOT BEEN TOUCHED by UCSC
JQUERY_FILES_UNTOUCHED= \
    d3pie.min.js \
    jquery.js jquery.imgareaselect.js jquery-ui.js jquery.contextmenu.js \
    jquery.cookie.js jquery.watermark.js jquery.watermarkinput.js jquery.ui.colorPicker.js \
    jquery.floatheader.js jquery.rotateTableCellContent.js jquery.plugins.js \
    jquery.validate.min.js jquery.history.js jquery.treetable.js mousetrap.min.js dagre-d3.js

# JQUERY or other third pary files which HAVE BEEN MODIFIED by UCSC
JQUERY_FILES_MODIFIED=jquery.tablednd.js ui.dropdownchecklist.js

JQUERY_FILES=${JQUERY_FILES_UNTOUCHED} ${JQUERY_FILES_MODIFIED}

# SYMLINKED files are include in CGIs via a version or timestamped symlink
#           to force client side updates
SYMLINKED_FILES=${UCSC_JS_FILES} ${JQUERY_FILES}

ifdef LOWELAB
    # LOWE lab files should be forced through jshint (but currently aren't)
    SYMLINKED_FILES += lowetooltip.js loweautocomplete.js
endif

jshintTop:
	${JSHINT} ${UCSC_JS_FILES} ${STATIC_FILES}

doInstallTop: jshintTop
	@mkdir -p ${DEST}/
	../utils/cpAndLinkToResourceFiles.pl ${extra} -exclude='makefile' \
			-versionFile=../inc/versionInfo.h -destDir=${DEST} ${SYMLINKED_FILES}
	for file in ${STATIC_FILES}; do \
	    rsync -a $$file ${DEST}/; \
	done

doInstall: doInstallTop ${SUBDIRS:%=%.doInstall}
