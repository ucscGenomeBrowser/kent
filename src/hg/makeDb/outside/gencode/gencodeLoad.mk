####
# build GENCODE tracks requires CCDS and markd python junk.
# targets:
#   all - do everything
#   fetch - download gencode
#   checkAttrs - check attribute conversion, so code can be updated to handle new biotypes
#   mkTables - create table files
#   loadTables - load tables
#   checkSanity - do some checks on the tables
#   cmpRelease - compare with previous release
#   joinerCheck - run joinerCheck on gencode tabkes
#
# can use -j n to run multiple jobs in parallel.
####

##
# make/bash robustness stuff
##
.SECONDARY:
host=$(shell hostname)
ppid=$(shell echo $$PPID)
tmpExt = ${host}.${ppid}.tmp
SHELL = bash -e
export SHELLOPTS=pipefail

##
# programs, etc
##
mach = $(shell uname -m)

##
# Release info and files from Sanger.
# BEGIN EDIT THESE EACH RELEASE
##
#db = mm10
#db = hg38
db = hg19
ifeq (${db},mm10)
    grcRefAssembly = GRCm38
    ver = M17
    prevVer = M16
    gencodeOrg = Gencode_mouse
    ftpReleaseSubdir = release_${ver}
    annGtfTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 92_38
    ensemblCDnaDb = mus_musculus_cdna_${ensemblVer}
    patchSeqs = KB469740.1 KB469738.2 JH792833.1 KB469741.1 JH792826.1 KK082443.1 KB469739.1 JH792832.1 KK082442.1 JH792831.1 KB469742.1 JH792834.1 JH792827.1 KK082441.1 JH792830.1 JH792828.1 KQ030491.1 KQ030485.1 KB469738.3 KQ030495.1 KQ030490.1 KQ030488.1 KQ030496.1 KQ030489.1 KQ030493.1 KQ030486.1 KQ030484.1 KQ030494.1 KQ030487.1 KQ030497.1 KV575238.1 KV575235.1 KB469741.2 KV575234.1 KV575237.1 KV575242.1 KV575232.1 KV575236.1 KV575233.1 KQ030492.1 JH792829.1 KV575239.1 KV575241.1 KV575240.1
else ifeq (${db},hg38)
    grcRefAssembly = GRCh38
    ver = 28
    prevVer = 27
    gencodeOrg = Gencode_human
    ftpReleaseSubdir = release_${ver}
    annGtfTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 92_38
    ensemblCDnaDb = homo_sapiens_cdna_${ensemblVer}
    patchSeqs = KN196487.1 KN538364.1 KN196473.1 KN196486.1 KN196472.1 KN196484.1 KN196485.1 KN196480.1 KN538369.1 KN196481.1 KN538362.1 KN538370.1 KN538363.1 KN538371.1 KN538360.1 KN538368.1 KN538372.1 KN538373.1 \
	KN196479.1 KN538361.1 KN196478.1 KN196477.1 KN196474.1 KN196475.1 KQ031383.1 KQ031384.1 KQ031389.1 KQ031387.1 KQ031385.1 \
	KQ458387.1 KQ090021.1 KQ458386.1 KQ458382.1 KN196482.1 KQ458383.1 KQ458385.1 KQ458388.1 KQ090015.1 KQ458384.1 KQ090016.1 KQ090014.1 KQ090024.1 KQ090028.1 KQ090018.1 KQ090026.1 KQ090025.1 KQ090022.1 KQ090027.1 \
	KN196476.1 KQ983258.1 KQ983256.1 KQ759760.1 KQ759762.1 KQ983257.1 KQ759761.1 KQ759759.1 KQ983255.1 KV575246.1 KV575249.1 KV575248.1 KV575247.1 KV880766.1 KV575244.1 KV880768.1 KV575245.1 KV575252.1 KV575243.1 \
	KV575254.1 KV575255.1 KV575259.1 KV575256.1 KV575257.1 KV575260.1 KV575258.1 KQ090019.1 KV766192.1 KV766193.1 KV766196.1 KV766195.1 KV766198.1 KV880763.1 KV575250.1 KV575251.1 KV880765.1 KV575253.1 KV880764.1 \
	KZ208909.1 KZ559107.1 KZ559111.1 KZ559106.1 KZ559110.1 KZ208904.1 KZ559105.1 KZ559113.1 KZ208924.1 KZ559112.1 KZ208914.1 KZ208917.1 KZ208915.1 KZ208905.1 KZ208922.1 KZ559104.1 KZ208906.1 KZ559114.1 KZ208919.1 \
	KZ559102.1 KZ208913.1 KZ559115.1 KZ559103.1 KV766197.1 KZ559116.1 KZ208912.1 KZ208911.1 KZ208907.1 KZ208920.1 KZ208916.1 KZ208908.1 KZ559108.1 KZ208921.1 KZ559109.1
else ifeq (${db},hg19)
    grcRefAssembly = GRCh37
    verBase = 28
    ver = ${verBase}lift37
    ftpReleaseSubdir = release_${verBase}/GRCh37_mapping
    prevVer = 27lift37
    gencodeOrg = Gencode_human
    annGtfTypeName = annotation
    ensemblVer = 74_37
    ensemblCDnaDb = homo_sapiens_cdna_${ensemblVer}
    patchSeqs =
    isBackmap = yes
else
    $(error unimplement genome database: ${db})
endif
# END EDIT THESE EACH RELEASE

rel = V${ver}
releaseUrl = ftp://ftp.ebi.ac.uk/pub/databases/gencode/${gencodeOrg}/${ftpReleaseSubdir}
dataDir = data
relDir = ${dataDir}/release_${ver}
skipPatchSeqOpts = $(foreach p,${patchSeqs},--skipSeq=${p})
annotationGtf = ${relDir}/gencode.v${ver}.${annGtfTypeName}.gtf.gz
pseudo2WayGtf = ${relDir}/gencode.v${ver}.2wayconspseudos.gtf.gz
polyAGtf = ${relDir}/gencode.v${ver}.polyAs.gtf.gz

ccdsBinDir = ~markd/compbio/ccds/ccds2/output/bin/$(mach)/opt
gencodeMakeTracks = ${ccdsBinDir}/gencodeMakeTracks
gencodeMakeAttrs = ${ccdsBinDir}/gencodeMakeAttrs
gencodeExonSupportToTable = ${ccdsBinDir}/gencodeExonSupportToTable
gencodeGxfToGenePred = ${ccdsBinDir}/gencodeGxfToGenePred
gencodePolyaGxfToGenePred = ${ccdsBinDir}/gencodePolyaGxfToGenePred
gencodeGxfToAttrs = ${ccdsBinDir}/gencodeGxfToAttrs
ensToUcscChromMap = ${ccdsBinDir}/ensToUcscChromMap
gencodeBackMapMetadataIds = ${ccdsBinDir}/gencodeBackMapMetadataIds
encodeAutoSqlDir = ${HOME}/kent/src/hg/lib/encode

##
# intermediate data not loaded into tracks
##
gencodeGp = ${dataDir}/gencode.gp
gencodeTsv = ${dataDir}/gencode.tsv
ensemblToUcscChain = ${dataDir}/ensemblToUcsc.chain

# flag indicating fetch was done
fetchDone = ${relDir}/done

##
# track and table data
##
tableDir = tables
tablePre = wgEncodeGencode

# subset track and pattern for generate genePred and track names for each subset
# obtained from gencode.v*.annotation.level_1_2.gtf, gencode.v*.annotation.level_3.gtf
tableBasic = ${tablePre}Basic${rel}
tableBasicGp = ${tableDir}/${tableBasic}.gp

tableComp = ${tablePre}Comp${rel}
tableCompGp = ${tableDir}/${tableComp}.gp

tablePseudo = ${tablePre}PseudoGene${rel}
tablePseudoGp = ${tableDir}/${tablePseudo}.gp

tableAttrs = ${tablePre}Attrs${rel}
tableAttrsTab = ${tableDir}/${tableAttrs}.tab

tableTag = ${tablePre}Tag${rel}
tableTagTab = ${tableDir}/${tableTag}.tab

# obtained from gencode.v*.2wayconspseudos.GRCh37.gtf
table2WayConsPseudo = ${tablePre}2wayConsPseudo${rel}
table2WayConsPseudoGp = ${tableDir}/${table2WayConsPseudo}.gp

# obtained from gencode.v*.polyAs.gtf
tablePolyA = ${tablePre}Polya${rel}
tablePolyAGp = ${tableDir}/${tablePolyA}.gp

# other metadata
tableGeneSourceMeta = ${relDir}/gencode.v${ver}.metadata.Gene_source.gz
tableGeneSource = ${tablePre}GeneSource${rel}
tableGeneSourceTab = ${tableDir}/${tableGeneSource}.tab

tableTranscriptSourceMeta = ${relDir}/gencode.v${ver}.metadata.Transcript_source.gz
tableTranscriptSource = ${tablePre}TranscriptSource${rel}
tableTranscriptSourceTab = ${tableDir}/${tableTranscriptSource}.tab

tableTranscriptSupportMeta = ${relDir}/gencode.v${ver}.metadata.Transcript_supporting_feature.gz
tableTranscriptSupport = ${tablePre}TranscriptSupport${rel}
tableTranscriptSupportTab = ${tableDir}/${tableTranscriptSupport}.tab

tableExonSupportMeta = ${relDir}/gencode.v${ver}.metadata.Exon_supporting_feature.gz
tableExonSupport = ${tablePre}ExonSupport${rel}
tableExonSupportTab = ${tableDir}/${tableExonSupport}.tab

tablePdbMeta = ${relDir}/gencode.v${ver}.metadata.PDB.gz
tablePdb = ${tablePre}Pdb${rel}
tablePdbTab = ${tableDir}/${tablePdb}.tab

tablePubMedMeta = ${relDir}/gencode.v${ver}.metadata.Pubmed_id.gz
tablePubMed = ${tablePre}PubMed${rel}
tablePubMedTab = ${tableDir}/${tablePubMed}.tab

tableRefSeqMeta = ${relDir}/gencode.v${ver}.metadata.RefSeq.gz
tableRefSeq = ${tablePre}RefSeq${rel}
tableRefSeqTab = ${tableDir}/${tableRefSeq}.tab

tableSwissProtMeta = ${relDir}/gencode.v${ver}.metadata.SwissProt.gz
tableTrEMBLMeta = ${relDir}/gencode.v${ver}.metadata.TrEMBL.gz
tableUniProt = ${tablePre}UniProt${rel}
tableUniProtTab = ${tableDir}/${tableUniProt}.tab

tablePolyAFeatureMeta = ${relDir}/gencode.v${ver}.metadata.PolyA_feature.gz
tablePolyAFeature = ${tablePre}PolyAFeature${rel}
tablePolyAFeatureTab = ${tableDir}/${tablePolyAFeature}.tab

tableAnnotationRemarkMeta = ${relDir}/gencode.v${ver}.metadata.Annotation_remark.gz
tableAnnotationRemark = ${tablePre}AnnotationRemark${rel}
tableAnnotationRemarkTab = ${tableDir}/${tableAnnotationRemark}.tab

tableEntrezGeneMeta = ${relDir}/gencode.v${ver}.metadata.EntrezGene.gz
tableEntrezGene = ${tablePre}EntrezGene${rel}
tableEntrezGeneTab = ${tableDir}/${tableEntrezGene}.tab

tableTranscriptionSupportLevelData = ${dataDir}/gencode.v${ver}.transcriptionSupportLevel.tab
tableTranscriptionSupportLevel  = ${tablePre}TranscriptionSupportLevel${rel}
tableTranscriptionSupportLevelTab = ${tableDir}/${tableTranscriptionSupportLevel}.tab

genePredExtTables = ${tableBasic} ${tableComp} ${tablePseudo}
genePredTables =
tabTables = ${tableAttrs} ${tableTag} ${tableGeneSource} \
	    ${tableTranscriptSource} ${tableTranscriptSupport} \
	    ${tablePdb} ${tablePubMed} ${tableRefSeq} ${tableUniProt} \
	    ${tableAnnotationRemark} ${tableEntrezGene}  ${tableTranscriptionSupportLevel}
ifneq (${isBackmap}, yes)
    # these are not included in backmap releases
    genePredTables = ${table2WayConsPseudo}
    genePredExtTables += ${tablePolyA}
    tabTables += ${tableExonSupport}
endif
allTables = ${genePredExtTables} ${genePredTables} ${tabTables}

# directory for flags indicating tables were loaded
loadedDir = loaded

# directory for output and flags for sanity checks
checkDir = check

all: fetch mkTables loadTables checkSanity cmpRelease listTables


##
# fetch release, this doesn't get subdirectories so as not to copy the lift releases
##
fetch: ${fetchDone}
${fetchDone}:
	@mkdir -p $(dir $@) ${dataDir}
	wget -nv --cut-dirs=4 --directory-prefix=${relDir} -np "${releaseUrl}/*"
	chmod a-w ${relDir}/*
	touch $@

##
# dependencies for files from release
##
${annotationGtf}: ${fetchDone}
${pseudo2WayGtf}: ${fetchDone}
${polyAGtf}: ${fetchDone}
${tableGeneSourceMeta}: ${fetchDone}
${tableTranscriptSourceMeta}: ${fetchDone}
${tableTranscriptSupportMeta}: ${fetchDone}
${tableExonSupportMeta}: ${fetchDone}
${tablePdbMeta}: ${fetchDone}
${tablePubMedMeta}: ${fetchDone}
${tableRefSeqMeta}: ${fetchDone}
${tableSwissProtMeta}: ${fetchDone}
${tableTrEMBLMeta}: ${fetchDone}
${tablePolyAFeatureMeta}: ${fetchDone}
${tableAnnotationRemarkMeta}: ${fetchDone}
${tableEntrezGeneMeta}: ${fetchDone}

##
# primary table files
##
mkTables: ${genePredExtTables:%=${tableDir}/%.gp} ${genePredTables:%=${tableDir}/%.gp} \
	  ${tabTables:%=${tableDir}/%.tab}

# grab subset name from file pattern (this is what tr command below does)
${tableDir}/${tablePre}%${rel}.gp: ${gencodeGp} ${gencodeTsv}
	@mkdir -p $(dir $@)
	${gencodeMakeTracks}  $$(echo $* | tr A-Z a-z) ${gencodeGp} ${gencodeTsv} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableTagTab}: ${tableAttrsTab}
${tableAttrsTab}: ${gencodeGp} ${gencodeTsv}
	@mkdir -p $(dir $@)
	${gencodeMakeAttrs} ${gencodeGp} ${gencodeTsv} $@.${tmpExt} ${tableTagTab}
	mv -f $@.${tmpExt} $@

${table2WayConsPseudoGp}: ${pseudo2WayGtf}
	@mkdir -p $(dir $@)
	zcat $< | tawk '$$3=="transcript"{$$3 = "exon"} {print $$0}' | gtfToGenePred -ignoreGroupsWithoutExons stdin $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tablePolyAGp}: ${polyAGtf} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodePolyaGxfToGenePred} ${skipPatchSeqOpts} $< ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableUniProtTab}: ${tableSwissProtMeta} ${tableTrEMBLMeta} ${gencodeTsv}
	@mkdir -p $(dir $@)
	((${metaFilterCmdGz} ${tableSwissProtMeta} | tawk '{print $$0,"SwissProt"}') && (${metaFilterCmdGz}  ${tableTrEMBLMeta} | tawk '{print $$0,"TrEMBL"}')) | sort -k 1,1 > $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${ensemblToUcscChain}:
	@mkdir -p $(dir $@)
	${ensToUcscChromMap} ${ensemblCDnaDb} ${grcRefAssembly} ${db} /dev/stdout | pslSwap stdin stdout | pslToChain stdin $@.${tmpExt}
	mv -f $@.${tmpExt} $@

# other tab files, just copy to name following convention to make load rules
# work
ifeq (${isBackmap}, yes)
   metaFilterCmd = ${gencodeBackMapMetadataIds} ${gencodeTsv}
   metaFilterCmdGz = ${metaFilterCmd}
else
   metaFilterCmd = cat
   metaFilterCmdGz = zcat
endif
define copyMetadataTabGz
mkdir -p $(dir $@)
${metaFilterCmdGz} $< > $@.${tmpExt}
mv -f $@.${tmpExt} $@
endef
define copyMetadataTab
mkdir -p $(dir $@)
${metaFilterCmd} $< > $@.${tmpExt}
mv -f $@.${tmpExt} $@
endef

${tableGeneSourceTab}: ${tableGeneSourceMeta}
	${copyMetadataTabGz}
${tableTranscriptSourceTab}: ${tableTranscriptSourceMeta}
	${copyMetadataTabGz}
${tableTranscriptSupportTab}: ${tableTranscriptSupportMeta}
	${copyMetadataTabGz}
${tableExonSupportTab}: ${tableExonSupportMeta} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodeExonSupportToTable} ${skipPatchSeqOpts} ${tableExonSupportMeta} ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@
${tablePdbTab}: ${tablePdbMeta}
	${copyMetadataTabGz}
${tablePubMedTab}: ${tablePubMedMeta}
	${copyMetadataTabGz}
${tableRefSeqTab}: ${tableRefSeqMeta}
	${copyMetadataTabGz}

${tableTranscriptionSupportLevelTab}: ${tableTranscriptionSupportLevelData}
	mkdir -p $(dir $@)
	cp $< $@.${tmpExt}
	mv -f $@.${tmpExt} $@

# convert to zero-based, 1/2 open
${tablePolyAFeatureTab}: ${tablePolyAFeatureMeta}
	@mkdir -p $(dir $@)
	zcat $< | tawk '{print $$1,$$2-1,$$3,$$4,$$5-1,$$6,$$7,$$8}' | sort -k 4,4 -k 5,5n > $@.${tmpExt}
	mv -f $@.${tmpExt} $@
${tableAnnotationRemarkTab}: ${tableAnnotationRemarkMeta} ${gencodeTsv}
	@mkdir -p $(dir $@)
	${metaFilterCmdGz} $<  | tawk '{print $$1,gensub("\\\\n|\\\\","","g",$$2)}' | sort -k 1,1 > $@.${tmpExt}
	mv -f $@.${tmpExt} $@
# drop ENSTR entries that are a hack to support PAR sequences in GTF
${tableEntrezGeneTab}: ${tableEntrezGeneMeta}
	@mkdir -p $(dir $@)
	zcat $< | tawk '$$1!~/^ENSTR/' | sort -k 1,1 | ${metaFilterCmd} /dev/stdin > $@.${tmpExt}
	mv -f $@.${tmpExt} $@

##
# intermediate data for ensembl/havana, not loaded into databases
##
${gencodeGp}: ${annotationGtf} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodeGxfToGenePred} ${skipPatchSeqOpts} ${annotationGtf} ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableTranscriptionSupportLevelData}: ${gencodeTsv}
	touch $@
${gencodeTsv}: ${annotationGtf}
	@mkdir -p $(dir $@)
	${gencodeGxfToAttrs} --keepGoing ${annotationGtf} $@.${tmpExt} --tslTabOut=${tableTranscriptionSupportLevelData}.${tmpExt}
	mv -f ${tableTranscriptionSupportLevelData}.${tmpExt} ${tableTranscriptionSupportLevelData}
	mv -f $@.${tmpExt} $@

# check attributes so code can be updated to handle new biotypes
checkAttrs: ${annotationGtf}
	${gencodeGxfToAttrs} ${annotationGtf} /dev/null

##
# load tables
#  browser commands use static tmp file name, so use lock file to serialize
##
loadLock = flock load.lock

loadTables: ${genePredExtTables:%=${loadedDir}/%.genePredExt.loaded} \
	    ${genePredTables:%=${loadedDir}/%.genePred.loaded} \
	    ${tabTables:%=${loadedDir}/%.tab.loaded}

${loadedDir}/%.genePredExt.loaded: ${tableDir}/%.gp
	@mkdir -p $(dir $@)
	${loadLock} hgLoadGenePred -genePredExt ${db} $* $<
	touch $@

${loadedDir}/%.genePred.loaded: ${tableDir}/%.gp
	@mkdir -p $(dir $@)
	${loadLock} hgLoadGenePred ${db} $* $<
	touch $@

# generic tables
${loadedDir}/%.tab.loaded: ${tableDir}/%.tab
	@mkdir -p $(dir $@)
	${loadLock} hgLoadSqlTab ${db} $* ${encodeAutoSqlDir}/$(subst ${rel},,$*).sql $<
	touch $@

##
# sanity checks
##
# check if the .incorrect files is empty
define checkForIncorrect
awk 'END{if (NR != 0) {print "Incorrect data, see " FILENAME>"/dev/stderr"; exit 1}}' $(basename $@).incorrect
endef

checkSanity: ${checkDir}/${tableGeneSource}.checked ${checkDir}/${tableTranscriptSource}.checked \
	${checkDir}/${tableBasic}.checked ${checkDir}/${tableBasic}.pseudo.checked \
	${checkDir}/${tableComp}.pseudo.checked

# are gene source all in attrs
${checkDir}/${tableGeneSource}.checked: ${loadedDir}/${tableGeneSource}.tab.loaded ${loadedDir}/${tableAttrs}.tab.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select geneId from ${tableAttrs} where geneId not in (select geneId from ${tableGeneSource})' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# are transcript source all in attrs
${checkDir}/${tableTranscriptSource}.checked: ${loadedDir}/${tableTranscriptSource}.tab.loaded  ${loadedDir}/${tableAttrs}.tab.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select transcriptId from ${tableAttrs} where transcriptId not in (select transcriptId from ${tableTranscriptSource})' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# make sure all basic are in comprehensive
${checkDir}/${tableBasic}.checked: ${loadedDir}/${tableBasic}.genePredExt.loaded ${loadedDir}/${tableComp}.genePredExt.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select * from ${tableBasic} where name not in (select name from ${tableComp});' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# make there are no psuedo in basic
${checkDir}/${tableBasic}.pseudo.checked: ${loadedDir}/${tableBasic}.genePredExt.loaded ${loadedDir}/${tableAttrs}.tab.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select * from ${tableBasic}, ${tableAttrs} where name = transcriptId and geneType like "%pseudo%" and geneType != "polymorphic_pseudogene"' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# make there are no psuedo in comprehensive
${checkDir}/${tableComp}.pseudo.checked: ${loadedDir}/${tableComp}.genePredExt.loaded ${loadedDir}/${tableAttrs}.tab.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select * from ${tableComp}, ${tableAttrs} where name = transcriptId and geneType like "%pseudo%" and geneType != "polymorphic_pseudogene"' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# create table list to past into redmine
listTables: tables.lst

tables.lst: loadTables
	hgsql -Ne 'show tables like "wgEncodeGencode%V${ver}"' ${db} >$@.tmp
	mv -f $@.tmp $@


##
# compare number of tracks with previous
##
cmpRelease: gencode-cmp.tsv

gencode-cmp.tsv: loadTables
	@echo 'table	V${prevVer}	V${ver}'  >$@.tmp
	@for tab in ${allTables} ; do \
	    prevTab=$$(echo "$$tab" | sed 's/V${ver}/V${prevVer}/g') ; \
	    echo "$${tab}	"$$(hgsql -Ne "select count(*) from $${prevTab}" ${db})"	"$$(hgsql -Ne "select count(*) from $${tab}" ${db}) ; \
	done >>$@.tmp
	mv -f $@.tmp $@

joinerCheck: loadTables
	@mkdir -p check
	for tbl in $$(hgsql -Ne 'show tables like "wgEncodeGencode%V${ver}"' ${db} | egrep -v 'wgEncodeGencode2wayConsPseudo|wgEncodeGencodePolya') ; do \
	    echo  table=$$tbl; \
	    runJoiner.csh ${db} $$tbl ~/kent/src/hg/makeDb/schema/all.joiner noTimes; \
	    done >check/joiner.out 2>&1
	if fgrep Error: check/joiner.out ; then false;  else true; fi


clean:
	rm -rf ${dataDir} ${tableDir} ${loadedDir} ${checkDir}
