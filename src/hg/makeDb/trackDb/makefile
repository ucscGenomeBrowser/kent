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

DBS = 	hg13 hg15 hg16 hg17 \
	panTro1 \
	canFam1 \
	mm2 mm3 mm4 mm5\
	rn2 rn3 \
	bosTau1 \
	monDom1 \
	galGal2 \
	xenTro1 \
        tetNig1 \
	fr1 \
	cioSav1 \
	danRer1 danRer2 \
	dm1 dm2 \
	dp1 dp2 dp3 \
	droYak1 \
	droAna1 \
	droMoj1 \
	droVir1 \
	droEre1 \
	anoGam1 \
	apiMel1 \
	apiMel2	\
	ci1 \
	ce1 ce2 \
	cb1 \
	falciparum \
	sacCer1 \
	pyrAer1 \
	pyrFur2 \
	sc1 \
	vibCho1

MGC_DBS = \
	hg16 hg17 \
	mm3 mm4 mm5 \
	rn3 \
	danRer1	danRer2 \
	xenTro1

update:
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

alpha:
	cvs up -d -P .
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
		cmd="hgTrackDb $$vis $$org $$DB trackDb ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec $$org $$DB hgFindSpec ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		if test -f $$dbpath/description.html ; then \
		    cp $$dbpath/description.html /gbdb/$$DB/html ; \
		fi ; \
	    fi ; \
	done
	cmd="../../encode/encodeTrackDb.csh"; echo $$cmd; $$cmd

strict:
	cvs up -d -P .
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
		cmd="hgTrackDb -strict=yes $$vis $$org $$DB trackDb ../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec -strict $$org $$DB hgFindSpec ../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

# this will fail if we are not in a beta checkout:
checkbeta:
	cvs status trackDb.ra | egrep 'v[0-9]+_branch' > /dev/null

beta: checkbeta strict

# This target adds trackDbLocal to the mgc-specific server.  Also
# loads the grpMgc table.
mgc:
	@for DB in $(MGC_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		dbpath=`ls -1 -d */$$DB 2> /dev/null` ; \
		if test -n "$$dbpath" ; then \
		    org=`echo $$dbpath | sed -e 's/\/.*//'` ; \
		    vis="" ; \
		    cmd="hgTrackDb -strict=yes -raName=trackDbMgc.ra $$vis $$org $$DB trackDbLocal ../../lib/trackDb.sql ." ; \
		    echo $$cmd ;  $$cmd ; \
		    cmd="hgsql $$DB < grpMgc.sql" ; \
		    echo $$cmd ;  $$cmd ; \
		fi ; \
	    fi ; \
	done

