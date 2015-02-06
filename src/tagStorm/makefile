
all:    
	cd tagStormHoist && ${MAKE}
	cd tagStormReformat && ${MAKE}

alpha: all

compile:
	cd tagStormHoist && ${MAKE} compile
	cd tagStormReformat && ${MAKE} compile

test:
	cd tagStormHoist && ${MAKE} test
	cd tagStormReformat && ${MAKE} test

clean::
	cd tagStormHoist && ${MAKE} clean
	cd tagStormReformat && ${MAKE} clean
