kentSrc = ../..
A = pslMap
include ${kentSrc}/inc/userApp.mk

pslMap.o: usage.msg

# create usage msg from text
usage.msg: usage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	sed -Ee 's/^.*$$/"&\\n"/' $< >> $@.tmp
	mv -f $@.tmp $@

clean::
	rm -f usage.msg
