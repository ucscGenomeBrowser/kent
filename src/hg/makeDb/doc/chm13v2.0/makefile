
asmId = GCA_009914755.4_T2T-CHM13v2.0
accession = GCA_009914755.4
stageDir = /hive/data/genomes/asmHubs/genbankBuild/GCA/009/914/755/${asmId}
pushFromDir = /hive/data/genomes/asmHubs//GCA/009/914/755/${accession}
pushToDir = /mirrordata/hubs/GCA/009/914/755/${accession}

# default set of hub track html description files

htmlFiles = html/GCA_009914755.4_T2T-CHM13v2.0.assembly.html \
html/GCA_009914755.4_T2T-CHM13v2.0.chainNet.html \
html/GCA_009914755.4_T2T-CHM13v2.0.cpgIslands.html \
html/GCA_009914755.4_T2T-CHM13v2.0.description.html \
html/GCA_009914755.4_T2T-CHM13v2.0.gc5Base.html \
html/GCA_009914755.4_T2T-CHM13v2.0.repeatMasker.html \
html/GCA_009914755.4_T2T-CHM13v2.0.simpleRepeat.html \
html/GCA_009914755.4_T2T-CHM13v2.0.windowMasker.html \
html/GCA_009914755.4_T2T-CHM13v2.0.xenoRefGene.html

# these html files come from the T2T porting project, not maintained here
outsideSource = GCA_009914755.4_T2T-CHM13v2.0.catLiftOffGenes.html \
GCA_009914755.4_T2T-CHM13v2.0.catLiftOffGenesV1.html \
GCA_009914755.4_T2T-CHM13v2.0.censat.html \
GCA_009914755.4_T2T-CHM13v2.0.clinVar20220313.html \
GCA_009914755.4_T2T-CHM13v2.0.dbSNP155.html \
GCA_009914755.4_T2T-CHM13v2.0.gwasSNPs2022-03-08.html \
GCA_009914755.4_T2T-CHM13v2.0.hgCactus.html \
GCA_009914755.4_T2T-CHM13v2.0.hgLiftOver.html \
GCA_009914755.4_T2T-CHM13v2.0.hgUnique.html \
GCA_009914755.4_T2T-CHM13v2.0.proseq.html \
GCA_009914755.4_T2T-CHM13v2.0.rdnaModel.html \
GCA_009914755.4_T2T-CHM13v2.0.rnaseq.html \
GCA_009914755.4_T2T-CHM13v2.0.sedefSegDups.html

# tracks not used, and userTrackDb comes from T2T porting project

notPushed = html/GCA_009914755.4_T2T-CHM13v2.0.augustus.html \
html/GCA_009914755.4_T2T-CHM13v2.0.tanDups.html \
trackDb/GCA_009914755.4_T2T-CHM13v2.0.userTrackDb.txt

# files used to construct a useOneFile hub.txt
hubTxt = hub/GCA_009914755.4_T2T-CHM13v2.0.hub.txt \
hub/GCA_009914755.4_T2T-CHM13v2.0.genomes.txt \
trackDb/GCA_009914755.4_T2T-CHM13v2.0.trackDb.txt

alpha:: singleFileHub
	@printf "# setting up hub files for hgdownload-test staging\n"
	make stageFiles
	make test

# 	@printf "# %s %s\n" "$*" "${stageDir}"

stageFiles: $(htmlFiles:%=%stageFiles)
%stageFiles:
	@rm -f "${stageDir}/$*"
	@cp --remove-destination -p "$*" "${stageDir}/$*"

# verify the hub is functioning OK on hgwdev/genome-test from hgdownload-test
# can't yet use -checkSettings with hubCheck - there are some warnings there
# that cause this test to fail
test:
	@printf "# verify hub is functioning from hgdownload-test.gi.ucsc.edu\n"
	hubCheck "https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt"
	@printf "hubCheck OK\n"
	@printf "track count: %d tracks via API list tracks\n" "`curl -L "https://genome-test.gi.ucsc.edu/list/tracks?genome=GCA_009914755.4;trackLeavesOnly=1;hubUrl=https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt"  2> /dev/null | python -mjson.tool | egrep ": {$$"  | tr -d '"' | sed -e 's/^ \+//; s/ {//;' | xargs echo | wc -w`"
	@printf "should be more than 70 tracks\n"


# verify the hub is functioning OK from hgdownload.soe.ucsc.edu
verify:
	@printf "# verify hub is functioning from hgdownload.soe.ucsc.edu\n"
	hubCheck "https://hgdownload.soe.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt"
	@printf "hubCheck OK\n"
	@printf "track count: %d tracks via API list tracks\n" "`curl -L "https://apibeta.soe.ucsc.edu/list/tracks?genome=GCA_009914755.4;trackLeavesOnly=1;hubUrl=https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt"  2> /dev/null | python -mjson.tool | egrep ": {$$"  | tr -d '"' | sed -e 's/^ \+//; s/ {//;' | xargs echo | wc -w`"
	@printf "should be more than 70 tracks\n"

# user must have login permissions as qateam to hgdownload
verifyLogin:
	@ssh qateam@hgdownload.soe.ucsc.edu date

# push the hub to hgdownload
public: verifyLogin
	@printf "pushing to hgdownload\n" 1>&2
	rsync --delete --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${pushFromDir}/" "qateam@hgdownload.soe.ucsc.edu:${pushToDir}/" 2>&1 | grep -v "X11 forwarding request" || true
	rsync --stats -a -L -P "${pushFromDir}/download.hub.txt" "qateam@hgdownload.soe.ucsc.edu:${pushToDir}/hub.txt" 2>&1 | grep -v "X11 forwarding request" || true
	@printf "# successful rsync\n" 1>&2


clean::
	rm -f hub.txt

singleFileHub: ${hubTxt}
	@rm -f hub.txt
	@grep -v genomesFile hub/${asmId}.hub.txt > hub.txt
	@printf "useOneFile on\n\n" >> hub.txt
	@grep -v trackDb hub/${asmId}.genomes.txt >> hub.txt
	@echo >> hub.txt
	@cat trackDb/${asmId}.trackDb.txt >> hub.txt
	@cp --remove-destination -p hub.txt ${stageDir}/${asmId}.singleFile.hub.txt
	@cp --remove-destination -p hub.txt ${stageDir}/${asmId}.download.hub.txt
	@cp --remove-destination -p hub/${asmId}.groups.txt ${stageDir}/
	@cp --remove-destination -p hub/${asmId}.genomes.txt ${stageDir}/
	cp --remove-destination -p hub/${asmId}.hub.txt ${stageDir}/
	cp --remove-destination -p trackDb/${asmId}.trackDb.txt ${stageDir}/
