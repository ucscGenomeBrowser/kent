
# ~galt/kent/src/hg/qaPushQ

include ../../inc/common.mk

JKSRCDIR = ../..
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

EXE = qaPushQ

#.c.o:
#	gcc -ggdb -Wimplicit -pedantic -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm -lcrypt
MYLIBS = $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a 
#$(JKLIBDIR)/jkOwnLib.a

all: $(EXE)

pushQ.c pushQ.h: pushQ.as
	autoSql pushQ.as pushQ -dbLink

pushQ.o: pushQ.c pushQ.h 

formPushQ.h: formPushQ.html
	echo "char formQ[] =" > formPushQ.h
	gawk '{ print "\"" $$0 "\\n\"" }' formPushQ.html >> formPushQ.h
	echo ";" >> formPushQ.h
	

$(EXE).o: formPushQ.h ../inc/versionInfo.h

O = $(EXE).o

$(EXE): pushQ.o $O 
	gcc $O pushQ.o $(MYLIBS) $L
	mv a.out $(EXE)
	strip $(EXE)
	chmod a+rx $(EXE)
	cp $(EXE) /usr/local/apache/cgi-bin

backup:
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(EXE)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

push:
	scp qaPushQ hgwbeta:$(APACHE)/cgi-bin/

clean:
	rm -f *.o
	rm -f formPushQ.h
	rm -f $(EXE)

