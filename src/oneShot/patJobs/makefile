include ../../inc/common.mk


O = patJobs.o 

patJobs: $(O)
	${CC} ${COPT} ${CFLAGS} -o patJobs $O ../../lib/$(MACHTYPE)/jkweb.a ${L}

clean::
	rm -f ${O}
