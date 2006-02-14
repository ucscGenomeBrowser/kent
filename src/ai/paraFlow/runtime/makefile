include ../../../inc/common.mk

O = ../compiler/util.o argsEnv.o array.o dir.o fileLib.o getObj.o initVar.o \
    numbers.o  object.o print.o runType.o string.o

runtime.a: $(O) 
	ar rcus runtime.a $(O)

