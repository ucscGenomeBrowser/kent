include ../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkweb.a

A = tracks.exe

O = tracks.o 

my: compile
	mv $A /usr/local/apache/cgi-bin-$(USER)/$A

mgsc: compile
	mv $A /usr/local/apache/cgi-bin-mgsc/$A

alpha: compile
	mv $A /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A /usr/local/apache/cgi-bin-beta/$A


compile: $O 
	${CC} $O $(MYLIBS) $(L) 
	mv a.out $A
#strip $A
	chmod a+rx $A

install:	compile
	mkdir -p ${DESTDIR}/usr/local/apache/cgi-bin
	mv $A ${DESTDIR}/usr/local/apache/cgi-bin/$A

debug: $O
	${CC} $O $(MYLIBS) $(L) 
	mv a.out $A

clean:
	rm -f $O $A

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
