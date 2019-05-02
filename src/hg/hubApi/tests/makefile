
test:: test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10  \
	test11 test12 test13 test14 test15 test16 test19 test21 test22 test23 \
	test26 test27 test28 test29 test30 test31 test39 test40 \
	listFunctions getFunctions getSequence wigData errorTests

all:: listFunctions getFunctions getSequence wigData errorTests

listFunctions:	test1 test2 test3 test4 test5 test22 test23 test30 test31

getFunctions: test6 test7 test8 test9 test10 test11 test12 test13 test14 \
	test15 test16 test19 test21 test26 test27 test28 \
	test29 test39 test40

getSequence: getSeq01 getSeq02 getSeq03

wigData: wig01 wig02 wig03 wig04 wig05 wig06 wig07 wig08 wig09 wig10 \
	wig11 wig12 wig13 wig14 wig15 wig16 wig17 wig18 wig19 wig20 \
	wig21 wig22 wig23 wig24

errorTests: err01 err02 err03 err04 err05 err06 err07 err08 err09 err10 \
	err11 err12 err13 err14 err15 err16 err17 err18 err19 err20 \
	err21 err22 err23 err24 err25 err26 err27 err28 err29

notSupported: notSup01 notSup02

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

# testing /list/hubGenomes
test1: setOutput
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/hubGenomes" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks? hubUrl genome=_araTha1 trackLeavesOnly
test2: setOutput
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="_araTha1" -trackLeavesOnly | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11 trackLeavesOnly=1
test3: setOutput
	@./jsonConsumer.pl -endpoint="/list/tracks" -genome="ce11" -trackLeavesOnly | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11
test4: setOutput
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes?genome=ce11&track=gold
test5: setOutput
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -genome="ce11" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

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
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrM assembly hub
test12: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrCp" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrI&start=1&end=1024 assembly hub
test13: setOutput
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrCp" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
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

# testing /list/chromosomes? hubUrl genome=_araTha1
test22: setOutput
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl genome=_araTha1 track=assembly_
test23: setOutput
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
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

# testing /list/tracks? hubUrl genome=_araTha1
test30: setOutput
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="_araTha1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=ce11
test31: setOutput
	@./jsonConsumer.pl -endpoint="/list/tracks" -genome="ce11" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
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
	@./jsonConsumer.pl -endpoint="/getData/sequence" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -chrom="chrCp" -start=4321 -end=5678 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
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
# /getData/track? hubUrl Plants &track=assembly_&chrom=chrI&start=1&end=14309681
err01: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=assembly_;chrom=chrI;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=14309681;start=1\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrI" -start=1 -end=14309681 2>&1 | sed -e 's#https://.*/getData#/getData#;' | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not open bigWig file
err02: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base_;chrom=chrA9;genome=_braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome="_braRap1" -track="gc5Base_" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, can not find hubUrl
err03: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base_;chrom=chrA9;genome=_braRap1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/xPlants/hub.txt" -genome="_braRap1" -track="gc5Base_" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# error test, missing argument genome for /getData/track  with hubUrl
err04: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/getData/track?track=gc5Base_;chrom=chrA9;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -track="gc5Base_" -chrom=chrA9 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
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
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/tracks?trackLeavesOnly=1;genome=_araTha1\n"
	@./jsonConsumer.pl -genome="_araTha1" -endpoint="/list/tracks" -trackLeavesOnly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
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
#  find specified track for genome=_braRap1 track=_assembly (wrong name)
err20: setOutput
	@printf "### $@ https://hgwdev-api.gi.ucsc.edu/list/chromosomes?hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt;genome=_braRap1;track=_assembly\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/bPlants/hub.txt" -genome=_braRap1 -track=_assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes? hubUrl can not
#  find specified database in track hub
err21: setOutput
	@printf "### $@ http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt?genome=hg19\n"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome=hg19 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
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
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=_araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect genome specified
err27: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?track=assembly_;genome=araTha1_;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1_ -track=assembly_ 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl incorrect track specified
err28: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/getData/track?track=_assembly;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=_araTha1 -track=_assembly 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/chromosomes hubUrl bad genome name
err29: setOutput
	@printf "### $@ 'https://hgwdev-api.gi.ucsc.edu/list/chromosomes?genome=araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt'"
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -genome=araTha1 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/list#/list#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

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
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig11: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;chrom=chrCp;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -chrom="chrCp" -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig12: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;chrom=chrCp;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -chrom="chrCp" -start=1234 -end=5647 -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
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
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom
wig23: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;chrom=chrCp;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -chrom="chrCp" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub bigWig file, with chrom, start, end
wig24: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=gc5Base_;chrom=chrCp;genome=_araTha1;hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt;end=5647;start=1234;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -chrom="chrCp" -start=1234 -end=5647 -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track that is not yet supported: bam
notSup01: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=shMethylSubtrack;chrom=chr1;genome=hg19;hubUrl=http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://lasallelab.genomecenter.ucdavis.edu/UCSChub/hub.txt" -genome="hg19" -track="shMethylSubtrack" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# request a track that is not yet supported: vcfTabix
notSup02: setOutput
	@printf "### $@ 'https://hgwdev-hiram.gi.ucsc.edu/cgi-bin/hubApi/getData/track?track=CTVT_SNV_Som;chrom=chr1;genome=canFam3;hubUrl=https://data.broadinstitute.org/vgb/dog/dog/hub.txt;jsonOutputArrays=1;maxItemsOutput=5'\n"
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="https://data.broadinstitute.org/vgb/dog/dog/hub.txt" -genome="canFam3" -track="CTVT_SNV_Som" -chrom="chr1" -jsonOutputArrays -maxItemsOutput=5 2>&1 | egrep -v "${excludeLines}" | sed -e 's#https://.*/getData#/getData#;' | gzip -c > testOutput/$@.gz
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

