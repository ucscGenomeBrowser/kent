
destDir=/hive/data/genomes/asmHubs/VGP/ucscNames
srcDir=${HOME}/kent/src/hg/makeDb/doc/VGP

all:: ${destDir}/genomes.txt
	ls -og ${destDir}/hubIndex.html ${destDir}/asmStatsVGP.html
	ls -og ${destDir}/genomes.txt ${destDir}/commonNameOrder.list

classList=amphibian.list bird.list fish.list mammal.list reptile.list

# this mkHubIndex.pl also writes out a commonNameOrder.list that
# is used by mkAsmStats.pl and mkGenomes.pl to order their listings

${destDir}/hubIndex.html: ${srcDir}/mkHubIndex.pl ${srcDir}/commonNames.txt ${classList}
	cd ${destDir} && ${srcDir}/mkHubIndex.pl > $@
	chmod 775 $@

${destDir}/asmStatsVGP.html: ${destDir}/hubIndex.html ${srcDir}/mkAsmStats.pl
	cd ${destDir} && ${srcDir}/mkAsmStats.pl > $@
	chmod 775 $@

${destDir}/genomes.txt:  ${destDir}/asmStatsVGP.html ${srcDir}/mkGenomes.pl
	cd ${destDir} && ${srcDir}/mkGenomes.pl > $@

