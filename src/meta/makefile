
all:    
	cd metaFlatten && ${MAKE}
	cd metaReformat && ${MAKE}

alpha: all

test:
	cd metaFlatten && ${MAKE} test
	cd metaReformat && ${MAKE} test

clean:
	rm meta*/*.o

