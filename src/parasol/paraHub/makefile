include ../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

O = paraHub.o spoke.o heartbeat.o queue.o sockSuck.o

paraHub: $(O)
	${CC} ${COPT} ${CFLAGS} -ggdb -o ../bin/paraHub $O ../lib/$(MACHTYPE)/paralib.a ../../lib/$(MACHTYPE)/jkweb.a -lpthread

clean::
	rm -f ../bin/paraHub $O
