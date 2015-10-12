
all:    
	cd tagStormHoist && ${MAKE}
	cd tagStormReformat && ${MAKE}
	cd tagStormFromTab && ${MAKE}
	cd jsonToTagStorm && ${MAKE}
	cd tagStormInfo && ${MAKE}
	cd tagStormJoinTab && ${MAKE}
	cd tagStormToJson && ${MAKE}
	cd tagStormDeleteTags && ${MAKE}

alpha: all

compile:
	cd tagStormHoist && ${MAKE} compile
	cd tagStormReformat && ${MAKE} compile
	cd tagStormFromTab && ${MAKE} compile
	cd jsonToTagStorm && ${MAKE} compile
	cd tagStormInfo && ${MAKE} compile
	cd tagStormJoinTab && ${MAKE} compile
	cd tagStormToJson && ${MAKE} compile
	cd tagStormDeleteTags && ${MAKE} compile

test:
	cd tagStormHoist && ${MAKE} test
	cd tagStormReformat && ${MAKE} test
	cd tagStormFromTab && ${MAKE} test
	cd jsonToTagStorm && ${MAKE} test
	cd tagStormInfo && ${MAKE} test
	cd tagStormJoinTab && ${MAKE} test
	cd tagStormToJson && ${MAKE} test
	cd tagStormDeleteTags && ${MAKE} test


clean::
	cd tagStormHoist && ${MAKE} clean
	cd tagStormReformat && ${MAKE} clean
	cd tagStormFromTab && ${MAKE} clean
	cd tagStormInfo && ${MAKE} clean
	cd tagStormJoinTab && ${MAKE} clean
	cd tagStormToJson && ${MAKE} clean
	cd tagStormDeleteTags && ${MAKE} clean
