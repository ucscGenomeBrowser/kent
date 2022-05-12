
accession = GCA_009914755.4
asmId = ${accession}_T2T-CHM13v2.0
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
html/GCA_009914755.4_T2T-CHM13v2.0.refSeqComposite.html \
html/GCA_009914755.4_T2T-CHM13v2.0.xenoRefGene.html

# tracks not used

notPushed = html/GCA_009914755.4_T2T-CHM13v2.0.augustus.html \
html/GCA_009914755.4_T2T-CHM13v2.0.tanDups.html

# files used to construct a useOneFile hub.txt
hubTxt = hub/GCA_009914755.4_T2T-CHM13v2.0.hub.txt \
hub/GCA_009914755.4_T2T-CHM13v2.0.genomes.txt \
trackDb/GCA_009914755.4_T2T-CHM13v2.0.trackDb.txt

alpha:: singleFileHub
	@printf "# setting up hub files for hgdownload-test staging\n"
	make stageFiles
	make test

stageFiles: $(htmlFiles:%=%stageFiles)
%stageFiles:
	@rm -f "${stageDir}/$*"
	@cp --remove-destination "$*" "${stageDir}/$*"

# verify the hub is functioning OK on hgwdev/genome-test from hgdownload-test
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
	@printf "# verify login to qateam@hgdownload functions, should see the date\n"
	@ssh qateam@hgdownload.soe.ucsc.edu date 2>&1 | grep -v "X11 forwarding"
	@printf "# login to qateam@hgdownload OK\n"

# push the hub to hgdownload
public: verifyLogin
	@printf "pushing to hgdownload\n" 1>&2
	rsync --exclude="hub.txt" --exclude="download.hub.txt" --stats -a -L -P "${pushFromDir}/" "qateam@hgdownload.soe.ucsc.edu:${pushToDir}/" 2>&1 | grep -v "X11 forwarding request" || true
	rsync --stats -a -L -P "${pushFromDir}/download.hub.txt" "qateam@hgdownload.soe.ucsc.edu:${pushToDir}/hub.txt" 2>&1 | grep -v "X11 forwarding request" || true
	@printf "# successful rsync\n" 1>&2
	make verify

clean::
	rm -f hub.txt

singleFileHub: ${hubTxt}
	@rm -f hub.txt
	@grep -v genomesFile hub/${asmId}.hub.txt > hub.txt
	@printf "useOneFile on\n\n" >> hub.txt
	@grep -v trackDb hub/${asmId}.genomes.txt >> hub.txt
	@echo >> hub.txt
	@cat trackDb/${asmId}.trackDb.txt >> hub.txt
	@cp --remove-destination hub.txt ${stageDir}/${asmId}.singleFile.hub.txt
	@cp --remove-destination hub.txt ${stageDir}/${asmId}.download.hub.txt
	@cp --remove-destination hub/${asmId}.groups.txt ${stageDir}/
	@cp --remove-destination hub/${asmId}.genomes.txt ${stageDir}/
	cp --remove-destination hub/${asmId}.hub.txt ${stageDir}/
	@rm -f hub.txt
