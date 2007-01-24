include ../../../inc/common.mk

XINC = -I/usr/include
L = $(MYSQLLIBS) -lm -lcrypt -lssl
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = gsidMember

O = $A.o members.o bio.o

#include ../../../inc/cgi_build_rules.mk

HG_WARN=${HG_WARN_ERR}

my: compile
	chmod a+rx $A${EXE}
	mv $A${EXE} ${CGI_BIN}-signup/$A

compile: $O $(MYLIBS)
	${CC} ${COPT} -o $A $O $(MYLIBS) $L

members.c members.h: members.as
	autoSql members.as members -dbLink

init:
	hgsql membership < members.sql
	hgsql membership < transactions.sql
	hgsql membership -e 'insert into members (email,password,activated) values ("hivcat","mIdhzf33z","Y")'
	

clean: 
	rm -f $O members.c members.h members.sql

