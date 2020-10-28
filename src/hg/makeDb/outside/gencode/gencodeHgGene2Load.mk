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
#   clean - remove all than release directory
#   dropTables - drop the tables
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
db = hg38
#db = hg19
#db = mm10
#preRelease = no
preRelease = yes
ifeq (${db},mm10)
    grcRefAssembly = GRCm38
    ver = M23
    prevVer = M22
    gencodeOrg = Gencode_mouse
    ftpReleaseSubdir = release_${ver}
    annGffTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 97_39
    ensemblPrevVer = 96_38
    ensemblCDnaDb = mus_musculus_cdna_${ensemblPrevVer}
else ifeq (${db},hg38)
    grcRefAssembly = GRCh38
    ver = 35
    prevVer = 34
    gencodeOrg = Gencode_human
    ftpReleaseSubdir = release_${ver}
    annGffTypeName = chr_patch_hapl_scaff.annotation
    ensemblVer = 101_39
    ensemblPrevVer = 100_38
    ensemblCDnaDb = homo_sapiens_cdna_${ensemblPrevVer}
else ifeq (${db},hg19)
    grcRefAssembly = GRCh37
    verBase = 32
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
relRootDir = release
relDir = ${relRootDir}/release_${ver}
annotationGff = ${relDir}/gencode.v${ver}.${annGffTypeName}.gff3.gz

kentDir = /cluster/home/markd/compbio/gencode/projs/hggene-gencode/kent/src
gencodeBinDir = ${kentDir}/hg/makeDb/outside/gencode/bin
autoSqlDir = ${kentDir}/hg/lib
gencodeExonSupportToTable = ${gencodeBinDir}/gencodeExonSupportToTable
gencodeGxfToGenePred = ${gencodeBinDir}/gencodeGxfToGenePred
gencodeGxfToAttrs = ${gencodeBinDir}/gencodeGxfToAttrs
ensToUcscMkLift = ${gencodeBinDir}/ensToUcscMkLift
gencodeBackMapMetadataIds = ${gencodeBinDir}/gencodeBackMapMetadataIds

##
# intermediate data not loaded into tracks
##
gencodeAttrsTsv = ${dataDir}/gencodeAttrs.tsv
ensemblToUcscChain = ${dataDir}/ensemblToUcsc.chain

# flag indicating fetch was done
fetchDone = ${relDir}/done

##
# track and table data
##
tableDir = tables
tablePre = gencode

tableAnnot = ${tablePre}Annot${rel}
tableAnnotGp = ${tableDir}/${tableAnnot}.gp

tableAttrs = ${tablePre}Attrs${rel}
tableAttrsTab = ${tableDir}/${tableAttrs}.tab

tableTag = ${tablePre}Tag${rel}
tableTagTab = ${tableDir}/${tableTag}.tab

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
   tableToGeneSymbolMeta = ${relDir}/gencode.v${ver}.metadata.HGNC.gz
else
   tableToGeneSymbolMeta = ${relDir}/gencode.v${ver}.metadata.MGI.gz
endif
tableToGeneSymbol = ${tablePre}ToGeneSymbol${rel}
tableToGeneSymbolTab = ${tableDir}/${tableToGeneSymbol}.tab

tableToPdbMeta = ${relDir}/gencode.v${ver}.metadata.PDB.gz
tableToPdb = ${tablePre}ToPdb${rel}
tableToPdbTab = ${tableDir}/${tableToPdb}.tab

tableToPubMedMeta = ${relDir}/gencode.v${ver}.metadata.Pubmed_id.gz
tableToPubMed = ${tablePre}ToPubMed${rel}
tableToPubMedTab = ${tableDir}/${tableToPubMed}.tab

tableToRefSeqMeta = ${relDir}/gencode.v${ver}.metadata.RefSeq.gz
tableToRefSeq = ${tablePre}ToRefSeq${rel}
tableToRefSeqTab = ${tableDir}/${tableToRefSeq}.tab

tableSwissProtMeta = ${relDir}/gencode.v${ver}.metadata.SwissProt.gz
tableTrEMBLMeta = ${relDir}/gencode.v${ver}.metadata.TrEMBL.gz
tableToUniProt = ${tablePre}ToUniProt${rel}
tableToUniProtTab = ${tableDir}/${tableToUniProt}.tab

tableAnnotationRemarkMeta = ${relDir}/gencode.v${ver}.metadata.Annotation_remark.gz
tableAnnotationRemark = ${tablePre}AnnotationRemark${rel}
tableAnnotationRemarkTab = ${tableDir}/${tableAnnotationRemark}.tab

tableToEntrezGeneMeta = ${relDir}/gencode.v${ver}.metadata.EntrezGene.gz
tableToEntrezGene = ${tablePre}ToEntrezGene${rel}
tableToEntrezGeneTab = ${tableDir}/${tableToEntrezGene}.tab

tableTranscriptionSupportLevel  = ${tablePre}TranscriptionSupportLevel${rel}
tableTranscriptionSupportLevelTab = ${tableDir}/${tableTranscriptionSupportLevel}.tab

genePredExtTables = ${tableAnnot}
tabTables = ${tableAttrs} ${tableTag} ${tableGeneSource} \
	    ${tableTranscriptSource} ${tableTranscriptSupport} \
	    ${tableToGeneSymbol} ${tableToPdb} ${tableToPubMed} ${tableToRefSeq} ${tableToUniProt} \
	    ${tableAnnotationRemark} ${tableToEntrezGene}  ${tableTranscriptionSupportLevel}
ifeq (${isBackmap}, yes)
    targetGencodeTsv = ${dataDir}/target-gencode.tsv
else
    # these are not included in backmap releases
    tabTables += ${tableExonSupport}
endif
allTables = ${genePredExtTables} ${tabTables}

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
${tableGeneSourceMeta}: ${fetchDone}
${tableTranscriptSourceMeta}: ${fetchDone}
${tableTranscriptSupportMeta}: ${fetchDone}
${tableExonSupportMeta}: ${fetchDone}
${tableToGeneSymbolMeta}: ${fetchDone}
${tableToPdbMeta}: ${fetchDone}
${tableToPubMedMeta}: ${fetchDone}
${tableToRefSeqMeta}: ${fetchDone}
${tableSwissProtMeta}: ${fetchDone}
${tableTrEMBLMeta}: ${fetchDone}
${tableAnnotationRemarkMeta}: ${fetchDone}
${tableToEntrezGeneMeta}: ${fetchDone}

##
# primary table files
##
mkTables: ${genePredExtTables:%=${tableDir}/%.gp} \
	  ${tabTables:%=${tableDir}/%.tab}

${tableAnnotGp}: ${annotationGff} ${ensemblToUcscChain}
	@mkdir -p $(dir $@)
	${gencodeGxfToGenePred} ${db} ${annotationGff} ${ensemblToUcscChain} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableToUniProtTab}: ${tableSwissProtMeta} ${tableTrEMBLMeta} ${gencodeAttrsTsv}
	@mkdir -p $(dir $@)
	((${metaFilterCmdGz} ${tableSwissProtMeta} | tawk '{print $$0,"SwissProt"}') && (${metaFilterCmdGz}  ${tableTrEMBLMeta} | tawk '{print $$0,"TrEMBL"}')) | sort -k 1,1 > $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${ensemblToUcscChain}:
	@mkdir -p $(dir $@)
	${ensToUcscMkLift} ${db} $@.${tmpExt}
	mv -f $@.${tmpExt} $@

# other tab files, just copy to name following convention to make load rules
# work
ifeq (${isBackmap}, yes)
   metaFilterCmd = ${gencodeBackMapMetadataIds} ${gencodeAttrsTsv} ${targetGencodeTsv}
   metaFilterCmdGz = ${metaFilterCmd}
   metaFilterDepend = ${gencodeAttrsTsv} ${targetGencodeTsv}
else
   metaFilterCmd = cat
   metaFilterCmdGz = zcat
   metaFilterDepend =
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
${tableToGeneSymbolTab}: ${tableToGeneSymbolMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableToPdbTab}: ${tableToPdbMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableToPubMedTab}: ${tableToPubMedMeta} ${metaFilterDepend}
	${copyMetadataTabGz}
${tableToRefSeqTab}: ${tableToRefSeqMeta} ${metaFilterDepend}
	${copyMetadataTabGz}

# convert to zero-based, 1/2 open
${tableAnnotationRemarkTab}: ${tableAnnotationRemarkMeta} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	${metaFilterCmdGz} $<  | tawk '{print $$1,gensub("\\\\n|\\\\","","g",$$2)}' | sort -k 1,1 > $@.${tmpExt}
	mv -f $@.${tmpExt} $@
# drop ENSTR entries that are a hack to support PAR sequences in GTF
# FIXME: not needed???
${tableToEntrezGeneTab}: ${tableToEntrezGeneMeta} ${metaFilterDepend}
	@mkdir -p $(dir $@)
	zcat $< | tawk '$$1!~/^ENSTR/' | sort -k 1,1 | ${metaFilterCmd} /dev/stdin > $@.${tmpExt}
	mv -f $@.${tmpExt} $@

${tableAttrsTab}: ${gencodeAttrsTsv}
	touch $@
${tableTranscriptionSupportLevelTab}: ${gencodeAttrsTsv}
	touch $@
${tableTagTab}:  ${gencodeAttrsTsv}
	touch $@
${gencodeAttrsTsv}: ${annotationGff}
	@mkdir -p $(dir $@) ${tableDir}
	${gencodeGxfToAttrs} ${annotationGff} $@.${tmpExt} ${tableAttrsTab}.${tmpExt} ${tableTranscriptionSupportLevelTab}.${tmpExt} ${tableTagTab}.${tmpExt}
	mv -f ${tableAttrsTab}.${tmpExt} ${tableAttrsTab}
	mv -f ${tableTranscriptionSupportLevelTab}.${tmpExt} ${tableTranscriptionSupportLevelTab}
	mv -f  ${tableTagTab}.${tmpExt} ${tableTagTab}
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
	    ${tabTables:%=${loadedDir}/%.tab.loaded}

${loadedDir}/%.genePredExt.loaded: ${tableDir}/%.gp
	@mkdir -p $(dir $@)
	${loadLock} hgLoadGenePred -genePredExt ${db} $* $<
	touch $@

# generic tables
${loadedDir}/%.tab.loaded: ${tableDir}/%.tab
	@mkdir -p $(dir $@)
	${loadLock} hgLoadSqlTab ${db} $* ${autoSqlDir}/$(subst ${rel},,$*).sql $<
	touch $@

##
# sanity checks
##
# check if the .incorrect files is empty
define checkForIncorrect
awk 'END{if (NR != 0) {print "Incorrect data, see " FILENAME>"/dev/stderr"; exit 1}}' $(basename $@).incorrect
endef

checkSanity: ${checkDir}/${tableGeneSource}.checked ${checkDir}/${tableTranscriptSource}.checked \
	${checkDir}/${tableAnnot}.checked

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

# make sure all annots are in attrs
${checkDir}/${tableAnnot}.checked: ${loadedDir}/${tableAttrs}.tab.loaded ${loadedDir}/${tableAnnot}.genePredExt.loaded
	@mkdir -p $(dir $@)
	hgsql -Ne 'select * from ${tableAnnot} where name not in (select transcriptId from ${tableAttrs});' ${db} | sort -u >$(basename $@).incorrect
	@$(checkForIncorrect)
	touch $@

# create table list to past into redmine
listTables: tables.lst

tables.lst: loadTables
	hgsql -Ne 'show tables like "gencode%V${ver}"' ${db} >$@.tmp
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
	for tbl in $$(hgsql -Ne 'show tables like "gencode%V${ver}"' ${db}) ; do \
	    echo  table=$$tbl; \
	    runJoiner.csh ${db} $$tbl ~/kent/src/hg/makeDb/schema/all.joiner noTimes; \
	    done >check/joiner.out 2>&1
	if fgrep Error: check/joiner.out ; then false;  else true; fi


clean:
	rm -rf ${dataDir} ${tableDir} ${loadedDir} ${checkDir}

dropTables:  ${allTables:%=%.drop}
%.drop:
	hgsql ${db} -e 'drop table if exists $*'

