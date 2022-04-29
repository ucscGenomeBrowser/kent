SHELL = /bin/bash

ncbiAcc = GCA_009914755.4
asmName= T2T-CHM13v2.0
asmId = ${ncbiAcc}_${asmName}
asmBuildDir = /hive/data/genomes/asmHubs/genbankBuild/GCA/009/914/755/${asmId}
asmStageDir = /hive/data/genomes/asmHubs/GCA/009/914/755/${ncbiAcc}
trackDataDir = ${asmBuildDir}/trackData
bbiBuildDir = ${asmBuildDir}/bbi
htmlStageDir = ${asmStageDir}/html
liftOverDataDir = ${asmBuildDir}/liftOver
liftOverStageDir = ${asmStageDir}/liftOver
geneDataDir = ${trackDataDir}/catLiftOffGenesV1
geneStageDir = ${asmStageDir}/genes

stageUrl = https://hgdownload-test.gi.ucsc.edu/hubs/GCA/009/914/755/GCA_009914755.4/hub.txt

userTrackDbFile = ${asmBuildDir}/${asmId}.userTrackDb.txt

tracks = proseq rnaseq cytoBandMapped sedefSegDups rdnaModel catLiftOffGenesV1 hgLiftOver \
         censat hgUnique clinVar20220313 gwasSNPs2022-03-08 dbSNP155 microsatellites \
         sgdpCopyNumber encode t2tRepeatMasker

# snakes needs chromAlias fix:
# hgCactus

tracksWithHtml = $(filter-out cytoBandMapped,${tracks})

hgLiftOverFiles = chm13v2-hg19.over.chain.gz chm13v2-hg38.over.chain.gz hg19-chm13v2.over.chain.gz hg38-chm13v2.over.chain.gz

catGxfFiles = catLiftOffGenesV1.gff3.gz catLiftOffGenesV1.gtf.gz catLiftOffGenesV1.rna.fa.gz catLiftOffGenesV1.protein.fa.gz

.SECONDARY:
MAKEFLAGS += --check-symlink-times

#  not hubCheck -checkSettings currently has false positives, so not using
all: ${userTrackDbFile} bbiInstall htmlInstall liftOverInstall geneInstall
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

${htmlStageDir}/${asmId}.%.html: html/%.html
	@mkdir -p $(dir $@)
	cp -f $< $@

${htmlStageDir}/%.png: html/%.png
	@mkdir -p $(dir $@)
	cp -f $< $@

# install liftOver files
liftOverInstall: ${hgLiftOverFiles:%=${liftOverStageDir}/%}

${liftOverStageDir}/%: ${liftOverDataDir}/%
	@mkdir -p $(dir $@)
	ln -sf $< $@

# install gene downloads
geneInstall: ${catGxfFiles:%=${geneStageDir}/%}

${geneStageDir}/%: ${geneDataDir}/%
	@mkdir -p $(dir $@)
	ln -sf $$(realpath --relative-to=${geneStageDir} $<) $@


clean:
	rm -f ${userTrackDbFile}
