kentSrc = ../..
A = pslCDnaFilter
extraHeaders = cDnaAligns.h cDnaStats.h cDnaReader.h overlapFilter.h globalNearBestFilter.h localNearBestFilter.h polyASize.h hapRegions.h \
	usage.h algo.h
extraObjects = cDnaAligns.o cDnaStats.o cDnaReader.o overlapFilter.o globalNearBestFilter.o localNearBestFilter.o polyASize.o hapRegions.o

include ${kentSrc}/inc/userApp.mk

# create msg from text
%.h: %.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} -static -var=usageMsg $< >> $@.tmp
	mv -f $@.tmp $@

clean::
	rm -f pslCDnaFilter usage.msg algo.msg
