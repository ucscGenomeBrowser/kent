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
htdocsHgDownload=/usr/local/apache/htdocs-hgdownload
hubsDownload=${htdocsHgDownload}/hubs/${name}
asmHubSrc=/hive/data/genomes/asmHubs/${name}

all:: makeDirs mkGenomes symLinks hubIndex asmStats trackData hubTxt groupsTxt

makeDirs:
	mkdir -p ${destDir}

mkGenomes::
	${toolsDir}/mkGenomes.pl localhost 4040 ${orderList} > ${destDir}/${genomesTxt}.txt
	${toolsDir}/mkGenomes.pl hgwdev 4040 ${orderList} > ${destDir}/download.${genomesTxt}.txt

symLinks::
	${toolsDir}/mkSymLinks.pl ${orderList}
	@[ -d ${hubsDownload} ] && true || mkdir ${hubsDownload}
	@for html in index asmStats trackData ; do \
[ -L ${hubsDownload}/$${html}.html ] && true || ln -s ${asmHubSrc}/$${html}.html ${hubsDownload} ; \
[ -L ${hubsDownload}/download.$${html}.html ] && true || ln -s ${asmHubSrc}/download.$${html}.html ${hubsDownload} ; \
done
	@for txt in groups hub genomes download.genomes ; do \
[ -L ${hubsDownload}/$${txt}.txt ] && true || ln -s ${asmHubSrc}/$${txt}.txt ${hubsDownload} ; \
done

hubIndex::
	rm -f ${destDir}/${testIndexName}.html ${destDir}/${indexName}.html ${destDir}/download.${indexName}.html
	${toolsDir}/mkHubIndex.pl ${Name} ${name} ${defaultAssembly} ${orderList} | sed -e 's#${name}/hub.txt#${name}/${hubFile}.txt#;' > ${destDir}/download.${indexName}.html
	sed -e "s#genome.ucsc.edu/h/#genome-test.gi.ucsc.edu/h/#g; s/hgdownload.soe/hgdownload-test.gi/g;" ${destDir}/download.${indexName}.html > ${destDir}/${indexName}.html
	chmod +x ${destDir}/${indexName}.html ${destDir}/download.${indexName}.html

asmStats::
	rm -f ${destDir}/download.${statsName}.html ${destDir}/${statsName}.html ${destDir}/${testStatsName}.html
	${toolsDir}/mkAsmStats.pl ${Name} ${name} ${orderList} > ${destDir}/download.${statsName}.html
	sed -e "s#genome.ucsc.edu/h/#genome-test.gi.ucsc.edu/h/#g; s/hgdownload.soe/hgdownload-test.gi/g;" ${destDir}/download.${statsName}.html > ${destDir}/${statsName}.html
	chmod +x ${destDir}/${statsName}.html ${destDir}/download.${statsName}.html

# trackData makes different tables for the test vs. production version
# mkHubIndex.pl and mkAsmStats.pl should do this too . . .  TBD
trackData::
	rm -f ${destDir}/${testDataName}.html ${destDir}/${dataName}.html ${destDir}/download.${dataName}.html
	${toolsDir}/trackData.pl ${Name} ${name} ${orderList} > ${destDir}/download.${dataName}.html
	${toolsDir}/trackData.pl -test ${Name} ${name} ${orderList} > ${destDir}/${dataName}.html
	chmod +x ${destDir}/${dataName}.html
	chmod +x ${destDir}/download.${dataName}.html

hubTxt:
	rm -f ${destDir}/${testHubFile}.txt ${destDir}/${hubFile}.txt
	sed -e "s#index.html#${indexName}.html#; s#genomes.txt#${genomesTxt}.txt#;" ${srcDir}/${hubTxtFile} > ${destDir}/${hubFile}.txt

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

sendDownload::
	${toolsDir}/mkSendList.pl ${orderList} | while read F; do \
	  ${toolsDir}/sendToHgdownload.sh $$F < /dev/null; done
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/groups.txt \
		qateam@hgdownload:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/${hubFile}.txt \
		qateam@hgdownload:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${indexName}.html \
		qateam@hgdownload:/mirrordata/hubs/${name}/${indexName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${statsName}.html \
		qateam@hgdownload:/mirrordata/hubs/${name}/${statsName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${dataName}.html \
		qateam@hgdownload:/mirrordata/hubs/${name}/${dataName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${genomesTxt}.txt \
		qateam@hgdownload:/mirrordata/hubs/${name}/${genomesTxt}.txt

