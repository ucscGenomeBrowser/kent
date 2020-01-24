# generic makefile to construct the index pages and symlinks
# for any of the assembly hubs:
# primates, mammals, birds, fish, vertebrate
#
# will be included by those individual build directories with the
# Name and name defined to use here

destDir=/hive/data/genomes/asmHubs/${name}
srcDir=${HOME}/kent/src/hg/makeDb/doc/${name}AsmHub
toolsDir=${HOME}/kent/src/hg/makeDb/doc/asmHubs

all:: makeDirs ${destDir}/hub.txt ${destDir}/groups.txt ${destDir}/genomes.txt \
	${destDir}/index.html ${destDir}/asmStats${Name}.html

makeDirs:
	mkdir -p ${destDir}
	${toolsDir}/mkSymLinks.pl ${Name} ${name}

${destDir}/index.html: ${toolsDir}/mkHubIndex.pl
	${toolsDir}/mkHubIndex.pl ${Name} ${name} ${defaultAssembly} > $@
	sed -e "s/hgdownload.soe/genome-test.gi/g; s#/index.html#/testIndex.html#; s#${name}/hub.txt#${name}/testHub.txt#; s/asmStats${Name}/testAsmStats${Name}/;" $@ > ${destDir}/testIndex.html
	chmod +x $@ ${destDir}/testIndex.html

${destDir}/asmStats${Name}.html: ${toolsDir}/mkAsmStats.pl
	${toolsDir}/mkAsmStats.pl ${Name} ${name} > $@
	sed -e "s/hgdownload.soe/genome-test.gi/g; s/index.html/testIndex.html/; s#/asmStats#/testAsmStats#;" $@ > ${destDir}/testAsmStats${Name}.html
	chmod +x $@ ${destDir}/testAsmStats${Name}.html

${destDir}/genomes.txt:  ${toolsDir}/mkGenomes.pl \
	${toolsDir}/${name}.asmId.commonName.tsv \
	${toolsDir}/${name}.commonName.asmId.orderList.tsv
	${toolsDir}/mkGenomes.pl ${Name} ${name} > $@

${destDir}/hub.txt: ${srcDir}/hub.txt
	rm -f ${destDir}/hub.txt
	cp -p ${srcDir}/hub.txt ${destDir}/hub.txt
	sed -e 's/index.html/testIndex.html/;' ${srcDir}/hub.txt > ${destDir}/testHub.txt

${destDir}/groups.txt: ${toolsDir}/groups.txt
	rm -f ${destDir}/groups.txt
	cp -p ${toolsDir}/groups.txt ${destDir}/groups.txt
