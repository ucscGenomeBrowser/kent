.c.o:
	gcc -ggdb -Wimplicit -I../../inc -I../../../inc -c $*.c

L = $(MYSQLLIBS) -lm
BIN=$(HOME)/bin/$(MACHTYPE)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ldHgGene.o

all: ${BIN}/ldHgGene ${BIN}/sanger22-gff-doctor

${BIN}/ldHgGene: $O ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o ${BIN}/ldHgGene $O $(MYLIBS) $L

${BIN}/sanger22-gff-doctor: sanger22-gff-doctor
	cp -f $< $@
	chmod a+rx $@


lib:
	cd ../../../lib; make
