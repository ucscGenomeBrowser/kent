
# for GNU Make automatic variable documentation, see also:
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html

destDir=/hive/data/genomes/asmHubs/VGP/ucscNames
srcDir=${HOME}/kent/src/hg/makeDb/doc/VGP

all:: ${destDir}/genomes.txt ${destDir}/hub.txt ${destDir}/groups.txt
	@ls -og ${destDir}/index.html ${destDir}/asmStatsVGP.html
	@ls -og ${destDir}/commonNameOrder.list
	@ls -og $^

classList=amphibian.list bird.list fish.list mammal.list reptile.list

# this mkHubIndex.pl also writes out a commonNameOrder.list that
# is used by mkAsmStats.pl and mkGenomes.pl to order their listings

${destDir}/index.html: ${srcDir}/mkHubIndex.pl ${srcDir}/commonNames.txt ${classList}
	cd ${destDir} && ${srcDir}/mkHubIndex.pl > $@
	chmod 775 $@

${destDir}/asmStatsVGP.html: ${destDir}/index.html ${srcDir}/mkAsmStats.pl
	cd ${destDir} && ${srcDir}/mkAsmStats.pl > $@
	chmod 775 $@

${destDir}/genomes.txt:  ${destDir}/asmStatsVGP.html ${srcDir}/mkGenomes.pl
	cd ${destDir} && ${srcDir}/mkGenomes.pl > $@
	cd ${destDir} && ${srcDir}/mkSymLinks.sh

${destDir}/hub.txt: ${srcDir}/hub.txt
	rm $@
	cp -p  $< $@

${destDir}/groups.txt: ${srcDir}/groups.txt
	rm $@
	cp -p  $< $@
