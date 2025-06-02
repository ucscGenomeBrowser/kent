# generic makefile to construct the index pages and symlinks
# for any assembly hub
#
# will be included by those individual build directories with the
# following variables defined to customize the resulting files:
#
# destDir, srcDir, orderList, indexName, testIndexName,
# statsName, testStatsName, dataName, testDataName, genomesTxt, hubFile
# testHubFile, Name and name

# the .PHONY will make sure these targets run even if there happens to be
#    a file by the same name existing.  These rules don't make these files,
#    they are just procedures to run.

.PHONY: sanityCheck makeDirs mkJson mkGenomes symLinks hubIndex asmStats trackData groupsTxt

toolsDir=${HOME}/kent/src/hg/makeDb/doc/asmHubs
htdocsHgDownload=/usr/local/apache/htdocs-hgdownload
hubsDownload=${htdocsHgDownload}/hubs/${name}
asmHubSrc=/hive/data/genomes/asmHubs/${name}
downloadDest1=hgdownload1.soe.ucsc.edu
downloadDest2=hgdownload2.soe.ucsc.edu
downloadDest3=hgdownload3.gi.ucsc.edu
# 2025-04-06 hgdownload1.soe.ucsc.edu has address 128.114.119.163
# 2024-02-06 hgdownload2.gi.ucsc.edu has address 128.114.198.53
# 2025-04-06 hgdownload3.gi.ucsc.edu has address 169.233.10.12

all:: sanityCheck makeDirs mkJson mkGenomes symLinks hubIndex asmStats trackData groupsTxt

makeDirs:
	mkdir -p ${destDir}

sanityCheck:
	@goodBad=$$(cut -d'_' -f1-2 ${orderList} | sort | uniq -c | awk '$$1 > 1' | wc -l); \
	if [ $$goodBad -ne 0 ]; then \
	    tsvFile=$$(basename ${orderList}); \
	    echo "ERROR: duplicate accession in '$$tsvFile'"; \
	    cut -d'_' -f1-2 ${orderList} | sort | uniq -c | awk '$$1 > 1'; \
	    exit 255; \
	fi

sshKeyDownload:
	ssh -o PasswordAuthentication=no qateam@${downloadDest1} date
	ssh -o PasswordAuthentication=no qateam@${downloadDest3} date

sshKeyDynablat:
	ssh -o PasswordAuthentication=no qateam@dynablat-01 date

sshKeyCheck: sshKeyDownload sshKeyDynablat
	@printf "# ssh keys to hgdownload and dynablat-01 are good\n"

mkJson::
	if [ "$(name)" = "VGP" ]; then \
	sort -u *.orderList.tsv | ${toolsDir}/tsvToJson.py stdin > ${destDir}/assemblyList.json 2> ${name}.jsonData.txt; \
	else \
	${toolsDir}/tsvToJson.py ${orderList} > ${destDir}/assemblyList.json 2> ${name}.jsonData.txt; \
        fi

# mkGenomes needs symLinks to run before mkGenomes runs, and then
# the second symLinks after mkGenomes uses business created by mkGenomes

mkGenomes::
	@printf "# starting mkGenomes " 1>&2
	${toolsDir}/mkSymLinks.pl ${orderList}
	@date "+%s %F %T" 1>&2
	@rm -f hasChainNets.txt
	${toolsDir}/mkGenomes.pl dynablat-01 4040 ${orderList} > /dev/null
	@printf "# finished mkGenomes " 1>&2
	@date "+%s %F %T" 1>&2

# temporary mkGenomes to get the single file genomes.txt made on a special
# one time only static list
staticMkGenomes:
	@printf "# starting staticMkGenomes " 1>&2
	@date "+%s %F %T" 1>&2
	@rm -f hasChainNets.txt
	${toolsDir}/mkGenomes.pl dynablat-01 4040 ${orderList} > ${destDir}/${genomesTxt}.txt
	rm -f ${destDir}/download.${genomesTxt}.txt
	cp -p ${destDir}/${genomesTxt}.txt ${destDir}/download.${genomesTxt}.txt
	@printf "# finished staticMkGenomes " 1>&2
	@date "+%s %F %T" 1>&2

symLinks::
	${toolsDir}/mkSymLinks.pl ${orderList}
	@[ -d ${hubsDownload} ] && true || mkdir ${hubsDownload}
	@for html in ${indexName} ${statsName} ${dataName} ; do \
[ -L ${hubsDownload}/$${html}.html ] && true || ln -s ${asmHubSrc}/$${html}.html ${hubsDownload} ; \
[ -L ${hubsDownload}/download.$${html}.html ] && true || ln -s ${asmHubSrc}/download.$${html}.html ${hubsDownload} ; \
done
	@for txt in groups hub genomes download.genomes ; do \
[ -L ${hubsDownload}/$${txt}.txt ] && true || ln -s ${asmHubSrc}/$${txt}.txt ${hubsDownload} ; \
done
	@for json in assemblyList ; do \
[ -L ${hubsDownload}/$${json}.json ] && true || ln -s ${asmHubSrc}/$${json}.json ${hubsDownload} ; \
done

hubIndex::
	rm -f ${destDir}/${testIndexName}.html ${destDir}/${indexName}.html ${destDir}/download.${indexName}.html
	${toolsDir}/mkHubIndex.pl ${Name} ${name} ${defaultAssembly} ${orderList} | sed -e 's#${name}/hub.txt#${name}/${hubFile}.txt#;' > ${destDir}/download.${indexName}.html
	sed -e "s#genome.ucsc.edu/h/#genome-test.gi.ucsc.edu/h/#g; s/hgdownload.soe/hgdownload-test.gi/g; s#genome.ucsc.edu#genome-test.gi.ucsc.edu#;" ${destDir}/download.${indexName}.html > ${destDir}/${indexName}.html
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

indexPages: mkJson hubIndex asmStats trackData
	echo indexPages done

### obsolete, these hub.txt files are now static 2024-10-23
hubTxt:
	rm -f ${destDir}/${testHubFile}.txt ${destDir}/${hubFile}.txt
	sed -e "s#index.html#${indexName}.html#; s#genomes.txt#${genomesTxt}.txt#;" ${srcDir}/${hubTxtFile} > ${destDir}/${hubFile}.txt

# all hubs have the same set of groups, no need for any name customization
groupsTxt:
	rm -f ${destDir}/groups.txt
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

sendDownload:: sshKeyCheck
	${toolsDir}/mkSendList.pl ${orderList} | while read F; do \
	  ((N=N+1)); printf "### count %5d\t%s\t%s\n" $${N} $${F} "`date '+%F %T %s'`"; ${toolsDir}/sendToHgdownload.sh $$F < /dev/null; done
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/assemblyList.json \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/assemblyList.json \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/groups.txt \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/groups.txt \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/${hubFile}.txt \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/${hubFile}.txt \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${indexName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${indexName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${indexName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${indexName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${statsName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${statsName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${statsName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${statsName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${dataName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${dataName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${dataName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${dataName}.html

# no longer sending genomes.txt file 2024-10-23 - becomes static
obsolete:
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${genomesTxt}.txt \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${genomesTxt}.txt
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${genomesTxt}.txt \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${genomesTxt}.txt

verifyTestDownload:
	${toolsDir}/verifyOnDownload.sh api-test.gi.ucsc.edu ${orderList}

verifyDownload:
	${toolsDir}/verifyOnDownload.sh apibeta.soe.ucsc.edu ${orderList}

verifyDynamicBlat:
	grep -v "^#" ${orderList} | cut -d'_' -f1-2 | while read asmId; do \
	  ${toolsDir}/testDynBlat.sh $$asmId < /dev/null; done

sendIndexes::
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/assemblyList.json \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/assemblyList.json \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${indexName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${indexName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${indexName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${indexName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${statsName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${statsName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${statsName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${statsName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${dataName}.html \
		qateam@${downloadDest1}:/mirrordata/hubs/${name}/${dataName}.html
	rsync -L -a -P \
  /usr/local/apache/htdocs-hgdownload/hubs/${name}/download.${dataName}.html \
		qateam@${downloadDest3}:/mirrordata/hubs/${name}/${dataName}.html
