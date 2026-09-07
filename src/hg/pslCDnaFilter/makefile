kentSrc = ../..
A = pslCDnaFilter
# usage.h and algo.h are made by stringify below, so they have to be named
# here; the compiler cannot report a header it has not been able to read yet.
extraHeaders = usage.h algo.h
extraObjects = cDnaAligns.o cDnaStats.o cDnaReader.o overlapFilter.o globalNearBestFilter.o localNearBestFilter.o polyASize.o hapRegions.o

include ${kentSrc}/inc/userApp.mk

# create msg from text
%.h: %.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} -static -var=usageMsg $< >> $@.tmp
	mv -f $@.tmp $@

clean::
	rm -f pslCDnaFilter usage.h algo.h
