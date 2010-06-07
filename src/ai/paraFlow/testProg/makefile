include ../../../inc/common.mk

	
test: q
	cd oneFile && ${MAKE}
	cd testLib && ${MAKE}
	cd easy && ${MAKE}
	rm -f modules/*/*.pfh
	cd modules && ${MAKE}
	cd modules && ${MAKE}
	cd fixes && ${MAKE}

q:
	cd quick && ${MAKE}
	
clean:
	rm -f */*.o */*/*.o
	rm -f */*.pfh */*/*.pfh
	rm -f */*.c */*/*.c
