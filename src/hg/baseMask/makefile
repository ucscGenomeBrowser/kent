include ../../inc/common.mk

XINC = -I$(MYSQLINC)
L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = cachedMask.o trackToBaseMask.o bedToBaseMask.o baseMaskToBed.o baseMask.o
B = cachedMask trackToBaseMask bedToBaseMask baseMaskToBed baseMask

all: $B

cachedMask: ${MYLIBS} cachedMaskUsage.msg cachedMask.o 
	${CC} ${COPT} ${CFLAGS} ${XINC} -o cachedMask cachedMask.o ${MYLIBS} ${MYSQLLIBS} $L

trackToBaseMask: ${MYLIBS} trackToBaseMaskUsage.msg trackToBaseMask.o
	${CC} ${COPT} ${CFLAGS} -o trackToBaseMask trackToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

bedToBaseMask: ${MYLIBS} bedToBaseMaskUsage.msg bedToBaseMask.o
	${CC} ${COPT} ${CFLAGS} -o bedToBaseMask bedToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

baseMaskToBed: ${MYLIBS} baseMaskToBedUsage.msg baseMaskToBed.o
	${CC} ${COPT} ${CFLAGS} -o baseMaskToBed baseMaskToBed.o ${MYLIBS} ${MYSQLLIBS} $L

baseMask: ${MYLIBS} baseMaskUsage.msg baseMask.o
	${CC} ${COPT} ${CFLAGS} -o baseMask baseMask.o ${MYLIBS} ${MYSQLLIBS} $L


# create usage msg from text
cachedMaskUsage.msg: cachedMaskUsage.txt
	echo '/* GENERATED CODE, DO NOT EDIT */' > $@.tmp
	${STRINGIFY} $< >> $@.tmp
	mv -f $@.tmp $@

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
	rm -f cachedMaskUsage.msg baseMaskUsage.msg trackToBaseMaskUsage.msg bedToBaseMaskUsage.msg baseMaskToBedUsage.msg
	cd tests && ${MAKE} clean

