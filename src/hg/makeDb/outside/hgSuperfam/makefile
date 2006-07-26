include ../../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgSuperfam

O = $A.o

all: compile
	${STRIP} $A${EXE}
	mv $A ${HOME}/bin/$(MACHTYPE)/$A

compile: $O 
	${CC} ${COPT} ${CFLAGS} $O $(MYLIBS) $(L) 
	mv ${AOUT} $A${EXE}
	chmod a+rx $A

lib:
	cd ../../../../lib && ${MAKE}

tags: 
	ctags *.h *.c ../../lib/*.c ../../inc/*.h ../../../../lib/*.c ../../../inc/*.h

