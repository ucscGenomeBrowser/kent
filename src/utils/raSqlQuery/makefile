kentSrc = ../..
A = raSqlQuery
preMyLibs += $(kentSrc)/lib/$(MACHTYPE)/jkhgap.a
extraObjects = raRecord.o
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a
include $(kentSrc)/inc/userApp.mk
L += $(MYSQLLIBS) -lm -lz
