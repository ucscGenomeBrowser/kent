
JKSRCDIR = ../../../../../../kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

EXE = mysqlEscape

.c.o:
	gcc -ggdb -Wimplicit -I$(MYSQLINC) -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBS = $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a 


all: $(EXE)


O = $(EXE).o

$(EXE): $O 
	gcc $O $(MYLIBS) $L -o $(EXE)
	strip $(EXE)
	chmod a+rx $(EXE)
	cp $(EXE) /cluster/home/$(USER)/bin/$(MACHTYPE)/

backup:
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(EXE)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f *.o
	rm -f $(EXE)

