KENT_INC=-I../../../inc

straw: straw.o cStraw.o
	ld -r -o ../straw.o straw.o cStraw.o
straw.o: straw.cpp straw.h
	${CXX} straw.cpp ${KENT_INC} -g -c -std=c++14 -o straw.o
cStraw.o: cStraw.cpp straw.h
	${CXX} cStraw.cpp ${KENT_INC} -g -c -std=c++14 -o cStraw.o
clean:
	rm -f straw.o cStraw.o ../straw.o
