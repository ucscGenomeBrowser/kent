
all:    
	cd metaFlatten && ${MAKE}
	cd metaHoist && ${MAKE}
	cd metaReformat && ${MAKE}

alpha: all

test:
	cd metaFlatten && ${MAKE} test
	cd metaHoist && ${MAKE} test
	cd metaReformat && ${MAKE} test

clean:
	rm -f meta*/*.o

