include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

A = hgGateway

O = $A.o

my: compile
	mv $A /usr/local/apache/cgi-bin-$(USER)/$A

alpha: compile
	mv $A /usr/local/apache/cgi-bin/$A

beta: compile
	mv $A /usr/local/apache/cgi-bin-beta/$A

compile: $O 
	${CC} $O $(MYLIBS) $(L) 
	mv a.out $A
	strip $A
	chmod a+rx $A

install:	compile
	mkdir -p ${DESTDIR}/usr/local/apache/cgi-bin
	mv $A ${DESTDIR}/usr/local/apache/cgi-bin/$A

debug: $O 
	${CC} $O $(MYLIBS) $(L) 
	mv a.out $A

lib:
	cd ../../lib; make

clean:
	rm -f $O $A

tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h
