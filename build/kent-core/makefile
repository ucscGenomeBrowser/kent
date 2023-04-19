all: bindir
	cd src && ${MAKE} utils USE_HAL=0 USE_FREETYPE=0 USE_HIC=0 BINDIR=$(CURDIR)/bin

test:
	cd src && ${MAKE} test
bindir:
	mkdir -p bin

clean:
	cd src && ${MAKE} clean
