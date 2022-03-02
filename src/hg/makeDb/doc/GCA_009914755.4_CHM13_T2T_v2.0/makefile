asmid = GCA_009914755.4_CHM13_T2T_v2.0
asmBuildDir = /hive/data/genomes/asmHubs/genbankBuild/GCA/009/914/755/${asmid}
trackDataDir = ${asmDataDir}/trackData
bbiDir = ${asmBuildDir}/bbi
htmlDir = ${asmBuildDir}/html
stageUrl = https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt

userTrackDbFile = ${asmBuildDir}/${asmid}.userTrackDb.txt

tracks = proseq cytoBandMapped sedefSegDups rdnaModel

tracksWithHtml = $(filter-out cytoBandMapped,${tracks})

.SECONDARY:

all: ${userTrackDbFile} bbiLinks htmlCopy
	cd ${asmBuildDir} && ./doTrackDb.bash
	hubCheck ${stageUrl}

${userTrackDbFile}: ${tracks:%=trackDb/%.trackDb.txt}
	cat $^ > $@

# make bbi links for directories
.PHONY: bbiLinks bbiLink_%
bbiLinks: ${tracks:%=bbiLink_%}

# validate a symbolic link points to the right place and is not broken
bbiLink_%: | ${bbiDir}/${asmid}.%
	test "$$(readlink ${bbiDir}/${asmid}.$*)" == "../trackData/$*"

# create symlink if it doesn't exist
${bbiDir}/${asmid}.%:
	ln -sf ../trackData/$* ${bbiDir}/${asmid}.$*

# copy HTML to right locations
.PHONY: htmlCopy
htmlCopy: ${tracksWithHtml:%=${htmlDir}/${asmid}.%.html}

${htmlDir}/${asmid}.%.html: html/%.html
	cp -f $< $@
