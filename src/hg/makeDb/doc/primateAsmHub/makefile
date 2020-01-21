
Name="Primates"
name="primates"
destDir=/hive/data/genomes/asmHubs/${name}
srcDir=${HOME}/kent/src/hg/makeDb/doc/primateAsmHub

all:: makeDirs ${destDir}/hub.txt ${destDir}/groups.txt ${destDir}/genomes.txt \
	${destDir}/index.html ${destDir}/asmStats${Name}.html

makeDirs:
	mkdir -p ${destDir}
	${srcDir}/mkSymLinks.pl

${destDir}/index.html: ${srcDir}/mkHubIndex.pl
	${srcDir}/mkHubIndex.pl > $@
	sed -e "s/hgdownload.soe/genome-test.gi/g; s#${name}/hub.txt#${name}/testHub.txt#; s/asmStats${Name}/testAsmStats${Name}/;" $@ > ${destDir}/testIndex.html
	chmod +x $@ ${destDir}/testIndex.html

${destDir}/asmStats${Name}.html: ${srcDir}/mkAsmStats.pl
	${srcDir}/mkAsmStats.pl > $@
	sed -e "s/hgdownload.soe/genome-test.gi/g; s/index.html/testIndex.html/; s/asmStats${Name}/testAsmStats${Name}/;" $@ > ${destDir}/testAsmStats${Name}.html
	chmod +x $@ ${destDir}/testAsmStats${Name}.html

${destDir}/genomes.txt:  ${srcDir}/mkGenomes.pl \
	${srcDir}/primates.asmId.commonName.tsv \
	${srcDir}/primates.commonName.asmId.orderList.tsv
	${srcDir}/mkGenomes.pl > $@

# ${destDir}/genomes.txt:  ${destDir}/asmStats${Name}.html ${srcDir}/mkGenomes.pl ${srcDir}/mkSymLinks.sh
#	cd ${destDir} && ${srcDir}/mkSymLinks.sh

${destDir}/hub.txt: ${srcDir}/hub.txt
	rm -f ${destDir}/hub.txt
	cp -p ${srcDir}/hub.txt ${destDir}/hub.txt
	sed -e 's/index.html/testIndex.html/;' ${srcDir}/hub.txt > ${destDir}/testHub.txt

${destDir}/groups.txt: ${srcDir}/groups.txt
	rm -f ${destDir}/groups.txt
	cp -p ${srcDir}/groups.txt ${destDir}/groups.txt
