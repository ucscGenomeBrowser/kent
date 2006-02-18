include ../../../inc/common.mk

O = ../compiler/util.o argsEnv.o array.o dir.o fileLib.o getObj.o initVar.o \
    numbers.o  object.o pfString.o print.o runErr.o runType.o

runtime.a: $(O) 
	ar rcus runtime.a $(O)

