include ../inc/common.mk

O = idbQuery.o 

idbQuery.exe: $(O)
	gcc $O ../lib/$(MACHTYPE)/jkweb.a
	mv a.out idbQuery.exe
	chmod a+rx idbQuery.exe
	mv idbQuery.exe ${CGI_BIN}-${USER}
