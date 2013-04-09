kentSrc = ../../../..
A = samHit
include ${kentSrc}/inc/userApp.mk

alpha: compile
	rm -f ${CGI_BIN}/$A
	mv $A${EXE} ${CGI_BIN}/$A

beta: compile
	rm -f ${CGI_BIN_BETA}/$A
	mv $A${EXE} ${CGI_BIN_BETA}/$A
