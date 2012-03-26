include ../../inc/common.mk
# !!! NOTE we use gcc34 here to avoid a compiler compatibility issue with openss
#CC = gcc34

XINC = -I/usr/include
L += $(MYSQLLIBS) -lm -lcrypt -lssl
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = hgLogin

O = gbMembers.o $A.o

HG_WARN=${HG_WARN_ERR}

my: compile
	chmod a+rx $A${EXE}
	mv $A${EXE} ${CGI_BIN}-chinhli/$A

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

# gbMembers.c gbMembers.h: gbMembers.as
#        autoSql gbMembers.as gbMembers -dbLink

#init:
#        hgsql hgcentraltest < gbMembers.sql
#        hgsql hgcentraltest -e 'insert into gbMembers (userName,
#        realName,password,email,lastUse,activated,dateAuthenticated) values ("hgenecat","Fatcat Gene", "<hashed password here>","cat@xyz.edu", NOW(),"Y",NOW());'



tags: 
	ctags *.h *.c ../lib/*.c ../inc/*.h ../../lib/*.c ../../inc/*.h

clean: 
	rm -f $O 

