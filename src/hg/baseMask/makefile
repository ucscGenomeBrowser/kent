include ../../inc/common.mk

XINC = -I$(MYSQLINC)
L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = commonBaseMask.o cachedMask.o trackToBaseMask.o bedToBaseMask.o baseMaskToBed.o baseMask.o
B = cachedMask trackToBaseMask bedToBaseMask baseMaskToBed baseMask

all: $B

cachedMask: cachedMaskUsage.msg commonBaseMask.o cachedMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} ${XINC} -o cachedMask commonBaseMask.o cachedMask.o ${MYLIBS} ${MYSQLLIBS} $L

trackToBaseMask: trackToBaseMaskUsage.msg commonBaseMask.o trackToBaseMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o trackToBaseMask commonBaseMask.o trackToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

bedToBaseMask: bedToBaseMaskUsage.msg bedToBaseMask.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o bedToBaseMask bedToBaseMask.o ${MYLIBS} ${MYSQLLIBS} $L

baseMaskToBed: baseMaskToBedUsage.msg baseMaskToBed.o ${MYLIBS} 
	${CC} ${COPT} ${CFLAGS} -o baseMaskToBed baseMaskToBed.o ${MYLIBS} ${MYSQLLIBS} $L

baseMask: baseMaskUsage.msg baseMask.o ${MYLIBS} 
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

