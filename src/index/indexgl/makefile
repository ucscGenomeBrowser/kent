include ../../inc/common.mk


O = indexgl.o 

indexgl: ${O} 
	${CC} $O ../../lib/${MACHTYPE}/jkweb.a \
		-o ${HOME}/bin/${MACHTYPE}/indexgl
	strip ${HOME}/bin/${MACHTYPE}/indexgl

lib:
	cd ../../lib;make; cd ../index/indexgl

