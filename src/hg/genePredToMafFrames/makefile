kentSrc = ../..
A = genePredToMafFrames
extraObjects = orgGenes.o mkMafFrames.o splitMultiMappings.o finishMafFrames.o
include $(kentSrc)/inc/userApp.mk

orgGenes.o: orgGenes.h orgGenes.c
mkMafFrames.o: mkMafFrames.c mkMafFrames.h orgGenes.h frameIncr.h
finishMafFrames.o: finishMafFrames.c finishMafFrames.h orgGenes.h frameIncr.h
genePredToMafFrames.o: genePredToMafFrames.c orgGenes.h mkMafFrames.h splitMultiMappings.h finishMafFrames.h
splitMultiMappings.o: splitMultiMappings.c splitMultiMappings.h orgGenes.h

savebak:
	savebak genePredToMafFrames  *.[ch] makefile tests/makefile tests/input
