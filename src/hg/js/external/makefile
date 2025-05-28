kentSrc = ../../..
include ../../../inc/common.mk
include ../install.mk

JS_FILES= BackboneExtend.js \
	es5-sham.4.0.3.min.js es5-shim.4.0.3.min.js \
	immutable.3.7.4.min.js \
	jquery.bifrost.1.0.1.min.js \
	lodash.3.10.0.compat.min.js \
	react-with-addons-0.12.2.min.js \
	sorttable.js tus.min.js \
	shepherd.min.js

# Don't bother jshint'ing external libs.
jshint:

doInstall:
	@mkdir -p ${DEST}/
	rsync -a ${JS_FILES} ${DEST}/
