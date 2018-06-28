kentSrc = ../../..
include ../../../inc/common.mk
# !!! NOTE we use gcc34 here to avoid a compiler compatibility issue with openss
#CC = gcc34

XINC = -I/usr/include
L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS} -lcrypt -lssl
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
A = gsidMember

O = paypalSignEncrypt.o bio.o members.o $A.o

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
	hgsql membership -e 'insert into members (email,password,activated) values ("hivcat","<hashed password here>","Y)"
	hgsql membership < invoices.sql

initSSL:
	openssl genrsa -out gsid_key.pem 1024
	openssl req -new -config openssl.cnf -key gsid_key.pem -x509 -days 3650 -out gsid_cert.pem
	(get paypal_cert.pem at 'https://www.paypal.com/us/cgi-bin/webscr?cmd=_profile-website-cert')
	(get sandbox_cert.pem at 'https://www.sandbox.paypal.com/us/cgi-bin/webscr?cmd=_profile-website-cert')
	cp *.pem /var/www/cgi-bin-signup/
	cp datemsk /var/www/cgi-bin-signup/

clean: 
	rm -f $O members.c members.h members.sql

