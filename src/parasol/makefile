all: 
	cd lib; make; cd ..
	cd gensub2; make; cd ..
	cd para; make; cd ..
	cd paraHub; make; cd ..
	cd paraHubStop; make; cd ..
	cd paraNode; make; cd ..
	cd paraNodeStart; make; cd ..
	cd paraNodeStatus; make; cd ..
	cd paraNodeStop; make; cd ..
	cd paraTestJob; make; cd ..
	cd parasol; make; cd ..

install:
	cp bin/para* bin/gensub2 $(HOME)/bin/i386
	cp bin/para* bin/gensub2 /cluster/bin/i386

clean:
	rm */*.o

broad:
	cd lib; make; cd ..
	cd broadNode; make; cd ..
	cd broadHub; make; cd ..
