include ../../../inc/common.mk

test:
	cd oneFile && ${MAKE}
	cd easy && ${MAKE}
	rm -f modules/*/*.pfh
	cd modules && ${MAKE}
	cd modules && ${MAKE}
	cd fixes && ${MAKE}

clean:
	rm -f */*.o */*/*.o
	rm -f */*.pfh */*/*.pfh
	rm -f */*.c */*/*.c
