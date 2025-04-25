kentSrc = ../..
include ../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgapcgi.a ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgHubConnect

O = $A.o trackHubWizard.o

include ../../inc/cgi_build_rules.mk
 
compile: $O
	${CC} -o $A${EXE} -g $O ${MYLIBS} ${L}
ifeq (${IS_HGWDEV},yes)
	cd hooks && ${MAKE}
endif

alpha:: compile
ifeq (${IS_HGWDEV},yes)
	cd hooks && ${MAKE} alpha
endif

beta:: compile
ifeq (${IS_HGWDEV},yes)
	cd hooks && ${MAKE} beta
endif
