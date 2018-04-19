
UTIL_DIRS = \
    tagStormHoist \
    tagStormReformat \
    tagStormFromTab \
    jsonToTagStorm \
    tagStormInfo \
    tagStormJoinTab \
    tagStormToHtml \
    tagStormToJson \
    tagStormDeleteTags \
    tagStormRenameTags \
    tagStormRenameVals \
    tagStormToCsv \
    tagStormToTab \
    tagStormToSql \
    tagStormCheck \
    tagStormArrayToCsv

# Used by cdw webworks, these will get copied to CGI_BIN dir to be run by CGIs
CGI_DIRS = \
    tagStormToCsv \
    tagStormToHtml \
    tagStormToTab

utils:
	${MAKE} buildUtils

buildUtils: $(UTIL_DIRS:%=%.buildUtils)
%.buildUtils:
	cd $* && $(MAKE)

test::	${UTIL_DIRS:%=%.test}
%.test:
	cd $* && ${MAKE} test

clean:: $(UTIL_DIRS:%=%.clean)
	- find . -name '*.o' -exec rm {} +
%.clean:
	(cd $* && ${MAKE} clean)

# have to include down here or it will mess up the default target
kentSrc = ..
include $(kentSrc)/inc/userApp.mk
cgi:: $(CGI_DIRS:%=%.cgi)
%.cgi: 
	(cd $* && ${MAKE} compile)
	chmod a+rx $*/$*${EXE}
	rm -f ${CGI_BIN}-${USER}/$*
	mv $*/$*${EXE} ${CGI_BIN}-${USER}/$*

alpha:: $(CGI_DIRS:%=%.alpha)
%.alpha:
	(cd $* && ${MAKE} compile)
	${STRIP} $*/$*${EXE}
	chmod g+w $*/$*${EXE}
	chmod a+rx $*/$*${EXE}
	rm -f ${CGI_BIN}/$*
	mv $*/$*${EXE} ${CGI_BIN}/$*

