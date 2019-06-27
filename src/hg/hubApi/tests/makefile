
test:: test0 test6 test7 test8 test9 test10  \
	test11 test12 test13 test14 test15 test16 test19 test21 \
	test26 test27 test28 test29 test39 test40 \
	listFunctions getFunctions getSequence wigData errorTests \
	notSupported bugReports

all:: listFunctions getFunctions getSequence wigData errorTests \
	notSupported

listFunctions: list01 list02 list03 list04 list05 list06 list07 list08 list09 \
	list10 list11 list12 list13 list14 list15

getFunctions: test6 test7 test8 test9 test10 test11 test12 test13 test14 \
	test15 test16 test19 test21 test26 test27 test28 \
	test29 test39 test40

getSequence: getSeq01 getSeq02 getSeq03

wigData: wig01 wig02 wig03 wig04 wig05 wig06 wig07 wig08 wig09 wig10 \
	wig11 wig12 wig13 wig14 wig15 wig16 wig17 wig18 wig19 wig20 \
	wig21 wig22 wig23 wig24

errorTests: err01 err02 err03 err04 err05 err06 err07 err08 err09 err10 \
	err11 err12 err13 err14 err15 err16 err17 err18 err19 err20 \
	err21 err22 err23 err24 err25 err26 err27 err28 err29 err30 \
	err31 err32 err33 err34 err35 err36 err37 err38 err39

notSupported: notSup01 notSup02

bugReports: redmine23733

setOutput:
	@if [ ! -d testOutput ]; then mkdir testOutput; fi

clean::
	rm -f testOutput/*.gz
	rmdir testOutput

# exclude these lines from the JSON output for comparison with expected
excludeLines = downloadTime|dataTime

# testing /list/noSubCommand, /list/publicHubs and /list/ucscGenomes
test0: setOutput
	@./jsonConsumer.pl -test0 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz
	@echo '### test0: ./jsonConsumer.pl -test0 - OK'

################################################### list functions ###########
# testing /list/hubGenomes
list01: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/hubGenomes" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? hubUrl genome=araTha1 trackLeavesOnly
list02: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/tracks?trackLeavesOnly=1;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="araTha1" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11 trackLeavesOnly=1
list03: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/tracks?trackLeavesOnly=1;genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/tracks" -genome="ce11" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11
list04: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=gold
list05: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=gold;genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" -track="gold" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl genome=araTha1
list06: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl genome=araTha1 track=assembly
list07: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=assembly;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? hubUrl genome=araTha1
list08: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/tracks?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="araTha1" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11
list09: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/tracks?genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/tracks" -genome="ce11" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=mrna SQL table name is all_mrna
list10: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=mrna;genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" -track="mrna" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=mrna SQL table name is all_mrna
# and ce4 has split tables
list11: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=mrna;genome=ce4'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce4" -track="mrna" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=rmsk different chrom name column
list12: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=rmsk;genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" -track="rmsk" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=rmsk different chrom name column
# and ce4 has split tables
list13: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=rmsk;genome=ce4'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce4" -track="rmsk" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=est different chrom name column
list14: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=est;genome=ce11'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" -track="est" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce4&track=est different chrom name column
# and ce4 has split tables
list15: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/list/chromosomes?track=est;genome=ce4'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce4" -track="est" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

##########################   getData functions  #############################
# testing /getData/sequence?genome=ce11&chrom=chrM
test6: setOutput
	@./jsonConsumer.pl -endpoint="/getData/sequence" -genome="ce11" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?genome=ce11&chrom=chrM&start=1&end=128
test7: setOutput
	@./jsonConsumer.pl -endpoint="/getData/sequence" -genome="ce11" -chrom="chrM" -start=1 -end=128 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold
test8: setOutput
	@./jsonConsumer.pl -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce11" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=chrM
test9: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce11&track=gold&chrom=chrI&start=1&end=1024
test10: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="gold" -chrom="chrI" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold assembly hub
test11: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrM assembly hub
test12: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrCp" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrI&start=1&end=1024 assembly hub
test13: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrCp" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved  track hub
test14: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1  track hub
test15: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" -chrom="chr1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1&start=N&end=M  track hub
test16: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" -chrom="chr1" -start=10600000 -end=10800000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# bigBed data from a database
# /getData/track? genome=galGal6 &track=ncbiRefSeqOther&chrom=chr1&start=750000&end=5700000
test19: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="galGal6" -track="ncbiRefSeqOther" -chrom="chr1" -start=750000 -end=55700000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a database where the SQL table name is different than the
#    track name
# /getData/track? genome=hg19 &track=wgEncodeRegMarkH3k4me1H1hesc&chrom=chr1&start=62300000&end=62301000
test21: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="hg19" -track="wgEncodeRegMarkH3k4me1H1hesc" -chrom="chr1" -start=62300000 -end=62301000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing genePred track
#    /getData/track?genome=ce11&chrom=chrV&track=refGene&start=5231045&end=5241045
test26: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="refGene"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing PSL track
#    /getData/track?genome=ce11&chrom=chrV&track=ncbiRefSeqPsl&start=5231045&end=5241045
test27: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="ncbiRefSeqPsl"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing RMSK track
#    /getData/track?genome=ce11&debug=0&chrom=chrV&track=rmsk&start=5231045&end=5241045
test28: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="rmsk"  -chrom="chrV" -start=5231045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# test superTrack child getData
#    /getData/track?genome=ce11&chrom=chrV&track=cpgIslandExt&start=5001045&end=5241045
test29: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -genome="ce11" -track="cpgIslandExt"  -chrom="chrV" -start=5001045 -end=5241045 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce4&track=gold for split table operation
test39: setOutput
	@./jsonConsumer.pl -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce4" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce4&track=gold for split table operation
# plus jsonArrayOutput
test40: setOutput
	@./jsonConsumer.pl -jsonOutputArrays -maxItemsOutput=123 -endpoint="/getData/track" -genome="ce4" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

###############################################################################
#  /getData/sequence testing
###############################################################################
# testing /getData/sequence? hubUrl genome=hg19&chrom=chrCp&start=4321&end=5647
getSeq01: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -chrom="chrCp" -start=4321 -end=5678 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? hubUrl Bejerano genome=hg19&chrom=chrM&start=4321&end=5647
getSeq02: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -chrom="chrM" -start=4321 -end=5678 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence? genome=monDom5&chrom=chr1&start=4321&end=5647
getSeq03: setOutput
	@printf "### $@ "
	@./jsonConsumer.pl -endpoint="/getData/sequence" -genome="monDom5" -chrom="chr1" -start=748055061 -end=748055161 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

###############################################################################
#  ERROR testing
###############################################################################
# generate an error: ask for non-existent chrom in a track hub
# /getData/track? hubUrl Plants &track=assembly&chrom=chrI&start=1&end=14309681
err01: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=assembly;chrom=chrI;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=14309681;start=1\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="assembly" -chrom="chrI" -start=1 -end=14309681 2>&1 | sed -e 's#https://.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not open bigWig file
err02: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base;chrom=chrA9;genome=braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome="braRap1" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not find hubUrl
err03: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base;chrom=chrA9;genome=braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt" -genome="braRap1" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /getData/track  with hubUrl
err04: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base;chrom=chrA9;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -track="gc5Base" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /getData/sequence  with hubUrl
err05: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/sequence?chrom=chrA9;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /list/tracks  with hubUrl
err06: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/tracks?trackLeavesOnly=1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument hubUrl for /list/tracks
err07: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/tracks?trackLeavesOnly=1;genome=araTha1\n"
	@./jsonConsumer.pl -genome="araTha1" -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument hubUrl and genome for /list/tracks
err08: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/tracks?trackLeavesOnly=1\n"
	@./jsonConsumer.pl -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?genome=cxe6 - error test illegal database name
# plus jsonArrayOutput
err09: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/sequence?genome=cxe6;jsonOutputArrays=1;maxItemsOutput=123\n"
	@./jsonConsumer.pl -jsonOutputArrays -maxItemsOutput=123 -endpoint="/getData/sequence" -genome="cxe6" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=cxe6 - error test illegal database name
# plus jsonArrayOutput
err10: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/tracks?genome=cxe6;jsonOutputArrays=1;maxItemsOutput=123\n"
	@./jsonConsumer.pl -jsonOutputArrays -maxItemsOutput=123 -endpoint="/list/tracks" -genome="cxe6" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err11: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?genome=ce6;track=gold;maxItemsOutput=1x23\n"
	@./jsonConsumer.pl -maxItemsOutput=1x23 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err12: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?genome=ce6;track=gold;maxItemsOutput=-2\n"
	@./jsonConsumer.pl -maxItemsOutput=-2 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err13: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?genome=ce6;track=gold;maxItemsOutput=0\n"
	@./jsonConsumer.pl -maxItemsOutput=0 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?genome=ce6 - error test illegal maxItemsOutput
err14: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?genome=ce6;track=gold;maxItemsOutput=0\n"
	@./jsonConsumer.pl -maxItemsOutput=0 -endpoint="/getData/track" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/tracks?genome=ce6 - error test no such function on endpoint
err15: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/tracks?genome=ce6;track=gold\n"
	@./jsonConsumer.pl -endpoint="/getData/tracks" -genome="ce6" -track=gold 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/endpoints - error test no such function
err16: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/endpoints\n"
	@./jsonConsumer.pl -endpoint="/list/endpoints" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/hubGenomes with hubUrl but no genome specified
err17: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/hubGenomes?genome=araTha1'\n"
	@./jsonConsumer.pl -endpoint="/list/tracks" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl missing genome=
err18: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl missing incorrect genome=araTha
err19: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome=araTha 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl can not
#  find specified track for genome=braRap1 track=assembly (wrong name)
err20: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt;genome=braRap1;track=assemblx\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome=braRap1 -track=assemblx 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl can not
#  find specified database in track hub
err21: setOutput
	@printf "### $@ 'https://apibeta.soe.ucsc.edu/list/chromosomes?genome=hg18;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome=hg18 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? bad hubUrl
err22: setOutput
	@printf "### $@ http://genome-test.gi.ucsc.edu/~hiram/hubs/xGillBejerano/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/xGillBejerano/hub.txt" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? genome=ce11 track=gbLoaded - not a position track
err23: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/chromosomes?genome=ce11;track=gbLoaded'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome=ce11 -track=gbLoaded 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/hubGenomes extraneous genome=araTha1 causes error
err24: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt;genome=araTha1'\n"
	@./jsonConsumer.pl -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -endpoint="/list/hubGenomes" -genome=araTha1 2>&1 | sed -e 's#https://.*/list#/list#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes missing hubUrl or db
err25: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/chromosomes'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl no track specified
err26: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect genome specified
err27: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?track=assembly;genome=axaTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=axaTha1 -track=assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect track specified
err28: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?track=asemblx;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -track=assemblx 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes hubUrl bad genome name
err29: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/chromosomes?genome=arxTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=arxTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err30: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/publicHubs?hubUrl=araTha1;genome=abc'\n"
	@./jsonConsumer.pl -endpoint="/list/publicHubs" -hubUrl=araTha1 -genome=abc 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err31: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/ucscGenomes?hubUrl=araTha1;genome=abc'\n"
	@./jsonConsumer.pl -endpoint="/list/ucscGenomes" -hubUrl=araTha1 -genome=abc 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err32: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/hubGenomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1'\n"
	@./jsonConsumer.pl -endpoint="/list/hubGenomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err33: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/tracks?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1'\n"
	@./jsonConsumer.pl -endpoint="/list/tracks" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err34: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1'\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err35: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/sequence?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1;start=4567;end=8901;trackLeavesOnly'\n"
	@./jsonConsumer.pl -endpoint="/getData/sequence" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 -start=4567 -end=8901 -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing extraneous parameters on functions
err36: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;trackLeavesOnly=1;chrom=chr1;start=4567;end=8901;trackLeavesOnly;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;trackLeavesOnly=1'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 -trackLeavesOnly -chrom=chr1 -start=4567 -end=8901 -trackLeavesOnly -track=assembly -maxItemsOutput=5 -jsonOutputArrays -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing duplicated parameters on functions
err37: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;maxItemsOutput=0'\n"
	@htmlCheck getAll 'https://api-test.gi.ucsc.edu/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=1;maxItemsOutput=0' 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing illegal values on parameters
err38: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=xyz'\n"
	@htmlCheck getAll 'https://api-test.gi.ucsc.edu/getData/track?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;genome=araTha1;chrom=chr1;start=4567;end=8901;track=assembly;maxItemsOutput=5;jsonOutputArrays=xyz' 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing protected track data
err39: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=cosmicRegions;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=cosmicRegions -maxItemsOutput=5 -jsonOutputArrays 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

#####################  wiggle data ###########################################
# classic wiggle with wig table and wib file
wig01: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce4 -track=gc5Base -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig02: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;chrom=chrIV;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIV -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig03: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;chrom=chrIII;start=12345;end=45678;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIII -start=12345 -end=45678 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic bigWig with table and bigWig file
wig04: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig05: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIV;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIV -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig06: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIII;start=12345;end=45678;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIII -start=12345 -end=45678 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl
wig07: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl with chrom
wig08: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chr19;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chr19 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track with no database table, just a bigDataUrl with chrom,start,end
wig09: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chrX;start=123456;end=456789;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chrX -start=123456 -end=456789 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file
wig10: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig11: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig12: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -start=1234 -end=5647 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file, with jsonArrayOutput
wig13: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;maxItemsOutput=5;jsonOutputArrays=1'\n"
	@./jsonConsumer.pl -jsonOutputArrays -endpoint="/getData/track" -genome=ce4 -track=gc5Base -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig14: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;chrom=chrIV;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIV -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig15: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5Base;genome=ce4;chrom=chrIII;start=12345;end=45678;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce4 -track=gc5Base -chrom=chrIII -start=12345 -end=45678 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic bigWig with table and bigWig file
wig16: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom specified
wig17: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIV;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIV -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# classic wiggle with wig table and wib file with chrom start,end specified
wig18: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gc5BaseBw;genome=ce11;chrom=chrIII;start=12345;end=45678;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=ce11 -track=gc5BaseBw -chrom=chrIII -start=12345 -end=45678 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl
wig19: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track without a database table, just a bigDataUrl with chrom
wig20: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chr19;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chr19 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# a bigWig track with no database table, just a bigDataUrl with chrom,start,end
wig21: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=gnomadGenomesMeanCoverage;genome=hg19;chrom=chrX;start=123456;end=456789;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg19 -track=gnomadGenomesMeanCoverage -chrom=chrX -start=123456 -end=456789 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file
wig22: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig23: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig24: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base;chrom=chrCp;genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="araTha1" -track="gc5Base" -chrom="chrCp" -start=1234 -end=5647 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track that is not yet supported: bam
notSup01: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=shMethylSubtrack;chrom=chr1;genome=hg19;hubUrl=http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt" -genome="hg19" -track="shMethylSubtrack" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track that is not yet supported: vcfTabix
notSup02: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=variants;chrom=chr1;genome=danRer11;hubUrl=http://research.nhgri.nih.gov/manuscripts/Burgess/zebrafish/downloads/NHGRI-1/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://research.nhgri.nih.gov/manuscripts/Burgess/zebrafish/downloads/NHGRI-1/hub.txt" -genome="danRer11" -track="variants" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
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

redmine23733: setOutput
	@printf "### $@ 'https://api-test.gi.ucsc.edu/getData/track?track=knownGene;genome=hg38;chrom=chr1;end=48000;start=47000'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -genome=hg38 -track=knownGene -chrom=chr1 -start=47000 -end=48000 2>&1 | sed -e 's/, /,\n/g;' | egrep -v "${excludeLines}" | tail -5 | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz
