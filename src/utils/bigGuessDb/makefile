kentSrc = ../..
include ../../inc/common.mk

compile:
	cp -p bigGuessDb ${DESTBINDIR}/bigGuessDb

lint:
	pylint bigGuessDb

clean:
	cd tests && ${MAKE} clean

test:
	cd tests && ${MAKE}

# big test, takes almost an hour, not doing during build
gbdbTest:
	for i in /gbdb/*; do db=`basename $$i`; if [ ! -f /gbdb/$$db/$$db.2bit ] ; then continue; fi; find /gbdb/$$db/ | egrep 'bb$$|bigWig$$|bw$$|bigBed$$' | while true; read fname; do guessedDb=`./bigGuessDb --best $$fname`; if [ "$$db" != "$$guessedDb" -a "$$guessedDb" != "emptyFile" ] ; then echo $$fname $$db $$guessedDb; fi ; done; done;
