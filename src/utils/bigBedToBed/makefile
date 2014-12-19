kentSrc = ../..
A = bigBedToBed
include $(kentSrc)/inc/userApp.mk
L += -lm -lz ${SOCKETLIB}

# the following was mostly copied from ~/kent/src/hg/makeDb/hgLoadBed/makefile
cgi:: compile
	chmod a+rx $A${EXE}
	@if [ ! -d "${CGI_BIN}-${USER}/utils" ]; then \
		${MKDIR} ${CGI_BIN}-${USER}/utils; \
	fi
	rm -f ${CGI_BIN}-${USER}/utils/$A
	mv $A${EXE} ${CGI_BIN}-${USER}/utils/$A

alpha:: strip
	@if [ ! -d "${CGI_BIN}/utils" ]; then \
		${MKDIR} ${CGI_BIN}/utils; \
	fi
	rm -f ${CGI_BIN}/utils/$A
	mv $A${EXE} ${CGI_BIN}/utils/$A

beta:: strip
	@if [ ! -d "${CGI_BIN}-beta/utils" ]; then \
		${MKDIR} ${CGI_BIN}-beta/utils; \
	fi
	rm -f ${CGI_BIN}-beta/utils/$A
	mv $A${EXE} ${CGI_BIN}-beta/utils/$A

strip::  compile
	${STRIP} $A${EXE}
	chmod g+w $A${EXE}
	chmod a+rx $A${EXE}

