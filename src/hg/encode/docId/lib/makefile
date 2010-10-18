include ../../../../inc/common.mk
XINC = -I$(MYSQLINC)

O = docIdSub.o

%.o: %.c
	${CC} ${COPT} ${CFLAGS} ${GBROWSE_D} ${LOWELAB_DEFS} ${HG_DEFS} ${HG_WARN} ${HG_INC} ${XINC} -o $@ -c $<

../../../../lib/$(MACHTYPE)/docId.a: $(O) 
	ar rcus ../../../../lib/$(MACHTYPE)/docId.a $(O)

clean:
	rm -f $(O);
	rm -f ../..../../lib/$(MACHTYPE)/docId.a;

