
test::	test0 test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 \
	test11 test12 test13 test14 test15 test16 test17 test18 test19 test20

clean::
	rm -f testOutput/*.gz

excludeLines = downloadTime|downloadTimeStamp

# test::
# 	@./jsonConsumer.pl 2> /dev/null | gzip -c > testOutput.json.gz
# 	@zdiff testOutput.json.gz expected.json.gz

# testing /list/noSubCommand, /list/publicHubs and /list/ucscGenomes
test0:
	@./jsonConsumer.pl 2> /dev/null | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/hubGenomes
test1:
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/hubGenomes" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?genome=_araTha1
test2:
	@./jsonConsumer.pl -hubUrl=http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt -endpoint="/list/tracks" -genome="_araTha1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /list/tracks?db=ce11
test3:
	@./jsonConsumer.pl -endpoint="/list/tracks" -db="ce11" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# TBD XXX testing /list/chromosomes with a hub
# testing /list/chromosomes?db=ce11
test4:
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -db="ce11" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# TBD XXX testing /list/chromosomes with a hub and track
# testing /list/chromosomes?db=ce11&track=gold
test5:
	@./jsonConsumer.pl -endpoint="/list/chromosomes" -db="ce11" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?db=ce11&chrom=chrM
test6:
	@./jsonConsumer.pl -endpoint="/getData/sequence" -db="ce11" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/sequence?db=ce11&chrom=chrM&start=1&end=128
test7:
	@./jsonConsumer.pl -endpoint="/getData/sequence" -db="ce11" -chrom="chrM" -start=1 -end=128 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?db=ce11&track=gold
test8:
	@./jsonConsumer.pl -endpoint="/getData/track" -db="ce11" -track="gold" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?db=ce11&track=gold&chrom=chrM
test9:
	@./jsonConsumer.pl -endpoint="/getData/track" -db="ce11" -track="gold" -chrom="chrM" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track?db=ce11&track=gold&chrom=chrI&start=1&end=1024
test10:
	@./jsonConsumer.pl -endpoint="/getData/track" -db="ce11" -track="gold" -chrom="chrI" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold assembly hub
test11:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrM assembly hub
test12:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrCp" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Plants &track=gold&chrom=chrI&start=1&end=1024 assembly hub
test13:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrCp" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved  track hub
test14:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1  track hub
test15:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" -chrom="chr1" | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# testing /getData/track? hubUrl Bejerano &track=ultraConserved&chrom=chr1&start=N&end=M  track hub
test16:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/GillBejerano/hub.txt" -genome="hg19" -track="ultraConserved" -chrom="chr1" -start=10600000 -end=10800000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# wiggle data from a track hub
# testing /getData/track? hubUrl Plants &track=gc5Base_&chrom=chrCp&start=1&end=1024 assembly hub
test17:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="gc5Base_" -chrom="chrCp" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# bigWig data from a database
# /getData/track? db=galGal6 &track=gc5BaseBw&chrom=chr1&start=1&end=1024
test18:
	@./jsonConsumer.pl -endpoint="/getData/track" -db="galGal6" -track="gc5BaseBw" -chrom="chr1" -start=1 -end=1024 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# bigBed data from a database
# /getData/track? db=galGal6 &track=gc5BaseBw&chrom=chr1&start=1&end=1024
test19:
	@./jsonConsumer.pl -endpoint="/getData/track" -db="galGal6" -track="ncbiRefSeqOther" -chrom="chr1" -start=750000 -end=55700000 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz

# generate an error: ask for non-existent chrom in a track hub
# /getData/track? hubUrl Plants &track=assembly_&chrom=chrI&start=1&end=14309681
test20:
	@./jsonConsumer.pl -endpoint="/getData/track" -hubUrl="http://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt" -genome="_araTha1" -track="assembly_" -chrom="chrI" -start=1 -end=14309681 | egrep -v "${excludeLines}" | gzip -c > testOutput/$@.gz
	@zdiff expected/$@.gz testOutput/$@.gz


