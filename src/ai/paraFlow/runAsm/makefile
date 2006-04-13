include ../../../inc/common.mk

O = asmString.o asmLong.o

#testParaRun: testParaRun.o runtime.a
#	${CC} ${COPT} ${CFLAGS} -o testParaRun testParaRun.o $O runtime.a ../../../lib/$(MACHTYPE)/jkweb.a -lpthread -lm

runAsm.a: $(O) 
	ar rcus runAsm.a $(O)

