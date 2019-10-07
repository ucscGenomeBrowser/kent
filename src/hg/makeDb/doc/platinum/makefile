# for GNU Make automatic variable documentation, see also:
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html

destDir=/hive/data/genomes/asmHubs/platinumGenomes
srcDir=${HOME}/kent/src/hg/makeDb/doc/platinum

all:: ${destDir}/genomes.txt

abc:: ${destDir}/hub.txt ${destDir}/groups.txt
	@ls -og ${destDir}/index.html ${destDir}/asmStatsPlatinum.html
	@ls -og ${destDir}/commonNameOrder.list
	@ls -og $^

# this mkHubIndex.pl also writes out a commonNameOrder.list that
# is used by mkAsmStats.pl and mkGenomes.pl to order their listings

${destDir}/index.html: ${srcDir}/mkHubIndex.pl ${srcDir}/commonNames.txt ${classList}
	cd ${destDir} && ${srcDir}/mkHubIndex.pl > $@
	chmod 775 $@

${destDir}/genomes.txt:  ${destDir}/asmStatsPlatinum.html ${srcDir}/mkGenomes.pl ${srcDir}/mkSymLinks.sh
	cd ${destDir} && ${srcDir}/mkGenomes.pl > $@
	cd ${destDir} && ${srcDir}/mkSymLinks.sh

${destDir}/asmStatsPlatinum.html: ${destDir}/index.html ${srcDir}/mkAsmStats.pl
	cd ${destDir} && ${srcDir}/mkAsmStats.pl > $@
	chmod 775 $@
