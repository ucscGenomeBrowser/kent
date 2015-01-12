# react.mk -- common makefile settings and targets for compiling React JSX into javascript bundle
# Include this after settings the variables JSX_FILES and BUNDLE_FILE, in subdir makefiles.

# This directory holds JS files compiled from JSX (if the environment includes JSX)
COMPILED_JS_DESTDIR=compiled

# This directory holds bundled files that are in git so that mirrors don't need to install node:
BUNDLE_DESTDIR=../bundle

# jsxhint: off unless JSXHINT is already in environment
ifeq (${JSXHINT},)
    JSXHINT=true
endif

# jsx: off unless JSX is already in environment (that means edits to .jsx files don't affect .js!)
ifeq (${JSX},)
    JSX=true
endif

# uglifyjs: off unless UGLIFYJS is already in env (that means that rebuilt .js doesn't get bundled!)
ifeq (${UGLIFYJS},)
    UGLIFYJS=true
endif

clean:
	rm -f compiled/*

compile: jsx

jsx:
	${JSXHINT} ${JSX_FILES}
	mkdir -p ${COMPILED_JS_DESTDIR}
	${JSX} -x jsx . ${COMPILED_JS_DESTDIR} 2>&1
	${UGLIFYJS} ${COMPILED_JS_DESTDIR}/*.js -o ${BUNDLE_DESTDIR}/${BUNDLE_FILE}

doInstall: jsx
	@mkdir -p ${DEST}/
	rsync -a ${BUNDLE_DESTDIR}/${BUNDLE_FILE} ${DEST}/;
