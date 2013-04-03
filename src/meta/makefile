
all:    
	cd metaFlatten && ${MAKE}
	cd metaHoist && ${MAKE}
	cd metaReformat && ${MAKE}

alpha: all

compile:
	cd metaFlatten && ${MAKE} compile
	cd metaReformat && ${MAKE} compile

test:
	cd metaFlatten && ${MAKE} test
	cd metaHoist && ${MAKE} test
	cd metaReformat && ${MAKE} test

clean::
	cd metaFlatten && ${MAKE} clean
	cd metaReformat && ${MAKE} clean
