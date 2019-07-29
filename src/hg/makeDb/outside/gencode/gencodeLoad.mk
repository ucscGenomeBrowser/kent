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
#
# - ensemblPrevVersion is use to get chrom name mappings for pre-release,
#   as this doesn't change between release.
##
#db = hg38
#db = hg19
db = mm10
preRelease = no
#preRelease = yes
ifeq (${db},mm10)
    grcRefAssembly = GRCm38
    ver = M22
    prevVer = M21
    gencodeOrg = Gencode_mouse
    ftpReleaseSubdir = release_${ver}
    annGffTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 97_38
    ensemblPrevVer = 96_38
    ensemblCDnaDb = mus_musculus_cdna_${ensemblPrevVer}
else ifeq (${db},hg38)
    grcRefAssembly = GRCh38
    ver = 31
    prevVer = 30
    gencodeOrg = Gencode_human
    ftpReleaseSubdir = release_${ver}
    annGffTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 97_38
    ensemblPrevVer = 96_38
    ensemblCDnaDb = homo_sapiens_cdna_${ensemblPrevVer}
else ifeq (${db},hg19)
    grcRefAssembly = GRCh37
    verBase = 31
    ver = ${verBase}lift37
    backmapTargetVer = 19
    ftpReleaseSubdir = release_${verBase}/GRCh37_mapping
    prevVer = 29lift37
    gencodeOrg = Gencode_human
    annGffTypeName = annotation
    ensemblVer = 74_37      # only used to get genome chromsome name mappings
    ensemblPrevVer = ${ensemblVer}  # doesn't change
    ensemblCDnaDb = homo_sapiens_cdna_${ensemblPrevVer}
    isBackmap = yes
else
    $(error unimplement genome database: ${db})
endif
# END EDIT THESE EACH RELEASE


ifeq (${preRelease},yes)
    # pre-release
    baseUrl = ftp://ftp.ebi.ac.uk/pub/databases/havana/gencode_pre
else
    # official release
    baseUrl = ftp://ftp.ebi.ac.uk/pub/databases/gencode
endif

rel = V${ver}
releaseUrl = ${baseUrl}/${gencodeOrg}/${ftpReleaseSubdir}
dataDir = data
relDir = ${dataDir}/release_${ver}
annotationGff = ${relDir}/gencode.v${ver}.${annGffTypeName}.gff3.gz
pseudo2WayGff = ${relDir}/gencode.v${ver}.2wayconspseudos.gff3.gz
polyAGff = ${relDir}/gencode.v${ver}.polyAs.gff3.gz

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

ifeq (${gencodeOrg}, Gencode_human)
   tableGeneSymbolMeta = ${relDir}/gencode.v${ver}.metadata.HGNC.gz
else
   tableGeneSymbolMeta = ${relDir}/gencode.v${ver}.metadata.MGI.gz
endif
tableGeneSymbol = ${tablePre}GeneSymbol${rel}
tableGeneSymbolTab = ${tableDir}/${tableGeneSymbol}.tab

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
	    ${tableGeneSymbol} ${tablePdb} ${tablePubMed} ${tableRefSeq} ${tableUniProt} \
	    ${tableAnnotationRemark} ${tableEntrezGene}  ${tableTranscriptionSupportLevel}
ifeq (${isBackmap}, yes)
    targetGencodeTsv = ${dataDir}/target-gencode.tsv
else
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
${annotationGff}: ${fetchDone}
${pseudo2WayGff}: ${fetchDone}
${polyAGff}: ${fetchDone}
${tableGeneSourceMeta}: ${fetchDone}
${tableTranscriptSourceMeta}: ${fetchDone}
${tableTranscriptSupportMeta}: ${fetchDone}
${tableExonSupportMeta}: ${fetchDone}
${tableGeneSymbolMeta}: ${fetchDone}
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

${table2WayConsPseudoGp}: ${pseudo2WayGff}
	@mkdir -p $(dir $@)
	gff3ToGenePred -allowMinimalGenes $< $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tablePolyAGp}: ${polyAGff} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodePolyaGxfToGenePred} $< ${ensemblToUcscChain} $@.${tmpExt}
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
   metaFilterCmd = ${gencodeBackMapMetadataIds} ${gencodeTsv} ${targetGencodeTsv}
   metaFilterCmdGz = ${metaFilterCmd}
   metaFilterDepend = ${gencodeTsv} ${targetGencodeTsv}
else
   metaFilterCmd = cat
   metaFilterCmdGz = zcat
   metaFilterDepend = ${gencodeTsv}
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

${tableGeneSourceTab}: ${tableGeneSourceMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableTranscriptSourceTab}: ${tableTranscriptSourceMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableTranscriptSupportTab}: ${tableTranscriptSupportMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableExonSupportTab}: ${tableExonSupportMeta} ${ensemblToUcscChain} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	${gencodeExonSupportToTable} ${tableExonSupportMeta} ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@
${tableGeneSymbolTab}: ${tableGeneSymbolMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tablePdbTab}: ${tablePdbMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tablePubMedTab}: ${tablePubMedMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableRefSeqTab}: ${tableRefSeqMeta} ${metaFilterDepend}
	${copyMetadataTabGz}

${tableTranscriptionSupportLevelTab}: ${tableTranscriptionSupportLevelData}
	mkdir -p $(dir $@)
	cp $< $@.${tmpExt}
	mv -f $@.${tmpExt} $@

# convert to zero-based, 1/2 open
${tablePolyAFeatureTab}: ${tablePolyAFeatureMeta} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	zcat $< | tawk '{print $$1,$$2-1,$$3,$$4,$$5-1,$$6,$$7,$$8}' | sort -k 4,4 -k 5,5n > $@.${tmpExt}
	mv -f $@.${tmpExt} $@
${tableAnnotationRemarkTab}: ${tableAnnotationRemarkMeta} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	${metaFilterCmdGz} $<  | tawk '{print $$1,gensub("\\\\n|\\\\","","g",$$2)}' | sort -k 1,1 > $@.${tmpExt}
	mv -f $@.${tmpExt} $@
# drop ENSTR entries that are a hack to support PAR sequences in GTF
${tableEntrezGeneTab}: ${tableEntrezGeneMeta} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	zcat $< | tawk '$$1!~/^ENSTR/' | sort -k 1,1 | ${metaFilterCmd} /dev/stdin > $@.${tmpExt}
	mv -f $@.${tmpExt} $@

##
# intermediate data for ensembl/havana, not loaded into databases
##
${gencodeGp}: ${annotationGff} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodeGxfToGenePred} ${annotationGff} ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableTranscriptionSupportLevelData}: ${metaFilterDepend}
	touch $@
${gencodeTsv}: ${annotationGff}
	@mkdir -p $(dir $@)
	${gencodeGxfToAttrs} --keepGoing ${annotationGff} $@.${tmpExt} --tslTabOut=${tableTranscriptionSupportLevelData}.${tmpExt}
	mv -f ${tableTranscriptionSupportLevelData}.${tmpExt} ${tableTranscriptionSupportLevelData}
	mv -f $@.${tmpExt} $@

${targetGencodeTsv}:
	@mkdir -p $(dir $@)
	hgsql ${db}  -e 'select * from wgEncodeGencodeAttrsV${backmapTargetVer}' > $@.${tmpExt}
	mv -f $@.${tmpExt} $@


# check attributes so code can be updated to handle new biotypes
checkAttrs: ${annotationGff}
	${gencodeGxfToAttrs} ${annotationGff} /dev/null

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
	@echo 'table	V${prevVer}	V${ver}	delta'  >$@.tmp
	@for tab in ${allTables} ; do \
	    prevTab=$$(echo "$$tab" | sed 's/V${ver}/V${prevVer}/g') ; \
	    echo "$${tab}	"$$(hgsql -Ne "select count(*) from $${prevTab}" ${db})"	"$$(hgsql -Ne "select count(*) from $${tab}" ${db}) ; \
	done | tawk '{print $$1, $$2, $$3, $$3-$$2}' >>$@.tmp
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
