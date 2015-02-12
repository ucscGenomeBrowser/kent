# simple.mk: simply jshint and rsync JS_FILES.  Include this after defining JS_FILES.

jshint:
	${JSHINT} --config ${JS_DIR}/jshintrc.json ${JS_FILES}

compile: jshint

doInstall: jshint
	@mkdir -p ${DEST}/
	rsync -a ${JS_FILES} ${DEST}/
