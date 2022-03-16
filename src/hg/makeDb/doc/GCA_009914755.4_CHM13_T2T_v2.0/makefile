ncbiAcc = GCA_009914755.4
asmId = ${ncbiAcc}_CHM13_T2T_v2.0
asmBuildDir = /hive/data/genomes/asmHubs/genbankBuild/GCA/009/914/755/${asmId}
asmStageDir = /hive/data/genomes/asmHubs/GCA/009/914/755/${ncbiAcc}
trackDataDir = ${asmDataDir}/trackData
bbiBuildDir = ${asmBuildDir}/bbi
htmlBuildDir = ${asmBuildDir}/html
htmlStageDir = ${asmStageDir}/html
stageUrl = https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt

userTrackDbFile = ${asmBuildDir}/${asmId}.userTrackDb.txt

tracks = proseq rnaseq cytoBandMapped sedefSegDups rdnaModel catLiftOffGenes

tracksWithHtml = $(filter-out cytoBandMapped,${tracks})

.SECONDARY:

all: ${userTrackDbFile} bbiInstall htmlInstall
	cd ${asmBuildDir} && ./doTrackDb.bash
	hubCheck ${stageUrl}

# ensure spaces separate the stanzas
${userTrackDbFile}: ${tracks:%=trackDb/%.trackDb.txt}
	awk 'FNR==1{print ""} {print $$0}'  $^ > $@

# make bbi links for directories
.PHONY: bbiInstall bbiInstall_%
bbiInstall: ${tracks:%=bbiInstall_%}

# validate a symbolic link points to the right place and is not broken
bbiInstall_%: | ${bbiBuildDir}/${asmId}.%
	test "$$(readlink ${bbiBuildDir}/${asmId}.$*)" == "../trackData/$*"

# create symlink if it doesn't exist
${bbiBuildDir}/${asmId}.%:
	ln -sf ../trackData/$* ${bbiBuildDir}/${asmId}.$*

# copy HTML to build and link to the stage dir
.PHONY: htmlInstall
htmlInstall: ${tracksWithHtml:%=htmlCheck_%}

htmlCheck_%: | ${htmlStageDir}/${asmId}.%.html
	test "$$(readlink ${htmlStageDir}/${asmId}.$*.html)" == "${htmlBuildDir}/${asmId}.$*.html"

${htmlStageDir}/${asmId}.%.html: ${htmlBuildDir}/${asmId}.%.html
	ln -sf $< $@

${htmlBuildDir}/${asmId}.%.html: html/%.html
	cp -f $< $@
