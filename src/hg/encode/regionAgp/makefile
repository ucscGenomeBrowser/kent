include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${HOME}/bin/${MACHTYPE}

SCRIPT_DIR = /cluster/data/encode/bin/scripts

SCRIPTS = \
	encodeRegionPackingList.pl

all:
	@for F in $(SCRIPTS); do \
            I=$${F%.*}; \
            if test "$$F" -nt "$(SCRIPT_DIR)/$$I"; then \
                echo "installing $$I in ${SCRIPT_DIR}" ;\
                cp $$F $(SCRIPT_DIR)/$$I ;\
                chmod 775 $(SCRIPT_DIR)/$$I ;\
            fi ;\
	done

PROG = regionAgp

O = ${PROG}.o

regionAgp: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/${PROG}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test
