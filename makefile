
JKSRCDIR = ../../kent/src
JKLIBDIR = $(JKSRCDIR)/lib/$(MACHTYPE)

EXE = git-reports

.c.o:
	gcc -ggdb -Wimplicit -DDEBUG -Wall -I$(JKSRCDIR)/inc -I$(JKSRCDIR)/hg/inc -c $*.c

L = $(MYSQLLIBS) -lm
MYLIBS = $(JKLIBDIR)/jkhgap.a $(JKLIBDIR)/jkweb.a 


all: $(EXE)


O = $(EXE).o

$(EXE): $O 
	gcc $O $(MYLIBS) $L -o $(EXE)
	chmod a+rx $(EXE)
	#strip $(EXE)
	#cp $(EXE) $(HOME)/bin/$(MACHTYPE)
	

test: $(EXE)
	$(EXE) v223_branch v224_branch 2010-01-05 2010-01-19 v224 /cluster/bin/build/buildrepo/ /cluster/home/galt/public_html/git-reports

backup:
	date +%Y-%m-%d-%H-%M | gawk '{printf("zip -r $(EXE)%s.zip *\n",$$1);}' > tempX
	chmod 755 tempX
	./tempX
	rm tempX 
	scp *.zip screech:/scratch/backups/
	rm *.zip

clean:
	rm -f *.o $(EXE) *.tmp

