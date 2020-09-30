# generic makefile to construct the index pages and symlinks
# for any assembly hub
#
# will be included by those individual build directories with the
# following variables defined to customize the resulting files:
#
# destDir, srcDir, orderList, indexName, testIndexName,
# statsName, testStatsName, dataName, testDataName, genomesTxt, hubFile
# testHubFile, Name and name

toolsDir=${HOME}/kent/src/hg/makeDb/doc/asmHubs

all:: makeDirs mkGenomes symLinks hubIndex asmStats trackData hubTxt groupsTxt

makeDirs:
	mkdir -p ${destDir}

mkGenomes::
	${toolsDir}/mkGenomes.pl ${orderList} > ${destDir}/${genomesTxt}.txt

symLinks::
	${toolsDir}/mkSymLinks.pl ${orderList}

hubIndex::
	${toolsDir}/mkHubIndex.pl ${Name} ${name} ${defaultAssembly} ${orderList} | sed -e 's#${name}/hub.txt#${name}/${hubFile}.txt#;' > ${destDir}/${indexName}.html
	sed -e "s#genome.ucsc.edu/h/#genome-test.gi.ucsc.edu/h/#g; s/hgdownload.soe/hgdownload-test.gi/g; s#/${indexName}.html#/${testIndexName}.html#; s#${name}/${hubFile}.txt#${name}/${testHubFile}.txt#; s/asmStats/${testStatsName}/; s#${dataName}.html#${testDataName}.html#;" ${destDir}/${indexName}.html > ${destDir}/${testIndexName}.html
	chmod +x ${destDir}/${indexName}.html ${destDir}/${testIndexName}.html

asmStats::
	rm -f ${destDir}/${statsName}.html ${destDir}/${testStatsName}.html
	${toolsDir}/mkAsmStats.pl ${Name} ${name} ${orderList} > ${destDir}/${statsName}.html
	sed -e "s#genome.ucsc.edu/h/#genome-test.gi.ucsc.edu/h/#g; s/hgdownload.soe/hgdownload-test.gi/g; s/index.html/${testIndexName}.html/; s#/${statsName}#/${testStatsName}#; s#${dataName}.html#${testDataName}.html#;" ${destDir}/${statsName}.html > ${destDir}/${testStatsName}.html
	chmod +x ${destDir}/${statsName}.html ${destDir}/${testStatsName}.html

# trackData makes different tables for the test vs. production version
# mkHubIndex.pl and mkAsmStats.pl should do this too . . .  TBD
trackData::
	${toolsDir}/trackData.pl ${Name} ${name} ${orderList} > ${destDir}/${dataName}.html
	${toolsDir}/trackData.pl -test ${Name} ${name} ${orderList} | sed -e "s#testIndex.html#${testIndexName}.html#; s#testAsmStats.html#${testStatsName}.html#;" > ${destDir}/${testDataName}.html
	chmod +x ${destDir}/${dataName}.html
	chmod +x ${destDir}/${testDataName}.html

hubTxt:
	rm -f ${destDir}/${hubFile}.txt
	sed -e "s#index.html#${indexName}.html#; s#genomes.txt#${genomesTxt}.txt#;" ${srcDir}/${hubTxtFile} > ${destDir}/${hubFile}.txt
	sed -e 's/index.html/${testIndexName}.html/; s#genomes.txt#${genomesTxt}.txt#;' ${srcDir}/${hubTxtFile} > ${destDir}/${testHubFile}.txt

# all hubs have the same set of groups, no need for any name customization
groupsTxt:
	rm -f ${destDir}/groups.txt
	cp -p ${toolsDir}/groups.txt ${destDir}/groups.txt

clean::
	rm -f ${destDir}/${hubFile}.txt
	rm -f ${destDir}/${testHubFile}.txt
	rm -f ${destDir}/groups.txt
	rm -f ${destDir}/${genomesTxt}.txt
	rm -f ${destDir}/${indexName}.html
	rm -f ${destDir}/${testIndexName}.html
	rm -f ${destDir}/${statsName}.html
	rm -f ${destDir}/${testStatsName}.html
