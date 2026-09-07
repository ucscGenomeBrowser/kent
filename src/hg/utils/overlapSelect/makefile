kentSrc = ../../..
A = overlapSelect
# usage.h is made by stringify below, so it has to be named here; the compiler
# cannot report a header it has not been able to read yet.
extraHeaders = usage.h
extraObjects = rowReader.o chromAnn.o chromAnnMap.o selectTable.o coordCols.o
include ${kentSrc}/inc/userApp.mk

# create usage msg from text
usage.h: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} -static -var=usageMsg $< >> $@.tmp
	mv -f $@.tmp $@

clean::
	rm -f ${extraObjects} usage.h
