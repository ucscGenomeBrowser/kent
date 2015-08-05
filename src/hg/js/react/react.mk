# react.mk -- common makefile settings and targets for compiling React JSX into javascript bundle
# Include this after settings the variables JSX_FILES and BUNDLE_FILE, in subdir makefiles.

# This directory holds JS files compiled from JSX (if the environment includes JSX)
COMPILED_JS_DESTDIR=compiled

# This directory holds bundled files that are in git so that mirrors don't need to install node:
BUNDLE_DESTDIR=../bundle

# Don't remove the bundle files when cleaning -- they're under git control!
clean:
	rm -f ${COMPILED_JS_DESTDIR}/*

compile: jsx

jshint:
	${JSXHINT} --config ${JS_DIR}/jshintrc.json ${JSX_FILES}

jsx: jshint
	rm -rf ${COMPILED_JS_DESTDIR}
	mkdir -p ${COMPILED_JS_DESTDIR}
	${JSX} -x jsx . ${COMPILED_JS_DESTDIR} 2>&1
	${UGLIFYJS} ${COMPILED_JS_DESTDIR}/*.js -o ${BUNDLE_DESTDIR}/${BUNDLE_FILE}

doInstall: jsx
	@mkdir -p ${DEST}/
	rsync -a ${BUNDLE_DESTDIR}/${BUNDLE_FILE} ${DEST}/;
