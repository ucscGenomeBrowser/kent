kentSrc = ..
include ../inc/localEnvironment.mk
include ../inc/common.mk

#***********************************************************************
# File : makefile
#************************************************************************


CC= g++
SRC= Tree.cc Leaf.cc readFile.cc SimMat.cc bestArr.cc
O= Tree.o Leaf.o readFile.o SimMat.o bestArr.o 
CFLAGS= -g 


#all: ${OBJ}
#	${CC} ${CFLAGS} ${OBJ} -lm -o arrange
#

../lib/$(MACHTYPE)/optimalLeaf.a: $(O) $(MACHTYPE)
	ar rcus ../lib/$(MACHTYPE)/optimalLeaf.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

Leaf.o : Leaf.cc Leaf.hh 
	${CC} -c  ${CFLAGS}  Leaf.cc

Tree.o : Tree.cc Tree.hh Leaf.hh
	${CC} -c  ${CFLAGS}  Tree.cc

readFile.o : readFile.cc readFile.hh 
	${CC} -c  ${CFLAGS}  readFile.cc

SimMat.o : SimMat.cc SimMat.hh readFile.hh 
	${CC} -c  ${CFLAGS}  SimMat.cc

bestArr.o : bestArr.cc  Tree.hh SimMat.hh
	${CC} -c  ${CFLAGS}  bestArr.cc

#***********************************************************************
#                         EOF
#**********************************************************************









# DO NOT DELETE
