include ../../../inc/common.mk

O = argsEnv.o array.o cacheQueue.o dir.o fileLib.o getObj.o initVar.o \
    numbers.o  object.o paraRun.o pfString.o print.o runErr.o runType.o \
    winMac.o

runtime.a: $(O) 
	ar rcus runtime.a $(O)

