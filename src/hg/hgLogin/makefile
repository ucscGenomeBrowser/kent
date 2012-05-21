include ../../inc/common.mk

HG_WARN=-Wall

L += -lm ${MYSQLLIBS} 
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

A = hgLogin

O = gbMembers.o $A.o

include ../../inc/cgi_build_rules.mk

compile: $O
	${CC} -o $A${EXE} -g $O ${MYLIBS} ${L} 


# gbMembers.c gbMembers.h: gbMembers.as
#        autoSql gbMembers.as gbMembers -dbLink

#init:
#        hgsql hgcentraltest < gbMembers.sql
#        hgsql hgcentraltest -e 'insert into gbMembers (userName,
#        realName,password,email,lastUse,activated,dateAuthenticated) values ("hgenecat","Fatcat Gene", "<hashed password here>","cat@xyz.edu", NOW(),"Y",NOW());'

