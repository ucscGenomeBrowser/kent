include ../../inc/common.mk


O = patJobs.o 

patJobs: $(O)
	${CC} ${COPT} ${CFLAGS} -o patJobs $O ../../lib/$(MACHTYPE)/jkweb.a

clean::
	rm -f ${O}
