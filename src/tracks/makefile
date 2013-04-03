include ../inc/common.mk

L += $(MYSQLLIBS) -lz -lm
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

A = tracksWS120.exe

O = tracks.o 

compile: $O 
	${CC} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A
#${STRIP} $A
	chmod a+rx $A

my: compile
	mv $A ${CGI_BIN}-$(USER)/$A

mgsc: compile
	mv $A ${CGI_BIN}-mgsc/$A

alpha: compile
	mv $A ${CGI_BIN}/$A

beta: compile
	mv $A ${CGI_BIN}-beta/$A


install:	compile
	${MKDIR} ${DESTDIR}${CGI_BIN}
	mv $A ${DESTDIR}${CGI_BIN}/$A

debug: $O
	${CC} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A

clean:
	rm -f $O $A

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
