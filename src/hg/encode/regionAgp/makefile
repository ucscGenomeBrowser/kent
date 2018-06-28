kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a
INSTALLDIR = ${BINDIR}
ENCODE_SCRIPT_DIR = /cluster/data/encode/bin/scripts
SCRIPT_DIR = /cluster/bin/scripts

PROG = regionAgp

ENCODE_SCRIPTS = \
	encodeRegionPackingList.pl

SCRIPTS = \
	contigAccession.pl

all: ${PROG} scripts 

encode_scripts:
	@for F in $(ENCODE_SCRIPTS); do \
            I=$${F%.*}; \
            if test "$$F" -nt "$(ENCODE_SCRIPT_DIR)/$$I"; then \
                echo "installing $$I in ${ENCODE_SCRIPT_DIR}" ;\
                cp $$F $(ENCODE_SCRIPT_DIR)/$$I ;\
            fi ;\
	done

scripts:
	@for F in $(SCRIPTS); do \
            I=$${F%.*}; \
            if test "$$F" -nt "$(SCRIPT_DIR)/$$I"; then \
                echo "installing $$I in ${SCRIPT_DIR}" ;\
                cp $$F $(SCRIPT_DIR)/$$I ;\
                chmod 775 $(SCRIPT_DIR)/$$I ;\
            fi ;\
	done

O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(INSTALLDIR)/${PROG} $O $(MYLIBS) $L
#${STRIP} ${BINDIR}/${PROG}${EXE}

test: $(INSTALLDIR)/$(PROG)
	cd tests && ${MAKE} test

clean::
	rm -f ${O}
