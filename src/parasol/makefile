all: 
	cd lib && ${MAKE}
	cd gensub2 && ${MAKE}
	cd para && ${MAKE}
	cd paraHub && ${MAKE}
	cd paraHubStop && ${MAKE}
	cd paraNode && ${MAKE}
	cd paraNodeStart && ${MAKE}
	cd paraNodeStatus && ${MAKE}
	cd paraNodeStop && ${MAKE}
	cd paraTestJob && ${MAKE}
	cd parasol && ${MAKE}

install: all
	cp bin/para* bin/gensub2 ${BINDIR}
	cp bin/para* bin/gensub2 /cluster/bin/${MACHTYPE}

clean:
	rm -f */*.o lib/*/*.a
	rm -rf bin

broad:
	cd lib && ${MAKE}
	cd broadNode && ${MAKE}
	cd broadHub && ${MAKE}
