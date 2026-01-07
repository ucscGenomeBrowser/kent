# If SERVERNAME is defined in the environment use it, else use api-test
SERVERNAME ?= https://hgwdev-${USER}.gi.ucsc.edu

test:: all

help::
	@printf "makefile targets:\n\
test   -  using your sandbox hgwdev-${USER}.gi.ucsc.edu to run all tests\n\
hgwdev -  using a unique vhost on hgwdev: api-test.gi.ucsc.edu\n\
alpha  -  using genome-test.gi.ucsc.edu\n\
beta   -  using apibeta-soe.ucsc.edu - the hgwbeta machine, not all tests\n\
       -  may work due to different sets of tracks and databases there.\n\
anyTestTarget - there are many test targets\n\
\n\
To run a specific test for a specific server, for example:\n\
\n\
  SERVERNAME=\"https://genome-test.gi.ucsc.edu\" make test8\n\
\n\
Note:  Your sandbox trackDb needs to be up to date to allow the tests\n\
       to function correctly.  If one fails, update that trackDb.\n"

hgwdev::
	SERVERNAME="https://api-test.gi.ucsc.edu" make test

alpha::
	SERVERNAME="https://genome-test.gi.ucsc.edu" make test

beta::
	SERVERNAME="https://apibeta.soe.ucsc.edu" make test

all:: test0 listFunctions getFunctions listSchema getSequence wigData \
	search supportedTypes errorTests notSupported bugReports

listFunctions: list01 list02 list04 list05 list06 list07 list08 \
	list10 list11 list12 list13 list14 list15 list16 list17 list18 \
	list20 list21 list23 list24 list25 list27 list28 list29 \
	list30 list31 list32

getFunctions: test8 test9 test10 test11 test12 test13 test14 \
	test15 test16 test19 test21 test26 test27 test28 \
	test29 test39 test40 test42 test43 test44 test45 test46 test47 \
	test48 test49 test50 test51 test52 test53 test54 test55 test56 test57 \
	test58 test59 multiTrack01

findGenome: findGenome01

listSchema: schema01 schema02 schema03 schema04 schema05 schema06 schema07 \
	schema08 schema09 schema10 schema11 schema12

getSequence: getSeq01 getSeq02 getSeq03 getSeq04 getSeq05 getSeq06 getSeq07

wigData: wig01 wig02 wig03 wig04 wig05 wig06 wig07 wig08 wig09 wig10 \
	wig11 wig12 wig13 wig14 wig15 wig16 wig17 wig18 wig19 wig20 \
	wig21 wig22 wig23 wig24

search: search01

chrAlias: chrAlias01 chrAlias02 chrAlias03 chrAlias04 chrAlias05 chrAlias06 \
	chrAlias07 chrAlias08 chrAlias09 chrAlias10 chrAlias11

supportedTypes: altGraphX barChart chain ctgPos expRatio \
	interact netAlign peptideMapping pgSnp bigDbSnp bigMaf bigChain

supportedTypes0: altGraphX barChart chain ctgPos expRatio factorSource gvf \
	interact netAlign peptideMapping pgSnp

errorTests: err01 err02 err03 err04 err05 err06 err07 err08 err09 err10 \
	err11 err12 err13 err14 err15 err16 err17 err18 err19 err20 \
	err21 err22 err23 err24 err25 err26 err27 err28 err29 err30 \
	err31 err32 err33 err34 err35 err36 err37 err38 err39 err40 \
	err41 err42 err43 err44 err45 err46 err47 err48 err49 err50 \
	err51 err52 err53

notSupported: notSup01 notSup02 notSup03 notSup07 notSup10

bugReports: redmine24089a redmine24089b redmine24666 redmine25840

setOutput:
	@if [ ! -d testOutput ]; then mkdir testOutput; fi

clean::
	rm -f testOutput/*.gz
	rmdir testOutput

# exclude these lines from the JSON output for comparison with expected
excludeLines = downloadTime|dataTime|priority

# testing /list/noSubCommand, /list/publicHubs and /list/ucscGenomes
test0: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -test0 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz
	@echo '### test0: ./jsonConsumer.pl -serverName="${SERVERNAME}" -test0 - OK'

################################################### list functions ###########
# testing /list/hubGenomes
list01: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n" "${SERVERNAME}"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/hubGenomes" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? hubUrl genome=araTha1 trackLeavesOnly
list02: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/tracks?trackLeavesOnly=1;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="araTha1" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11 trackLeavesOnly=1
# the grep " : {" will pick out just the track names to compare, so that
# the changing genbank tables do not mess up the comparison, this will only
# be comparing the track listing.  The other egrep -v removes table names that
# only exist on hgwdev so this test will work on public site
#list03: setOutput
#	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/tracks?trackLeavesOnly=1;genome=ce11'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -genome="ce11" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | grep " : {" | egrep -v "chainStrRat1|microsat|netStrRat1|tandemDups" | sort | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11
list04: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce11" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=gold
list05: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=gold;genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce11" -track="gold" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl genome=araTha1
list06: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl genome=araTha1 track=assembly
list07: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=assembly;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? hubUrl genome=araTha1
list08: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/tracks?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="araTha1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11
# the grep " : {" will pick out just the track names to compare, so that
# the changing genbank tables do not mess up the comparison, this will only
# be comparing the track listing
#list09: setOutput
#	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/tracks?genome=ce11'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -genome="ce11" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | grep " : {" | egrep -v "chainStrRat1|netStrRat1|tandemDups|microsat|tanDups" | sort | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=mrna SQL table name is all_mrna
list10: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=mrna;genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce11" -track="mrna" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=mrna SQL table name is all_mrna
# and ce4 has split tables
list11: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=mrna;genome=ce4'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce4" -track="mrna" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=rmsk different chrom name column
list12: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=rmsk;genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce11" -track="rmsk" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=rmsk different chrom name column
# and ce4 has split tables
list13: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=rmsk;genome=ce4'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce4" -track="rmsk" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=est different chrom name column
list14: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=est;genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce11" -track="est" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=est different chrom name column
# and ce4 has split tables
list15: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/list/chromosomes?track=est;genome=ce4'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="ce4" -track="est" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list chromosomes for a track that only has a bigDataUrl specification,
#    no database table
# testing /list/chromosomes?genome=equCab3&track=transMapEnsemblV5
list16: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=transMapEnsemblV5;genome=equCab3'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="equCab3" -track="transMapEnsemblV5" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list chromosomes for a bigWig track=gc5BaseBw genome=rheMac10
list17: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=gc5BaseBw;genome=rheMac10'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="rheMac10" -track="gc5BaseBw" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list chromosomes for a wigMaf table track=multiz7way genome=hg38
list18: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=multiz7way;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="multiz7way" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list chromosomes for a 'curated' assembly track=assembly genome=hs1
#list19: setOutput
#	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=assembly;genome=hs1'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hs1" -track="assembly" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#; s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? for a 'curated' assembly genome=mpxvRivers trackLeavesOnly
list20: setOutput
	@printf "### $@ '${SERVERNAME}/list/tracks?trackLeavesOnly=1;genome=mpxvRivers'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -genome="mpxvRivers" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#; s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=hs1 - a 'curated' assembly
list21: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?genome=hs1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hs1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=ce2
#list22: setOutput
#	@printf "### $@ '${SERVERNAME}/list/files?genome=ce2'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="ce2" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=GCA_021951015.1
list23: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=GCA_021951015.1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="GCA_021951015.1" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=ce2;maxItemsOutput=5
list24: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=ce2;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -maxItemsOutput=5  -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="ce2" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=ce2;maxItemsOutput=5;format=text
list25: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=ce2;maxItemsOutput=5;format=text'\n"
	@ curl -L "${SERVERNAME}/list/files?genome=ce2;maxItemsOutput=5;format=text" 2> /dev/null | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/genarkGenomes
#list26: setOutput
#	@printf "### $@ '${SERVERNAME}/list/genarkGenomes?maxItemsOutput=5'\n"
#	@./jsonConsumer.pl -maxItemsOutput=5  -serverName="${SERVERNAME}" -endpoint="/list/genarkGenomes" 2>&1 | egrep -v "${excludeLines}|totalAssemblies" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	#@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/genarkGenomes verify existence of genome GCA_000002765.3
list27: setOutput
	@printf "### $@ '${SERVERNAME}/list/genarkGenomes?genome=GCA_000002765.3;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -genome=GCA_000002765.3 -maxItemsOutput=5  -serverName="${SERVERNAME}" -endpoint="/list/genarkGenomes" 2>&1 | egrep -v "${excludeLines}|totalAssemblies" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=GCA_001698545.1 without hubUrl
list28: setOutput
	@printf "### $@ '${SERVERNAME}/list/tracks?genome=GCA_001698545.1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -genome="GCA_001698545.1" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;'  | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=GCA_001698545.1 with hubUrl
list29: setOutput
	@printf "### $@ '${SERVERNAME}/list/tracks?genome=GCA_001698545.1;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCA/001/698/545/GCA_001698545.1/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -genome="GCA_001698545.1" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCA/001/698/545/GCA_001698545.1/hub.txt" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;'  | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=GCA_021951015.1 without hubUrl
list30: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?genome=GCA_021951015.1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="GCA_021951015.1" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;'  | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=GCA_021951015.1 with hubUrl
list31: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?genome=GCA_021951015.1;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCA/021/951/015/GCA_021951015.1/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="GCA_021951015.1" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCA/021/951/015/GCA_021951015.1/hub.txt" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;'  | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/schema?genome=GCA_021951015.1;track=cpgIslandExt without hubUrl
list32: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?genome=GCA_021951015.1;track=cpgIslandExt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="GCA_021951015.1" -track="cpgIslandExt" 2>&1 | egrep -v "${excludeLines}" | egrep -v "dateTime" | sed -e 's#https://.*/list#/list#;'  | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

##########################   getData functions  #############################
# testing /getData/track?genome=ce11&track=gold
test8: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce11" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=chrM
test9: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=chrI&start=1&end=1024
test10: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="chrI" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold assembly hub
test11: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrM assembly hub
test12: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrCp" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrI&start=1&end=1024 assembly hub
test13: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrCp" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved  track hub
test14: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="hub_9604_ultraConserved" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1  track hub
test15: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="hub_9604_ultraConserved" -chrom="chr1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1&start=N&end=M  track hub
test16: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="hub_9604_ultraConserved" -chrom="chr1" -start=10600000 -end=10800000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# bigBed data from a database
# /getData/track? genome=galGal6 &track=ncbiRefSeqOther&chrom=chr1&start=750000&end=5700000
test19: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="galGal6" -track="ncbiRefSeqOther" -chrom="chr1" -start=750000 -end=55700000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a database where the SQL table name is different than the
#    track name
# /getData/track? genome=hg19 &track=wgEncodeRegMarkH3k4me1H1hesc&chrom=chr1&start=62300000&end=62301000
test21: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="wgEncodeRegMarkH3k4me1H1hesc" -chrom="chr1" -start=62300000 -end=62301000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing genePred track
#    /getData/track?genome=ce11&chrom=chrV&track=augustusGene&start=5231045&end=5241045
test26: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="augustusGene"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing PSL track
#    /getData/track?genome=ce11&chrom=chrV&track=ncbiRefSeqPsl&start=5231045&end=5241045
test27: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="ncbiRefSeqPsl"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing RMSK track
#    /getData/track?genome=ce11&debug=0&chrom=chrV&track=rmsk&start=5231045&end=5241045
test28: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="rmsk"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# test superTrack child getData
#    /getData/track?genome=ce11&chrom=chrV&track=cpgIslandExt&start=5001045&end=5241045
test29: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="cpgIslandExt"  -chrom="chrV" -start=5001045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce4&track=gold for split table operation
test39: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce4" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce4&track=gold for split table operation
# plus jsonArrayOutput
test40: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -jsonOutputArrays -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce4" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# bigDataUrl for bigGenePred track with no table, only trackDb
#test41: setOutput
#	@printf "### $@ ### '${SERVERNAME}/track?track=mane;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=mane -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# get data from wigMaf database table
test42: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=multiz7way;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=multiz7way -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from wigMaf database table with chrom specified
test43: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=multiz7way;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr22'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=multiz7way -maxItemsOutput=5 -jsonOutputArrays -chrom=chr22 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from wigMaf database table with chrom and start,end specified
test44: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=multiz7way;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr21;start=20000000;end=30000000'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=multiz7way -maxItemsOutput=5 -jsonOutputArrays -chrom=chr21 -start=20000000 -end=30000000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigLolly track
test45: setOutput
	@printf "### $@ ### '${SERVERNAME}/track?track=BLCA;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=BLCA -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigLolly track with chrom specified
test46: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=BLCA;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr22'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=BLCA -maxItemsOutput=5 -jsonOutputArrays -chrom=chr22 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigLolly track with chrom and start,end specified
test47: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=BLCA;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr21;start=20000000;end=30000000'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=BLCA -maxItemsOutput=5 -jsonOutputArrays -chrom=chr21 -start=20000000 -end=30000000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigBarChart track
test48: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=gtexTranscExpr;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gtexTranscExpr -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigBarChart track with chrom specified
test49: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=gtexTranscExpr;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr22'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gtexTranscExpr -maxItemsOutput=5 -jsonOutputArrays -chrom=chr22 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigBarChart track with chrom and start,end specified
test50: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=gtexTranscExpr;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr21;start=20000000;end=30000000'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gtexTranscExpr -maxItemsOutput=5 -jsonOutputArrays -chrom=chr21 -start=20000000 -end=30000000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigInteract track
test51: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=geneHancerInteractions;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=geneHancerInteractions -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigInteract track with chrom specified
test52: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=geneHancerInteractions;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr22'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=geneHancerInteractions -maxItemsOutput=5 -jsonOutputArrays -chrom=chr22 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from bigInteract track with chrom and start,end specified
test53: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=geneHancerInteractions;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr21;start=20000000;end=30000000'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=geneHancerInteractions -maxItemsOutput=5 -jsonOutputArrays -chrom=chr21 -start=20000000 -end=30000000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from clonePos track
test54: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=clonePos;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=clonePos -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from clonePos track with chrom specified
test55: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=clonePos;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr22'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=clonePos -maxItemsOutput=5 -jsonOutputArrays -chrom=chr22 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# get data from clonePos track with chrom and start,end specified
test56: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=clonePos;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5;chrom=chr21;start=20000000;end=30000000'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=clonePos -maxItemsOutput=5 -jsonOutputArrays -chrom=chr21 -start=20000000 -end=30000000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?genome=ce11;track=mrna;maxItemsOutput=5

# PSL track, no chrom specified, exercise column name tName instead of chrom
test57: setOutput
	@printf "### $@ ### '${SERVERNAME}/getData/track?track=mrna;genome=ce11;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=mrna -maxItemsOutput=5 -jsonOutputArrays | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=GCF_000002985.6&track=assembly
# without the hubUrl
# https://hgdownload.soe.ucsc.edu/hubs/GCF/000/002/985/GCF_000002985.6/
test58: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=123 -endpoint="/getData/track" -genome="GCF_000002985.6" -track="assembly" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=GCF_000002985.6&track=assembly
# with the hubUrl
# https://hgdownload.soe.ucsc.edu/hubs/GCF/000/002/985/GCF_000002985.6/
test59: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=123 -endpoint="/getData/track" -genome="GCF_000002985.6" -track="assembly" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCF/000/002/985/GCF_000002985.6//hub.txt" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# getData but with multiple tracks
multiTrack01:
	@printf "### $@ ### '${SERVERNAME}/getData/track?tracks=gold,gap;genome=hg38;jsonOutputArrays=1;chrom=chr2;start=16129261;end=16163278'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=gold,gap -jsonOutputArrays -chrom=chr2 -start=16129261 -end=16163278 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

###           but the name in the track is CP068276.2
###  can not decide what name to use to fetch data
# getData but with multiple tracks from a 'curated' hub
#multiTrack02:
#	@printf "### $@ ### '${SERVERNAME}/getData/track?tracks=assembly,cpgIslandExtUnmasked,simpleRepeat;genome=hs1;jsonOutputArrays=1;chrom=chr2;start=16129261;end=16163278'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hs1 -track=assembly,cpgIslandExtUnmasked,simpleRepeat -jsonOutputArrays -chrom=chr2 -start=16129261 -end=16163278 | egrep -v "${excludeLines}" | sed -e 's#hubs/.*/hub.txt#hubs/hub.txt#;' | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

#multiTrack03:
#	@printf "### $@ ### '${SERVERNAME}/getData/track?tracks=assembly,cpgIslandExtUnmasked,simpleRepeat;genome=hs1;jsonOutputArrays=1;chrom=CP068276.2;start=16129261;end=16163278'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hs1 -track=assembly,cpgIslandExtUnmasked,simpleRepeat -jsonOutputArrays -chrom=CP068276.2 -start=16129261 -end=16163278 | egrep -v "${excludeLines}" | sed -e 's#hubs/.*/hub.txt#hubs/hub.txt#;' | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz


###############################################################################
#  /getData/sequence testing
###############################################################################
# testing /getData/sequence? hubUrl genome=hg19&chrom=chrCp&start=4321&end=5647
getSeq01: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -chrom="chrCp" -start=4321 -end=5678 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? hubUrl Bejerano genome=hg19&chrom=chrM&start=4321&end=5647
getSeq02: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -chrom="chrM" -start=4321 -end=5678 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? genome=monDom5&chrom=chr1&start=4321&end=5647
getSeq03: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="monDom5" -chrom="chr1" -start=748055061 -end=748055161 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?genome=ce11&chrom=chrM
getSeq04: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="ce11" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?genome=ce11&chrom=chrM&start=1&end=128
getSeq05: setOutput
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="ce11" -chrom="chrM" -start=1 -end=128 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? hubUrl genome=hg19&chrom=chrCp&start=4321&end=5647
getSeq06: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -chrom="chrCp" -start=4321 -end=5678 -revComp=1 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? genome=monDom5;chrom=chr1;start=4321;end=5647;revComp=1
getSeq07: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="monDom5" -chrom="chr1" -start=748055061 -end=748055161 -revComp=1 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

###############################################################################
#  /findGenome testing
###############################################################################
# simple findGenome test
findGenome01: setOutput
	@printf "### $@ ### '${SERVERNAME}/cgi-bin/hubApi/findGenome?genomeSearch=hg38'\n"
	@curl -L "${SERVERNAME}/cgi-bin/hubApi/findGenome?genomeSearch=hg38" 2> /dev/null | python -mjson.tool | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

#	@ curl -L "${SERVERNAME}/search?search=brca;genome=hg38" 2> /dev/null | python -mjson.tool | egrep -c -n "\[|\]" | awk -v t="$@" '{if (($$1 < 30) || ($$1 > 50)) {printf "test %s failed, count %d not in range 30-40\n", t, $$1; exit 255;}}'
# | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz

###############################################################################
#  /list/schema testing
###############################################################################
# testing /list/schema? genome=monDom5&track=gold
schema01: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=gold;genome=monDom5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="monDom5" -track=gold | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/schema? genome=hg19&track=ctgPos
schema02: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=ctgPos;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg19" -track=ctgPos | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

schema03: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=gold;genome=ce2'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="ce2" -track=gold | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

schema04: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=assembly;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track=assembly | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# can list schema for tables without trackDb
schema05: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=chromAlias;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track=chromAlias | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for bigBed file via table fileName reference
schema06: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=crispr10KTargets;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track=crispr10KTargets | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for bigWig file with table and bigWig file
schema07: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=gc5BaseBw;genome=ce11'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="ce11" -track=gc5BaseBw | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for wigMaf table type
schema08: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=multiz7way;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track=multiz7way | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for bigLolly table type
schema09: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=BLCA;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track=BLCA | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for bigBarChart table type
schema10: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=gtexTranscExpr;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg19" -track=gtexTranscExpr | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for bigInteract table type
schema11: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=geneHancerInteractions;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg19" -track=geneHancerInteractions | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# list schema for clonePos table type
schema12: setOutput
	@printf "### $@ '${SERVERNAME}/list/schema?track=clonePos;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg19" -track=clonePos | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

###############################################################################
#  ERROR testing
###############################################################################
# generate an error: ask for non-existent chrom in a track hub
# /getData/track? hubUrl Plants &track=assembly&chrom=chrI&start=1&end=14309681
err01: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?track=assembly;chrom=chrI;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=14309681;start=1\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrI" -start=1 -end=14309681 2>&1 | sed -e 's#https://.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not open bigWig file
err02: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?track=gc5Base;chrom=chrA9;genome=braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome="braRap1" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not find hubUrl
err03: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?track=gc5Base;chrom=chrA9;genome=braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt" -genome="braRap1" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /getData/track  with hubUrl
err04: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?track=gc5Base;chrom=chrA9;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /getData/sequence  with hubUrl
err05: setOutput
	@printf "### $@ ${SERVERNAME}/getData/sequence?chrom=chrA9;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /list/tracks  with hubUrl
err06: setOutput
	@printf "### $@ ${SERVERNAME}/list/tracks?trackLeavesOnly=1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument hubUrl for /list/tracks
err07: setOutput
	@printf "### $@ ${SERVERNAME}/list/tracks?trackLeavesOnly=1;genome=araTha1\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -genome="araTha1" -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument hubUrl and genome for /list/tracks
err08: setOutput
	@printf "### $@ ${SERVERNAME}/list/tracks?trackLeavesOnly=1\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?genome=cxe6 - error test illegal database name
# plus jsonArrayOutput
err09: setOutput
	@printf "### $@ ${SERVERNAME}/getData/sequence?genome=cxe6;jsonOutputArrays=1;maxItemsOutput=123\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -jsonOutputArrays -maxItemsOutput=123 -endpoint="/getData/sequence" -genome="cxe6" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=cxe6 - error test illegal database name
# plus jsonArrayOutput
err10: setOutput
	@printf "### $@ ${SERVERNAME}/list/tracks?genome=cxe6;jsonOutputArrays=1;maxItemsOutput=123\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -jsonOutputArrays -maxItemsOutput=123 -endpoint="/list/tracks" -genome="cxe6" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err11: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?genome=ce6;track=gold;maxItemsOutput=1x23\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=1x23 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err12: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?genome=ce6;track=gold;maxItemsOutput=-2\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=-2 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err13: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?genome=ce6;track=gold;maxItemsOutput=0\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=0 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err14: setOutput
	@printf "### $@ ${SERVERNAME}/getData/track?genome=ce6;track=gold;maxItemsOutput=0\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -maxItemsOutput=0 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/tracks?genome=ce6 - error test no such function on endpoint
err15: setOutput
	@printf "### $@ ${SERVERNAME}/getData/tracks?genome=ce6;track=gold\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/tracks" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/endpoints - error test no such function
err16: setOutput
	@printf "### $@ ${SERVERNAME}/list/endpoints\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/endpoints" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/hubGenomes with hubUrl but no genome specified
err17: setOutput
	@printf "### $@ '${SERVERNAME}/list/hubGenomes?genome=araTha1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl missing genome=
err18: setOutput
	@printf "### $@ ${SERVERNAME}/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl missing incorrect genome=araTha
err19: setOutput
	@printf "### $@ ${SERVERNAME}/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome=araTha 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl can not
#  find specified track for genome=braRap1 track=assembly (wrong name)
err20: setOutput
	@printf "### $@ ${SERVERNAME}/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt;genome=braRap1;track=assemblx\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome=braRap1 -track=assemblx 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl can not
#  find specified database in track hub
err21: setOutput
	@printf "### $@ '${SERVERNAME}/chromosomes?genome=hg18;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome=hg18 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? bad hubUrl
err22: setOutput
	@printf "### $@ hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/xGillBejerano/hub.txt\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/xGillBejerano/hub.txt" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? genome=ce11 track=gbLoaded - not a position track
err23: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?genome=ce11;track=gbLoaded'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome=ce11 -track=gbLoaded 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/hubGenomes extraneous genome=araTha1 causes error
err24: setOutput
	@printf "### $@ '${SERVERNAME}/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt;genome=araTha1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -endpoint="/list/hubGenomes" -genome=araTha1 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes missing hubUrl or db
err25: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl no track specified
err26: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect genome specified
err27: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=assembly;genome=axaTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=axaTha1 -track=assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect track specified
err28: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=asemblx;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -track=assemblx 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes hubUrl bad genome name
err29: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?genome=arxTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=arxTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err30: setOutput
	@printf "### $@ '${SERVERNAME}/list/publicHubs?hubUrl=araTha1;genome=abc'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/publicHubs" -hubUrl=araTha1 -genome=abc 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err31: setOutput
	@printf "### $@ '${SERVERNAME}/list/ucscGenomes?hubUrl=araTha1;genome=abc'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/ucscGenomes" -hubUrl=araTha1 -genome=abc 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err32: setOutput
	@printf "### $@ '${SERVERNAME}/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/hubGenomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err33: setOutput
	@printf "### $@ '${SERVERNAME}/list/tracks?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/tracks" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err34: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err35: setOutput
	@printf "### $@ '${SERVERNAME}/getData/sequence?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1;start=4567;end=8901;trackLeavesOnly'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 -start=4567 -end=8901 -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err36: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1;start=4567;end=8901;trackLeavesOnly;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;trackLeavesOnly=1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 -start=4567 -end=8901 -trackLeavesOnly -track=assembly -maxItemsOutput=5 -jsonOutputArrays -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing duplicated parameters on functions
err37: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;maxItemsOutput=0'\n"
	@htmlCheck getAll "${SERVERNAME}/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;maxItemsOutput=0" 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing illegal values on parameters
err38: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=xyz'\n"
	@htmlCheck getAll "${SERVERNAME}/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=xyz" 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err39: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=cosmicRegions;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=cosmicRegions -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err40: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=decipherRaw;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=decipherRaw -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz


# testing protected track data
err41: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=decipher;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=decipher -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err42: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=knownToDecipher;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=knownToDecipher -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err43: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=knownCanonToDecipher;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=knownCanonToDecipher -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err44: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=decipherSnvsRaw;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=decipherSnvsRaw -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err45: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=lovdShort;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=lovdShort -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err46: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=hgmd;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=hgmd -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err47: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=lovdLong;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=lovdLong -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing always protected tracks, even with a range
err48: setOutput
	@printf "### $@ '${SERVERNAME}.edu/getData/track?tracks=gold,gap,decipher;genome=hg38;jsonOutputArrays=1;chrom=chr2;start=16129261;end=16163278'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=gold,gap,decipher -jsonOutputArrays -chrom=chr2 -start=16129261 -end=16163278 2>&1 | sed -e 's/, /,\n/g; s#http.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected  tracks that are allowed if not whole genome
err49: setOutput
	@printf "### $@ '${SERVERNAME}.edu/getData/track?tracks=gold,gap,omimGene2;genome=hg38;jsonOutputArrays=1;chrom=chr2;start=16129261;end=16163278'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg38 -track=gold,gap,omimGene2 -jsonOutputArrays -chrom=chr2 -start=16129261 -end=16163278 2>&1 | sed -e 's/, /,\n/g; s#http.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing chrom alias for a hub that doesn't have any aliases
err50: setOutput
	@printf "### $@ '${SERVERNAME}.edu/getData/track?tracks=assembly;genome=araTha1;chrom=chrPtld;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrPtld" 2>&1 | sed -e 's/, /,\n/g; s#http.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=GCA_021951015.2 - illegal name
err51: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=GCA_021951015.2'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="GCA_021951015.2" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=hg21 - illegal name
err52: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=hg21'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="hg21" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/files?genome=GCA_021951015.1 - additional illegal argument track
err53: setOutput
	@printf "### $@ '${SERVERNAME}/list/files?genome=GCA_021951015.1;track=gc5Base'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/files" -genome="GCA_021951015.1" -track="gc5Base" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;  s#/hubs/.*/hub#/hubs/hub#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

#####################  wiggle data ###########################################
# classic wiggle with wig table and wib file
wig01: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce4 -track=gc5Base -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig02: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;chrom=chrIV;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIV -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig03: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;chrom=chrIII;start=12345;end=45678;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIII -start=12345 -end=45678 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic bigWig with table and bigWig file
wig04: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig05: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIV;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIV -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig06: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIII;start=12345;end=45678;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIII -start=12345 -end=45678 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl
wig07: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl with chrom
wig08: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chr19;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chr19 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track with no database table, just a bigDataUrl with chrom,start,end
wig09: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chrX;start=123456;end=456789;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chrX -start=123456 -end=456789 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file
wig10: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig11: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig12: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -start=1234 -end=5647 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file, with jsonArrayOutput
wig13: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;maxItemsOutput=5;jsonOutputArrays=1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -jsonOutputArrays -endpoint="/getData/track" -genome=ce4 -track=gc5Base -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig14: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;chrom=chrIV;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIV -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig15: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5Base;genome=ce4;chrom=chrIII;start=12345;end=45678;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIII -start=12345 -end=45678 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic bigWig with table and bigWig file
wig16: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig17: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIV;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIV -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig18: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIII;start=12345;end=45678;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIII -start=12345 -end=45678 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl
wig19: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl with chrom
wig20: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chr19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chr19 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track with no database table, just a bigDataUrl with chrom,start,end
wig21: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chrX;start=123456;end=456789;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chrX -start=123456 -end=456789 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file
wig22: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig23: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig24: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -start=1234 -end=5647 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

##############################################################################
### search - test search function
##############################################################################

# basic search just to see if it works:
search01: setOutput
	@ printf "### $@ '${SERVERNAME}/search?search=brca;genome=hg38'\n"
	@ curl -L "${SERVERNAME}/search?search=brca;genome=hg38" 2> /dev/null | python -mjson.tool | egrep -c -n "\[|\]" | awk -v t="$@" '{if (($$1 < 30) || ($$1 > 50)) {printf "test %s failed, count %d not in range 30-40\n", t, $$1; exit 255;}}'

##############################################################################
### chrAlias - test chrom alias
##############################################################################

# testing /getData/track?genome=ce11&track=gold&chrom=MT
chrAlias01: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=ce11;track=gold;chrom=MT'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="MT" 2>&1 | egrep -v "${excludeLines}"  | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=NC_001328.1
chrAlias02: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=ce11;track=gold;chrom=NC_001328.1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="NC_001328.1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=X54252.1
chrAlias03: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=ce11;track=gold;chrom=X54252.1'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="X54252.1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=MtDNA
chrAlias04: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=ce11;track=gold;chrom=MtDNA'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="MtDNA" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# chrom alias on an assembly hub
chrAlias05: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=GCF_000001735.4;track=assembly;chrom=chrPltd;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt" -genome="GCF_000001735.4" -track="assembly" -chrom="chrPltd" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# chrom alias on an assembly hub
chrAlias06: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=GCF_000001735.4;track=assembly;chrom=Pltd;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt" -genome="GCF_000001735.4" -track="assembly" -chrom="Pltd" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? hubUrl genome=GCF_000001735.4&chrom=Pltd&start=4321&end=5647
chrAlias07: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/sequence?genome=GCF_000001735.4;chrom=Pltd;start=4321;end=5678;hubUrl=https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="https://hgdownload.soe.ucsc.edu/hubs/GCF/000/001/735/GCF_000001735.4/hub.txt" -genome="GCF_000001735.4" -chrom="Pltd" -start=4321 -end=5678 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/sequence#/sequence#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? genome=hg19&chrom=MT&start=4321&end=5647
chrAlias08: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/sequence?genome=hg19;chrom=MT;start=4321;end=5678'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="hg19" -chrom="MT" -start=4321 -end=5678 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/sequence#/sequence#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? genome=hs1&chrom=MT&start=4321&end=5647
chrAlias09: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/sequence?genome=hs1;chrom=MT;start=4321;end=5678'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome="hs1" -chrom="MT" -start=4321 -end=5678 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/sequence#/sequence#; s#hubs/.*/hub.txt#hubs/hub.txt#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? track=assembly&genome=hs1&chrom=chr1
chrAlias10: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=hs1;chrom=chr1;track=assembly'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hs1" -chrom="chr1" -track=assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#; s#hubs/.*/hub.txt#hubs/hub.txt#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? track=assembly&genome=hs1&chrom=CP068276.2
chrAlias11: setOutput
	@ printf "### $@ '${SERVERNAME}/getData/track?genome=hs1;chrom=CP068276.2;track=assembly'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hs1" -chrom="CP068276.2" -track=assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#; s#hubs/.*/hub.txt#hubs/hub.txt#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

##############################################################################
### supportedTypes - test both /list/chromosomes and /getData/track
###                - for each type
##############################################################################

altGraphX: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=sibTxGraph;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="sibTxGraph" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=sibTxGraph;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="sibTxGraph" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

barChart: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=gtexBrain;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="gtexBrain" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=gtexBrain;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="gtexBrain" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

chain: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=chainMelGal1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="chainMelGal1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=chainMelGal1;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="chainMelGal1" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# clonePos does not have an 'as' definiton in tableDescriptions
clonePos: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=clonePos;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="clonePos" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=clonePos;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="clonePos" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

ctgPos: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=ctgPos;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="ctgPos" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=ctgPos;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="ctgPos" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

expRatio: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=gnfAtlas2;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="gnfAtlas2" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=gnfAtlas2;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="gnfAtlas2" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

factorSource: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=encRegTfbsClustered;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="encRegTfbsClustered" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=encRegTfbsClustered;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="encRegTfbsClustered" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

gvf: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=iscaCuratedBenign;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="iscaCuratedBenign" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=iscaCuratedBenign;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="iscaCuratedBenign" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

interact: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=gtexTransEqtl;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="gtexTransEqtl" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=gtexTransEqtl;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="gtexTransEqtl" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# hapmapLdPhCeu does not have an 'as' definiton in tableDescriptions
ld2: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=hapmapLdPhCeu;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="hapmapLdPhCeu" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=hapmapLdPhCeu;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="hapmapLdPhCeu" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

netAlign: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=netMelGal1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="netMelGal1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/getData/track?track=netMelGal1;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="netMelGal1" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

peptideMapping: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=wgEncodeUncBsuProtGm12878MembranefractionSig;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="wgEncodeUncBsuProtGm12878MembranefractionSig" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/getData/track?track=wgEncodeUncBsuProtGm12878MembranefractionSig;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="wgEncodeUncBsuProtGm12878MembranefractionSig" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

pgSnp: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=pgKb1454;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg19" -track="pgKb1454" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/getData/track?track=pgKb1454;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="pgKb1454" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

bigDbSnp: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=dbSnp153Mult;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="dbSnp153Mult" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/getData/track?track=dbSnp153Mult;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="dbSnp153Mult" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/list/schema?track=dbSnp153Mult;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track="dbSnp153Mult" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

bigMaf: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=rbestNetHs1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="rbestNetHs1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/getData/track?track=rbestNetHs1;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="rbestNetHs1" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/list/schema?track=rbestNetHs1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track="rbestNetHs1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

bigChain: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=chainSynGCA_011100615.1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome="hg38" -track="chainSynGCA_011100615.1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/getData/track?track=chainSynGCA_011100615.1;chrom=chr1;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="chainSynGCA_011100615.1" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c >> testOutput/$@.gz
	@printf "### $@ '${SERVERNAME}/list/schema?track=chainSynGCA_011100615.1;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/schema" -genome="hg38" -track="chainSynGCA_011100615.1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c >> testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

##############################################################################
### notSupported
##############################################################################
# request a track type that is not yet supported: bam
notSup01: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=shMethylSubtrack;chrom=chr1;genome=hg19;hubUrl=http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -hubUrl="http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt" -genome="hg19" -track="shMethylSubtrack" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track type that is not yet supported: vcfTabix
notSup02: setOutput
	@printf "### $@ '${SERVERNAME}/cgi-bin/hubApi/getData/track?track=gnomadGenomesVariantsV3;chrom=chr21;start=5033429;end=5033498;genome=hg38;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg38" -track="gnomadGenomesVariantsV3" -chrom="chr21" -start="5033429" -end="5033498" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track type that is not yet supported: ld2
notSup03: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=hapmapLdPhCeu;genome=hg19'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track="hapmapLdPhCeu" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request chromosome list from a track that is a container, not a data track
notSup07: setOutput
	@printf "### $@ '${SERVERNAME}/list/chromosomes?track=gdcCancer;genome=hg38'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/list/chromosomes" -genome=hg38 -track="gdcCancer" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# hapmapLdPhCeu does not have an 'as' definiton in tableDescriptions
notSup10: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?track=hapmapLdPhCeu;chrom=chr1;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg19" -track="hapmapLdPhCeu" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz


#     34 bam - not supported</li>
#      2 bigBarChart - not supported</li>
#      3 bigInteract - not supported</li>
#     18 bigMaf - not supported</li>
#    152 composite container - not supported</li>
#     93 composite view - not supported</li>
#     37 halSnake - not supported</li>
#     31 superTrack child - not supported</li>
#      5 vcfTabix - not supported</li>

#redmine23733: setOutput
#	@printf "### $@ '${SERVERNAME}/getData/track?track=knownGene;genome=hg19;chrom=chr1;end=48000;start=47000'\n"
#	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome=hg19 -track=knownGene -chrom=chr1 -start=47000 -end=48000 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | grep -v "^#" | sort | gzip -c > testOutput/$@.gz
#	@zdiff expected/$@.gz testOutput/$@.gz

# end coordinate for get sequence past end of chromosome, database genome
redmine24089a: setOutput
	@printf "### $@ '${SERVERNAME}/getData/sequence?genome=hg19;chrom=chrM;start=16560;end=16572'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome=hg19 -chrom=chrM -start=16560 -end=16572 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# end coordinate for get sequence past end of chromosome, asmHub genome
redmine24089b: setOutput
	@printf "### $@ '${SERVERNAME}/getData/sequence?genome=araTha1;chrom=chrCp;start=154470;end=154479;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -chrom="chrCp" -start=154470 -end=154479 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# full chromosome fetch from a NIB based genom
redmine24666: setOutput
	@printf "### $@ '${SERVERNAME}/getData/sequence?genome=ci2;chrom=scaffold_8534'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/sequence" -genome=ci2 -chrom=scaffold_8534 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | grep -v "^#" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# full chromosome fetch from a NIB based genom
redmine25840: setOutput
	@printf "### $@ '${SERVERNAME}/getData/track?genome=hg18;track=cnpSebat2'\n"
	@./jsonConsumer.pl -serverName="${SERVERNAME}" -endpoint="/getData/track" -genome="hg18" -track="cnpSebat2" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

#	curl -L "https://api-test.gi.ucsc.edu/getData/sequence?genome=ci2;chrom=scaffold_8534" 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
# track types found in hg19 and hg38 trackDb 'type' column:
#   4586 narrowPeak
#   4472 bigWig
#   3294 bed
#   1745 broadPeak
#    872 chain
#    871 netAlign
#    596 bam
#    512 genePred
#    438 bigBed
#    368 pgSnp
#    348 bedGraph
#    286 wig
#    106 psl
#     46
#     35 bigLolly
#     26 wigMaf
#     22 gvf
#     22 chromGraph
#     22 bigPsl
#     21 vcfTabix
#     17 expRatio
#     13 bigBarChart
#     12 rmsk
#     12 bigInteract
#      8 ld2
#      8 bedDetail
#      7 peptideMapping
#      6 sample
#      6 altGraphX
#      5 factorSource
#      5 bed5FloatScore
#      4 ctgPos
#      2 type
#      2 downloadsOnly
#      2 coloredExon
#      2 clonePos
#      1 maf
#      1 interact
#      1 bigGenePred
#      1 bed6FloatScore
#      1 barChart
#      1 bamWig
