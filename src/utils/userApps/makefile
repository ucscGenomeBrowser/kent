kentSrc = ../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

CMD_LIST = fetchChromSizes
DOC_LIST = 

all:: cmd_all doc_all

cmd_all: ${CMD_LIST:%_cmd_all}
%_cmp_all:
	@${MKDIR} "${DESTDIR}${BINDIR}"
	rm -f "${DESTDIR}${BINDIR}/$*"
	cp $* "${DESTDIR}${BINDIR}/"
	chmod +x "${DESTDIR}${BINDIR}/$*"

doc_all: ${DOC_LIST:%=%_doc_all}
%_doc_all:
	rm -f "${DESTDIR}${BINDIR}/$*"
	cp -p $* "${DESTDIR}${BINDIR}/"

clean::
	@echo "nothing to clean in userApps"

