include ../../inc/common.mk


O = paraHub.o spoke.o heartbeat.o

paraHub: $(O)
	gcc -o $(HOME)/bin/$(MACHTYPE)/paraHub $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a
#	strip $(HOME)/bin/$(MACHTYPE)/paraHub

