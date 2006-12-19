include ../../../inc/common.mk
prefix=.
C_DEBUG=-g -O -w -W

C_INCL=-I

CFLAGS =   $(C_INCL) $(C_DEBUG)

everything : tfbsConsLoc


tfbsConsLoc : $(prefix)/tfbsConsLoc.c $(prefix)/tfbsConsLoc.h $(prefix)/matt-maf.c $(prefix)/matt-maf.h $(prefix)/tfbsConsLocUtil.c $(prefix)/tfbsConsLocUtil.h $(prefix)/seq.c $(prefix)/seq.h
	$(CC) $(prefix)/tfbsConsLoc.c $(prefix)/matt-maf.c $(prefix)/util.c $(prefix)/tfbsConsLocUtil.c $(prefix)/seq.c $(CFLAGS) -lm -o ${BINDIR}/tfbsConsLoc
