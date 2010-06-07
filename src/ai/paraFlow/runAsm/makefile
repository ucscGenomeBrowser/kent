include ../../../inc/common.mk

O = asmLong.o asmString.o asmToOld.o

#testParaRun: testParaRun.o runtime.a
#	${CC} ${COPT} ${CFLAGS} -o testParaRun testParaRun.o $O runtime.a ../../../lib/$(MACHTYPE)/jkweb.a -lpthread -lm

runAsm.a: $(O) 
	ar rcus runAsm.a $(O)

