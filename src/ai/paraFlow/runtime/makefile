include ../../../inc/common.mk

O = argsEnv.o array.o msort.o cacheQueue.o dir.o fileLib.o getObj.o initVar.o \
    numbers.o  object.o paraRun.o pfString.o print.o runErr.o runType.o \
    winMac.o

testParaRun: testParaRun.o runtime.a
	${CC} ${COPT} ${CFLAGS} -o testParaRun testParaRun.o $O runtime.a ../../../lib/$(MACHTYPE)/jkweb.a -lpthread -lm

runtime.a: $(O) 
	ar rcus runtime.a $(O)

