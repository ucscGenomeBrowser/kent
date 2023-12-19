kentSrc = ..
include ../inc/localEnvironment.mk
include ../inc/common.mk

#***********************************************************************
# File : makefile
#************************************************************************


CXX= g++
SRC= Tree.cc Leaf.cc readFile.cc SimMat.cc bestArr.cc
O= Tree.o Leaf.o readFile.o SimMat.o bestArr.o 
CXXFLAGS= -g 


#all: ${OBJ}
#	${CC} ${CFLAGS} ${OBJ} -lm -o arrange
#

../lib/$(MACHTYPE)/optimalLeaf.a: $(O) $(MACHTYPE)
	ar rcus ../lib/$(MACHTYPE)/optimalLeaf.a $(O)

$(MACHTYPE):
	mkdir $(MACHTYPE)

Leaf.o : Leaf.cc Leaf.hh 
	${CXX} -c  ${CXXFLAGS}  Leaf.cc

Tree.o : Tree.cc Tree.hh Leaf.hh
	${CXX} -c  ${CXXFLAGS}  Tree.cc

readFile.o : readFile.cc readFile.hh 
	${CXX} -c  ${CXXFLAGS}  readFile.cc

SimMat.o : SimMat.cc SimMat.hh readFile.hh 
	${CXX} -c  ${CXXFLAGS}  SimMat.cc

bestArr.o : bestArr.cc  Tree.hh SimMat.hh
	${CXX} -c  ${CXXFLAGS}  bestArr.cc

#***********************************************************************
#                         EOF
#**********************************************************************









# DO NOT DELETE
