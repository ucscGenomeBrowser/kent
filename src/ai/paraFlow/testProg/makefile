include ../../../inc/common.mk

test: quickTest
	cd oneFile && ${MAKE}
	cd easy && ${MAKE}
	rm -f modules/*/*.pfh
	cd modules && ${MAKE}
	cd modules && ${MAKE}
	cd fixes && ${MAKE}

quickTest:
	cd quick && ${MAKE}
	
clean:
	rm -f */*.o */*/*.o
	rm -f */*.pfh */*/*.pfh
	rm -f */*.c */*/*.c
