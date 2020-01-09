# for GNU Make automatic variable documentation, see also:
# https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html

Name="GlobalReference"
destDir=/hive/data/genomes/asmHubs/globalReference
srcDir=${HOME}/kent/src/hg/makeDb/doc/globalReference

all:: ${destDir}/hub.txt ${destDir}/groups.txt ${destDir}/genomes.txt

abc:: ${destDir}/hub.txt ${destDir}/groups.txt
	@ls -og ${destDir}/index.html ${destDir}/asmStats${Name}.html
	@ls -og ${destDir}/commonNameOrder.list
	@ls -og $^

# this mkHubIndex.pl also writes out a commonNameOrder.list that
# is used by mkAsmStats.pl and mkGenomes.pl to order their listings

${destDir}/index.html: ${srcDir}/mkHubIndex.pl ${srcDir}/commonNames.txt ${classList}
	cd ${destDir} && ${srcDir}/mkHubIndex.pl > $@
	chmod 775 $@

${destDir}/genomes.txt:  ${destDir}/asmStats${Name}.html ${srcDir}/mkGenomes.pl ${srcDir}/mkSymLinks.sh
	cd ${destDir} && ${srcDir}/mkGenomes.pl > $@
	cd ${destDir} && ${srcDir}/mkSymLinks.sh

${destDir}/asmStats${Name}.html: ${destDir}/index.html ${srcDir}/mkAsmStats.pl
	cd ${destDir} && ${srcDir}/mkAsmStats.pl > $@
	chmod 775 $@

${destDir}/hub.txt: ${srcDir}/hub.txt
	rm -f ${destDir}/hub.txt
	cp -p ${srcDir}/hub.txt ${destDir}/hub.txt

${destDir}/groups.txt: ${srcDir}/groups.txt
	rm -f ${destDir}/groups.txt
	cp -p ${srcDir}/groups.txt ${destDir}/groups.txt
