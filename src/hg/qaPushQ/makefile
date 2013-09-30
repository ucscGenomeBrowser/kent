
include ../../inc/common.mk

#make COPT= alpha  
#(to disable gcc optimization which is messing up RL right now)

A = qaPushQ

L += $(MYSQLLIBS) -lm -lcrypt
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = $A.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}/$A

compile: $O pushQ.o
	${CC} $O pushQ.o ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A

pushQ.c pushQ.h: pushQ.as
	autoSql pushQ.as pushQ -dbLink

pushQ.o: pushQ.c pushQ.h 

formPushQ.h: formPushQ.html
	echo "char formQ[] =" > formPushQ.h
	sed -e 's/\"/\\\"/g' formPushQ.html | gawk '{ print "\"" $$0 "\\n\"" }' >> formPushQ.h
	echo ";" >> formPushQ.h
	

$O: pushQ.h formPushQ.h ../inc/versionInfo.h

backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(A)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

push:
	scp $(APACHE)/cgi-bin/$(A) qateam@hgwbeta:$(APACHE)/cgi-bin/

clean::
	rm -f formPushQ.h $(A)
	rm -f pushQ.o
