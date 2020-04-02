# generic makefile to construct the index pages and symlinks
# for any of the assembly hubs:
# primates, mammals, birds, fish, vertebrate
#
# will be included by those individual build directories with the
# Name and name defined to use here

destDir=/hive/data/genomes/asmHubs/${name}
srcDir=${HOME}/kent/src/hg/makeDb/doc/${name}AsmHub
toolsDir=${HOME}/kent/src/hg/makeDb/doc/asmHubs

all:: makeDirs symLinks ${destDir}/hub.txt ${destDir}/groups.txt \
	mkGenomes ${destDir}/index.html \
	${destDir}/asmStats.html

makeDirs:
	mkdir -p ${destDir}

symLinks::
	${toolsDir}/mkSymLinks.pl ${Name} ${name}

hubIndex::
	${toolsDir}/mkHubIndex.pl ${Name} ${name} ${defaultAssembly} > ${destDir}/index.html
	sed -e "s/hgdownload.soe/hgdownload-test.gi/g; s#/index.html#/testIndex.html#; s#${name}/hub.txt#${name}/testHub.txt#; s/asmStats/testAsmStats/;" ${destDir}/index.html > ${destDir}/testIndex.html
	chmod +x ${destDir}/index.html ${destDir}/testIndex.html

asmStats::
	rm -f ${destDir}/asmStats${Name}.html ${destDir}/testAsmStats${Name}.html
	${toolsDir}/mkAsmStats.pl ${Name} ${name} > ${destDir}/asmStats.html
	sed -e "s/hgdownload.soe/hgdownload-test.gi/g; s/index.html/testIndex.html/; s#/asmStats#/testAsmStats#;" ${destDir}/asmStats.html > ${destDir}/testAsmStats.html
	chmod +x ${destDir}/asmStats.html ${destDir}/testAsmStats.html

trackData::
	${toolsDir}/trackData.pl ${Name} ${name} > ${destDir}/trackData.html
	chmod +x ${destDir}/trackData.html

mkGenomes::
	${toolsDir}/mkGenomes.pl ${Name} ${name} > ${destDir}/genomes.txt

${destDir}/hub.txt: ${srcDir}/hub.txt
	rm -f ${destDir}/hub.txt
	cp -p ${srcDir}/hub.txt ${destDir}/hub.txt
	sed -e 's/index.html/testIndex.html/;' ${srcDir}/hub.txt > ${destDir}/testHub.txt

${destDir}/groups.txt: ${toolsDir}/groups.txt
	rm -f ${destDir}/groups.txt
	cp -p ${toolsDir}/groups.txt ${destDir}/groups.txt

clean::
	rm -f ${destDir}/hub.txt
	rm -f ${destDir}/testHub.txt
	rm -f ${destDir}/groups.txt
	rm -f ${destDir}/genomes.txt
	rm -f ${destDir}/index.html
	rm -f ${destDir}/testIndex.html
	rm -f ${destDir}/asmStats.html
	rm -f ${destDir}/testAsmStats.html
