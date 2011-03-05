include ../../inc/common.mk

O = aladdin.o 

aladdin.exe: $(O)
	${CC} ${COPT} ${CFLAGS} $O ../../lib/$(MACHTYPE)/jkweb.a ${L} -lm
	mv ${AOUT} aladdin.exe
	chmod a+rx aladdin.exe

push:	
	mv aladdin.exe ${CGI_BIN}-${USER}

clean::
	rm -f aladdin.exe
