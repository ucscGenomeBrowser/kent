
# tests for DAS server.  Currently, the results change from day to day
# depending on the contents from the database.  So it only compares
# the results with the contents of the expected directory if the 
# output file actually exists.  This way, one can do something like:
#
#   make test sandbox=  # test with hgwdev
#   mv output expected
#   make test sandbox=-myname  # test with hgwdev-myname
#
# to compare results.
#
# requires the xmlproc_val command from the xmlproc package.
#

# to test timing, try sometime like:
#  (hgsql -e 'flush tables'; time  wget -nv --save-headers -O output/typesTest.das 'http://hgwdev-markd.cse.ucsc.edu/cgi-bin/das/hg18/types?segment=22')>&log


sandbox=-${USER}
dasUrl = http://hgwdev${sandbox}.cse.ucsc.edu/cgi-bin/das
testDb = hg18
testUrl = ${dasUrl}/${testDb}
testSeg = 22:16635715,16637714

test:	dsnTest \
	entryPtsTest \
	dnaTest \
	typesTest \
	featuresTest \
	featuresTypeTest \
	featuresTypeExactTest \
	featuresType2Test

dsnTest:
	@${MAKE} doTest id=$@ request='${dasUrl}/dsn'
entryPtsTest:
	@${MAKE} doTest id=$@ request='${testUrl}/entry_points'
dnaTest:
	@${MAKE} doTest id=$@ request='${testUrl}/dna?segment=${testSeg}'
typesTest:
	@${MAKE} doTest id=$@ request='${testUrl}/types?segment=${testSeg}'
featuresTest:
	@${MAKE} doTest id=$@ request='${testUrl}/features?segment=${testSeg}'
featuresTypeTest:
	@${MAKE} doTest id=$@ request='${testUrl}/features?segment=${testSeg}&type=transMapAlnMRna'

# "phastConsElements28way" use to incorrectly match "phastConsElements28wayPlacMammal"
# as well due use deprecated regular expressions
featuresTypeExactTest:
	@${MAKE} doTest id=$@ request='${testUrl}/features?segment=${testSeg}&type=phastConsElements28way'

# two types
featuresType2Test:
	@${MAKE} doTest id=$@ request='${testUrl}/features?segment=${testSeg}&type=transMapAlnMRna&type=transMapAlnRefSeq'

# recursive target, call with: id, request
# saves response headers and body, then splits them, dropping date for easy diffing
doTest:
	@mkdir -p output
	wget -nv --save-headers -O output/${id}.das '${request}'
	@awk '/^Date:/{next} {print $$0} /^$$/{exit}' output/${id}.das > output/${id}.hdrs
	@awk 'on{print $$0} /^$$/{on=1}' output/${id}.das > output/${id}.xml
	xmlproc_val output/${id}.xml
	@if [ -e expected/${id}.hdrs ] ; then diff expected/${id}.hdrs output/${id}.hdrs ; else echo "NOTE: no expected/${id}.hdrs" ; fi
	@if [ -e expected/${id}.xml ] ; then diff expected/${id}.xml output/${id}.xml ; else echo "NOTE: no expected/${id}.xml" ; fi

clean:
	rm -rf output
