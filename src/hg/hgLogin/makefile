kentSrc = ../..
include ../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a ../../htslib/libhts.a ${MYSQLLIBS}

A = hgLogin

O = gbMembers.o $A.o

include ../../inc/cgi_build_rules.mk

compile: $O
	${CC} -o $A${EXE} -g $O ${MYLIBS}


# gbMembers.c gbMembers.h: gbMembers.as
#        autoSql gbMembers.as gbMembers -dbLink

#init:
#        hgsql hgcentraltest < gbMembers.sql
#        hgsql hgcentraltest -e 'insert into gbMembers (userName,
#        realName,password,email,lastUse,activated,dateAuthenticated) values ("hgenecat","Fatcat Gene", "<hashed password here>","cat@xyz.edu", NOW(),"Y",NOW());'

