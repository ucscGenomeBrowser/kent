include ../../../inc/common.mk

O = ../compiler/util.o argsEnv.o dir.o fileLib.o initVar.o object.o \
    print.o runType.o scan.o string.o

runtime.a: $(O) 
	ar rcus runtime.a $(O)

