kentSrc = ../..

include ../../inc/common.mk

#make COPT= alpha  
#(to disable gcc optimization which is messing up RL right now)

A = hgGeneRing

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lcrypt
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS = $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 

O = $A.o

include ../../inc/cgi_build_rules.mk

mgsc: compile
	mv $A ${CGI_BIN}/$A

compile: $O interaction.o
	${CC} $O interaction.o ${MYLIBS} ${L}
	mv ${AOUT} $A${EXE}
	chmod a+rx $A

$O: $A.c

interaction.c interaction.h: interaction.as
	autoSql interaction.as interaction -dbLink

interaction.o: interaction.c interaction.h 

backup: clean
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(A)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

push:
	scp $(APACHE)/cgi-bin/$(A) hgwbeta:$(APACHE)/cgi-bin/
