# Make your private trackDb with 
#	make update
# Make it for genome-test with
#	make alpha

# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of trackDb/ before doing a make alpha. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

include ../../../inc/common.mk

DBS = 	hg13 hg15 hg16 hg17 hg18\
	panTro1 \
	rheMac1 rheMac2 \
	canFam1 canFam2 \
	mm2 mm3 mm4 mm5 mm6 mm7 mm8 \
	rn2 rn3 rn4 \
	oryCun1 \
	bosTau1 \
	bosTau2 \
	dasNov1 \
	loxAfr1 \
	echTel1 \
	monDom1 monDom2 monDom4 \
	galGal2 \
	xenTro1 \
	tetNig1 \
	fr1 \
	cioSav1 \
	danRer1 danRer2 danRer3\
	dm1 dm2 \
	dp1 dp2 dp3 \
	droYak1 droYak2 \
	droAna1 droAna2 \
	droMoj1 droMoj2 \
	droVir1 droVir2 \
	droEre1 \
	droSim1 \
	droGri1 \
	droPer1 \
	droSec1 \
	anoGam1 \
	apiMel1 \
	apiMel2	\
	triCas1 triCas2 \
	ci1 ci2 cioSav2 \
	strPur1 \
	ce1 ce2 ce3 \
	cb1 cb2 \
	caeRem1 \
	falciparum \
	sacCer1 \
	sc1 \
	canHg4 canHg9 car46 canHg10 canHg11 canHg12\

MGC_DBS = \
	hg16 hg17 hg18\
	mm5 mm6 \
	rn3 \
	danRer1	danRer2 \
	xenTro1

ARCH_DBS = \
	pyrAer1 aerPer1\
	sulSol1 sulfAcid1 sulfToko1 \
	archFulg1 \
	pyrFur2 pyrHor1 pyrAby1 therKoda1\
	halMar1 haloHalo1 \
	metMar1 methJann1 methTher1 methKand1\
	metAce1 methBark1 methMaze1 methBurt1 \
	therAcid1 therVolc1 ferrAcid1 picrTorr1 \
	nanEqu1 natrPhar1 methStad1 methHung1\
	vibrChol1\


update: #clean
	@for D in *; do \
            if [ -d "$$D" ]; then \
		if test "$$D" != "zoo" ; then \
		    if [ -e "$$D/makefile" ]; then \
			( cd $$D && echo $$D && $(MAKE) ) ;\
			x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
		    fi; \
                fi; \
            fi; \
	done
	@for DB in $(DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    if test -f $$dbpath/visibility.ra ; then \
			vis="-visibility=$$dbpath/visibility.ra" ; \
		    else if test -f $$org/visibility.ra ; then \
			vis="-visibility=$$org/visibility.ra" ; \
		    else \
			vis="" ; \
		fi ; fi ; fi; \
		cmd="hgTrackDb $$vis $$org $$DB trackDb_${USER} ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec $$org $$DB hgFindSpec_${USER} ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

alpha: clean
	${CVS} up -d -P .
	@for D in *; do \
            if [ -d "$$D" ]; then \
                if [ -e "$$D/makefile" ]; then \
                    ( cd $$D && echo $$D && $(MAKE) ) ;\
                    x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
                fi; \
            fi; \
	done
	@for DB in $(DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    if test -f $$dbpath/visibility.ra ; then \
			vis="-visibility=$$dbpath/visibility.ra" ; \
		    else if test -f $$org/visibility.ra ; then \
			vis="-visibility=$$org/visibility.ra" ; \
		    else \
			vis="" ; \
		fi ; fi ; fi; \
		cmd="hgTrackDb -release=alpha $$vis $$org $$DB trackDb ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec $$org $$DB hgFindSpec ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		if test -f $$dbpath/description.html ; then \
		    cp $$dbpath/description.html /gbdb/$$DB/html ; \
		fi ; \
	    fi ; \
	done
#cmd="../../encode/encodeTrackDb.csh ../../lib/trackDb.sql" ; echo $$cmd; $$cmd

strict: clean
	${CVS} up -d -P .
	@for D in *; do \
            if [ -d "$$D" ]; then \
                if [ -e "$$D/makefile" ]; then \
                    ( cd $$D && echo $$D && $(MAKE) ) ;\
                    x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
                fi; \
            fi; \
	done
	@for DB in $(DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    if test -f $$dbpath/visibility.ra ; then \
			vis="-visibility=$$dbpath/visibility.ra" ; \
		    else if test -f $$org/visibility.ra ; then \
			vis="-visibility=$$org/visibility.ra" ; \
		    else \
			vis="" ; \
		fi ; fi ; fi; \
		cmd="hgTrackDb -strict -release=beta $$vis $$org $$DB trackDb ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec -strict $$org $$DB hgFindSpec ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

archaea: clean
	${CVS} up -d -P .
	@for D in *; do \
            if [ -d "$$D" ]; then \
                if [ -e "$$D/makefile" ]; then \
                    ( cd $$D && echo $$D && $(MAKE) ) ;\
                    x=$$? ; if [ $$x -ne 0 ]; then exit $$x; fi \
                fi; \
            fi; \
	done
	@for DB in $(ARCH_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    if test -f $$dbpath/visibility.ra ; then \
			vis="-visibility=$$dbpath/visibility.ra" ; \
		    else if test -f $$org/visibility.ra ; then \
			vis="-visibility=$$org/visibility.ra" ; \
		    else \
			vis="" ; \
		fi ; fi ; fi; \
		cmd="hgTrackDb -strict -release=alpha $$vis $$org $$DB trackDb ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec -strict $$org $$DB hgFindSpec ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

# this will fail if we are not in a beta checkout:
checkbeta:
	${CVS} status trackDb.ra | egrep 'v[0-9]+_branch' > /dev/null

beta: checkbeta clean strict

# This target adds trackDbLocal to the mgc-specific server.  Also
# loads the grpMgc table.
mgc:
	ln -sf  human/hg17/exoniphy.html exoniphy.html
	ln -sf  human/hg17/exoniphyGene.html exoniphyGene.html
	ln -sf  mgcTwinscan.html mgcTwinscanEst.html
	ln -sf  mgcNScan.html mgcNScanEst.html
	@for DB in $(MGC_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    vis="" ; \
		    cmd="hgTrackDb -strict -release=beta -raName=trackDbMgc.ra $$vis $$org $$DB trackDbLocal ../../lib/trackDb.sql ." ; \
		    echo $$cmd ;  $$cmd ; \
		    cmd="hgsql $$DB < grpMgc.sql" ; \
		    echo $$cmd ;  $$cmd ; \
		fi ; \
	    fi ; \
	done

# Get rid of symbolic links (created by lower-level makefiles):
clean:
	find . -type l -exec rm {} \;

