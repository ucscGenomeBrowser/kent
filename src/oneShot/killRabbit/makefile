include ../../inc/common.mk


L = $(MYSQLLIBS) -lm 

O = killRabbit.o

killRabbit: lib $O ../../lib/$(MACHTYPE)/jkweb.a
	gcc $O ../../lib/$(MACHTYPE)/jkweb.a $L
	mv a.out killRabbit
	${STRIP} killRabbit${EXE}

lib:
	cd ../../lib; make
