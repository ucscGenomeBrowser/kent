include ../../inc/common.mk

L = ${MYSQLLIBS} -lm 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgTracks

O = $A.o altGraphXTrack.o chainTrack.o contigTrack.o \
	coverageTrack.o cytoBandTrack.o \
        expRatioTracks.o goldTrack.o loweLabTracks.o \
	mafTrack.o netTrack.o pslTrack.o rmskTrack.o \
	sampleTracks.o wabaTrack.o wigTrack.o

my: compile
	mv $A ${CGI_BIN}-${USER}/$A

alpha: strip
	mv $A ${CGI_BIN}/$A

beta: strip
	mv $A ${CGI_BIN}-beta/$A

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

compile: $O 
	${CC} $O ${MYLIBS} ${L}
	mv a.out $A
	chmod a+rx $A

strip: compile
	strip $A

install: strip
	@if [ ! -d "${DESTDIR}${CGI_BIN}" ]; then \
		${MKDIR} "${DESTDIR}${CGI_BIN}"; \
	fi
	mv $A ${DESTDIR}${CGI_BIN}/$A

debug: $O
	${CC} $O ${MYLIBS} ${L} 
	mv a.out $A

lib:
	cd ../../lib; make

clean:
	rm -f $O $A

tags:  
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
