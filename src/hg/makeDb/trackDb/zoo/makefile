# Browser supports multiple trackDb's so that individual developers
# can change things rapidly without stepping on other people's toes. 
# Usually when updating it is best to update your own trackDb and
# test it to make sure it works and that you have cvs updated all
# of this dir before doing a make beta_*_update. Note that you
# must specify which trackDb you are using in your .hg.conf file
# or in the cgi-bin-$(USER)/hg.conf file. Something like: 
# db.trackDb=trackDb_YourUserName

#ZOO_DBS =   zooBaboon3 zooCat3 zooChicken3 zooChimp3 zooCow3 zooDog3 zooFugu3 \
            zooHuman3 zooMouse3 zooPig3 zooRat3 zooTetra3 zooZebrafish3 
ZOO_DBS =

update:
	@for DB in $(ZOO_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		cmd="hgTrackDb zoo $$DB trackDb_${USER} ../../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec zoo $$DB hgFindSpec_${USER} ../../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

alpha:
	@for DB in $(ZOO_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		cmd="hgTrackDb zoo $$DB trackDb ../../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec zoo $$DB hgFindSpec ../../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cp description.html /gbdb/zoo3/html;\
	    fi ; \
	done

beta: alpha

strict:
	@for DB in $(ZOO_DBS) x; do \
	    if test "$$DB" != "x" ; then \
		cmd="hgTrackDb -strict=yes zoo $$DB trackDb ../../../lib/trackDb.sql ." ; \
		echo $$cmd ;  $$cmd ; \
		cmd="hgFindSpec -strict zoo $$DB hgFindSpec ../../../lib/hgFindSpec.sql ." ; \
		echo $$cmd ;  $$cmd ; \
	    fi ; \
	done

