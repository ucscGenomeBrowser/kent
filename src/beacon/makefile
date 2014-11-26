include ../inc/common.mk

#cannot use common include rules, don't know how to overwrite :: targets
#include ../../inc/cgi_build_rules.mk

ifeq (${CGI_BIN_USER},)
    CGI_BIN_USER=${CGI_BIN}-${USER}
endif

ifeq (${CGI_BIN_BETA},)
    CGI_BIN_BETA=${CGI_BIN}-beta
endif

my::
	pylint -E query
	mkdir -p ${CGI_BIN_USER}/beacon
	rm -f ${CGI_BIN_USER}/beacon/query
	cp -p query ${CGI_BIN_USER}/beacon/
	cp -p accCounts.gdbm ${CGI_BIN_USER}/beacon/
	chmod a+w ${CGI_BIN_USER}/beacon/accCounts.gdbm
	cp -p insdcToUcsc.tab ${CGI_BIN_USER}/beacon/
	cp ${BINDIR}/bigBedToBed ${CGI_BIN_USER}/beacon/
	cp /hive/data/genomes/hg19/bed/beacon/*.bb ${CGI_BIN_USER}/beacon/
	cd ${CGI_BIN_USER}/beacon/ && ln -sf query queryHgmd && ln -sf query queryClinvar && ln -sf query queryLovd && ln -sf query queryUniprot


alpha::
	pylint -E query
	mkdir -p ${CGI_BIN}/beacon
	rm -f ${CGI_BIN}/beacon/query
	cp -p query ${CGI_BIN}/beacon/
	cp -p accCounts.gdbm ${CGI_BIN}/beacon/
	chmod a+w ${CGI_BIN}/beacon/accCounts.gdbm
	cp -p insdcToUcsc.tab ${CGI_BIN}/beacon/
	cp ${BINDIR}/bigBedToBed ${CGI_BIN}/beacon/
	cp /hive/data/genomes/hg19/bed/beacon/*.bb ${CGI_BIN}/beacon/
	cd ${CGI_BIN}/beacon/ && ln -sf query queryHgmd && ln -sf query queryClinvar && ln -sf query queryLovd && ln -sf query queryUniprot

beta::
	pylint -E query
	mkdir -p ${CGI_BIN_BETA}/beacon
	rm -f ${CGI_BIN_BETA}/beacon/query
	cp -p query ${CGI_BIN_BETA}/beacon/
	cp -p accCounts.gdbm ${CGI_BIN_BETA}/beacon/
	chmod a+w ${CGI_BIN_BETA}/beacon/accCounts.gdbm
	cp -p insdcToUcsc.tab ${CGI_BIN_BETA}/beacon/
	# for beta, using the stable binary from /cluster/bin
	cp /cluster/bin/x86_64/bigBedToBed ${CGI_BIN_BETA}/beacon/
	cp /hive/data/genomes/hg19/bed/beacon/*.bb ${CGI_BIN}/beacon/
	cd ${CGI_BIN_BETA}/beacon/ && ln -sf query queryHgmd && ln -sf query queryClinvar && ln -sf query queryLovd && ln -sf query queryUniprot

clean::
	rm -f 

compile::

