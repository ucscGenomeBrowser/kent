kentSrc = ../..
include ../../inc/common.mk

compile:
	cp -p hubtools ${DESTBINDIR}/hubtools

lint:
	pylint hubtools

clean:
	cd tests && ${MAKE} clean

#test:
	#cd tests && ${MAKE}
