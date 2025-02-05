kentSrc = ../../..
A = overlapSelect
extraHeaders = rowReader.h chromAnn.h chromAnnMap.h selectTable.h coordCols.h \
	usage.h
extraObjects = rowReader.o chromAnn.o chromAnnMap.o selectTable.o coordCols.o
include ${kentSrc}/inc/userApp.mk

# create usage msg from text
usage.h: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} -static -var=usageMsg $< >> $@.tmp
	mv -f $@.tmp $@

clean::
	rm -f ${extraObjects} usage.h
