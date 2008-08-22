include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = trackToBaseMask.o bedToBaseMask.o baseMaskToBed.o baseMask.o
B = trackToBaseMask   bedToBaseMask   baseMaskToBed   baseMask

all: $B

trackToBaseMask: trackToBaseMaskUsage.msg trackToBaseMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o trackToBaseMask trackToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

bedToBaseMask: bedToBaseMaskUsage.msg bedToBaseMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o bedToBaseMask bedToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

baseMaskToBed: baseMaskToBedUsage.msg baseMaskToBed.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o baseMaskToBed baseMaskToBed.o ${MYLIBS} ${MYSQLLIBS} $L

baseMask: baseMaskUsage.msg baseMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o baseMask baseMask.o ${MYLIBS} ${MYSQLLIBS} $L

*.o: *.c


# create usage msg from text
baseMaskUsage.msg: baseMaskUsage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

baseMaskToBedUsage.msg: baseMaskToBedUsage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

trackToBaseMaskUsage.msg: trackToBaseMaskUsage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

bedToBaseMaskUsage.msg: bedToBaseMaskUsage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

test:
	cd tests && ${MAKE} test

clean:
	rm -f $O $B
	rm -f baseMaskUsage.msg trackToBaseMaskUsage.msg bedToBaseMaskUsage.msg baseMaskToBedUsage.msg
	cd tests && ${MAKE} clean

