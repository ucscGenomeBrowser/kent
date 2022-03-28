SHELL = /bin/bash

ncbiAcc = GCA_009914755.4
asmId = ${ncbiAcc}_CHM13_T2T_v2.0
asmBuildDir = /hive/data/genomes/asmHubs/genbankBuild/GCA/009/914/755/${asmId}
asmStageDir = /hive/data/genomes/asmHubs/GCA/009/914/755/${ncbiAcc}
trackDataDir = ${asmBuildDir}/trackData
bbiBuildDir = ${asmBuildDir}/bbi
htmlBuildDir = ${asmBuildDir}/html
htmlStageDir = ${asmStageDir}/html
liftOverDataDir = ${asmBuildDir}/liftOver
liftOverStageDir = ${asmStageDir}/liftOver

stageUrl = https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt

userTrackDbFile = ${asmBuildDir}/${asmId}.userTrackDb.txt

tracks = proseq rnaseq cytoBandMapped sedefSegDups rdnaModel catLiftOffGenesV1 hgLiftOver
tracksWithHtml = $(filter-out cytoBandMapped,${tracks})

hgLiftOverFiles = chm13v2-hg19.chain.gz chm13v2-hg38.chain.gz hg19-chm13v2.chain.gz hg38-chm13v2.chain.gz

.SECONDARY:
MAKEFLAGS += --check-symlink-times

all: ${userTrackDbFile} bbiInstall htmlInstall liftOverInstall
	cd ${asmBuildDir} && ./doTrackDb.bash
	hubCheck ${stageUrl}

# ensure spaces separate the stanzas
${userTrackDbFile}: ${tracks:%=trackDb/%.trackDb.txt}
	awk 'FNR==1{print ""} {print $$0}'  $^ > $@

# make bbi links for directories
.PHONY: bbiInstall bbiInstall_%
bbiInstall: ${tracks:%=${bbiBuildDir}/${asmId}.%}

# create symlink if it doesn't exist
${bbiBuildDir}/${asmId}.%:
	ln -sf ../trackData/$* ${bbiBuildDir}/${asmId}.$*

# copy HTML to build and link to the stage dir
.PHONY: htmlInstall
htmlInstall: ${tracksWithHtml:%=${htmlStageDir}/${asmId}.%.html}

${htmlStageDir}/${asmId}.%.html: ${htmlBuildDir}/${asmId}.%.html
	@mkdir -p $(dir $@)
	ln -sf $< $@

${htmlBuildDir}/${asmId}.%.html: html/%.html
	@mkdir -p $(dir $@)
	cp -f $< $@

# install liftOver files
liftOverInstall: ${hgLiftOverFiles:%=${liftOverStageDir}/%}

${liftOverStageDir}/%: ${liftOverDataDir}/%
	@mkdir -p $(dir $@)
	ln -sf $< $@
