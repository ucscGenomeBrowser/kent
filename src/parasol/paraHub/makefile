include ../../inc/common.mk


O = paraHub.o spoke.o heartbeat.o queue.o sockSuck.o

paraHub: $(O)
	gcc -o ../bin/paraHub $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a -lpthread
	strip ../bin/paraHub

