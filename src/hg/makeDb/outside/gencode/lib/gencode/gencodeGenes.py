"""
Objects to store gencode gene annotations
"""
import re
from pycbio.hgdata.genePred import GenePredReader
from pycbio.tsv import TsvReader, TabFileReader
from pycbio import PycbioException
from pycbio.hgdata.rangeFinder import RangeFinder
from pycbio.sys.symEnum import SymEnum, auto
from gencode.gencodeTags import GencodeTag, gencodeTagNotFull
from gencode.biotypes import BioType, bioTypesCoding, bioTypesNonCoding, bioTypesPseudo, bioTypesProblem, bioTypesIG, bioTypesTR, getFunctionForBioType

class GencodeSource(SymEnum):
    HAVANA = auto()
    ENSEMBL = auto()

class GencodeMethod(SymEnum):
    manual = auto()
    automatic = auto()

class GencodeExtendedMethod(SymEnum):
    manualOnly = auto()
    automaticOnly = auto()
    merged = auto()

class GencodeFunction(SymEnum):
    pseudo = auto()
    coding = auto()
    nonCoding = auto()
    problem = auto()

class GencodeCompleteness(SymEnum):
    partial = auto()
    full = auto()

def _noneIfEmpty(s):
    return s if len(s) > 0 else None

def _emptyIfNone(s):
    return s if s is not None else ""

def sourceToMethod(source):
    if source == GencodeSource.HAVANA:
        return GencodeMethod.manual
    elif source == GencodeSource.ENSEMBL:
        return GencodeMethod.automatic
    else:
        raise GencodeGenesException("source not defined")

def sourceToExtendedMethod(metaSrc):
    # source is from gene or transcript source files
    hasHav = metaSrc.find("havana") >= 0
    hasEns = metaSrc.find("ensembl") >= 0
    if hasHav and hasEns:
        return GencodeExtendedMethod.merged
    elif hasHav:
        return GencodeExtendedMethod.manualOnly
    else:
        return GencodeExtendedMethod.automaticOnly

class GencodeGenesException(PycbioException):
    "exception associated with Gencode Genes objects"
    pass

class BioTags(set):
    "set of tags of type BioTag"
    def __init__(self, tagsInit=None):
        "tagsInit can be a string that is parsed or a list, tuple, or set to initialize the set"
        if tagsInit is not None:
            if isinstance(tagsInit, str):
                self._initFromStr(tagsInit)
            else:
                set.__init__(self, tagsInit)

    def _initFromStr(self, tagsStr):
        tagsStr = tagsStr.strip()
        if len(tagsStr) > 0:
            for t in tagsStr.split(","):
                t = t.strip()
                if t.startswith("seleno_"):  # deal with seleno_62
                    t = "seleno"
                self.add(GencodeTag(t))

    def __str__(self):
        tagStrs = [str(t) for t in self]
        tagStrs.sort()
        return ",".join(tagStrs)

    def getSorted(self):
        return sorted(self)

    def isFullLength(self):
        for tag in self:
            if tag in gencodeTagNotFull:
                return False
        return True

class GencodeTranscriptLocus(object):
    """object for a transcript at a given locus.  Need due to PAR on older GENCODEs were
    id was duplicated"""
    __slots__ = ("transcript", "gp", "geneLocus")

    def __init__(self, transcript, gp):
        self.transcript = transcript
        self.geneLocus = None
        self.gp = gp

    def isSingleExon(self):
        return (len(self.gp.exons) == 1)

    def __str__(self):
        return self.gp.name + "\t" + self.gp.chrom + ":" + str(self.gp.txStart) + "-" + str(self.gp.txEnd)

    @property
    def id(self):
        return self.transcript.id

    @property
    def chrom(self):
        return self.gp.chrom

    @property
    def chromStart(self):
        return self.gp.txStart

    @property
    def chromEnd(self):
        return self.gp.txEnd

    @property
    def strand(self):
        return self.gp.strand

    def hasMultiLoci(self):
        "is the transcript associated with multiple loci?"
        return len(self.transcript.transcriptLoci) > 1

    def hasCds(self):
        return (self.gp.cdsStart < self.gp.cdsEnd)

class GencodeTranscript(object):
    "object for a single transcript"
    __slots__ = ("id", "name", "bioType", "havanaId", "ccdsId", "proteinId", "transcriptRank",
                 "source", "tags", "gene", "level", "extendedMethod", "transcriptSupportLevel", "transcriptLoci")

    # FIXME: gene and transcript tags should not really be combined, but
    # need to have geneTag and transcriptTag tables load from table dumps.
    def __init__(self, transcriptId):
        self.id = transcriptId
        self.name = None
        self.bioType = None
        self.havanaId = None
        self.ccdsId = None
        self.proteinId = None
        self.transcriptRank = None
        self.source = None
        self.tags = BioTags()
        self.level = None
        self.gene = None
        self.extendedMethod = None
        self.transcriptSupportLevel = None   # only set from GTF/GFF3
        self.transcriptLoci = []  # GencodeTranscriptLocus objects

    def finish(self):
        "finish construction, sorting into predictable order"
        self.transcriptLoci.sort(key=lambda loci: (loci.gp.chrom, loci.gp.txStart))

    @property
    def transcriptClass(self):
        return self.getFunction()

    def hasCds(self):
        return self.transcriptLoci[0].hasCds()

    def isCoding(self):
        return self.bioType in bioTypesCoding

    def isNonCoding(self):
        return self.bioType in bioTypesNonCoding

    def isPseudo(self):
        return self.bioType in bioTypesPseudo

    def isProblem(self):
        return self.bioType in bioTypesProblem

    def isHLA(self):
        "is this an HLA transcript?"
        return self.gene.isHLA()

    def isOlfactoryReceptor(self):
        "is this an olfactory receptor transcript?"
        return self.gene.isOlfactoryReceptor()

    def isImmunoglobin(self):
        return self.bioType in bioTypesIG

    def isTCellReceptor(self):
        return self.bioType in bioTypesTR

    def isFullLength(self):
        return self.tags.isFullLength()

    def isSingleExon(self):
        return self.transcriptLoci[0].isSingleExon()

    def getFunction(self):
        # all transcripts in pseudogenes are psuedogene transcripts
        if self.gene.getFunction() == GencodeFunction.pseudo:
            return GencodeFunction.pseudo
        else:
            return getFunctionForBioType(self.bioType)

    def getCompleteness(self):
        return GencodeCompleteness.full if self.isFullLength() else GencodeCompleteness.partial

    def getMethod(self):
        return sourceToMethod(self.source)

    def getExtendedMethod(self):
        """get extended method that indicates merged transcripts.  Must have
        loaded the Transcript_source metadata"""
        if self.extendedMethod is None:
            raise GencodeGenesException("Transcript_source metadata must be loaded to get transcript extended method: " + self.id)
        return self.extendedMethod

    infoTsvHeader = ("geneId", "geneName", "geneType", "transcriptId", "transcriptName", "transcriptType", "havanaGene", "havanaTranscript", "ccdsId", "level", "transcriptClass", "proteinId", "tags")

    def toInfoRow(self):
        return [self.gene.id, self.gene.name, self.gene.bioType, self.id, self.name, self.bioType, _emptyIfNone(self.ccdsId), self.level, self.transcriptClass, _emptyIfNone(self.proteinId),
                self.transcriptRank, str(BioTags(self.tags | self.gene.tags))]

class GencodeGeneLocus(object):
    """object to group all of the GencodeTranscriptLocus objects for a given
    GencodeGene.  Doesn't consider strand, since anti-sense transcripts are part of
    the same gene."""
    __slots__ = ("gene", "chrom", "chromStart", "chromEnd", "strand", "transcriptLoci")

    def __init__(self, gene, chrom):
        self.gene = gene
        self.chrom = chrom
        self.chromStart = None
        self.chromEnd = None
        self.strand = None
        self.transcriptLoci = []

    def finish(self):
        "finish construction, sorting into predictable order"
        self.transcriptLoci.sort(key=lambda loci: (loci.gp.chrom, loci.gp.txStart))

    @property
    def id(self):
        return self.gene.id

    def isSingleExonGene(self):
        "is this a single-exon gene (all transcripts are single exon)"
        for tl in self.transcriptLoci:
            if not tl.isSingleExon():
                return False
        return True

    def add(self, transcriptLocus):
        assert transcriptLocus.gp.chrom == self.chrom
        self.transcriptLoci.append(transcriptLocus)
        if self.chromStart is None:
            self.chromStart = transcriptLocus.gp.txStart
            self.chromEnd = transcriptLocus.gp.txEnd
            self.strand = transcriptLocus.gp.strand
        else:
            self.chromStart = min(self.chromStart, transcriptLocus.gp.txStart)
            self.chromEnd = max(self.chromEnd, transcriptLocus.gp.txEnd)
            if self.strand != transcriptLocus.gp.strand:
                raise GencodeGenesException("gene has transcripts with different strands: " + self.gene.id)

class GencodeGene(object):
    "object for a gene"
    __slots__ = ("id", "name", "bioType", "havanaId", "source", "transcripts", "extendedMethod", "geneLoci", "tags")

    def __init__(self, geneId):
        self.id = geneId
        self.name = None
        self.bioType = None
        self.source = None
        self.havanaId = None
        self.extendedMethod = None
        self.tags = BioTags()  # gene level tags
        self.transcripts = []  # GencodeTranscript objects
        self.geneLoci = []     # GencodeGeneLocus objects

    def finish(self):
        "finish construction, sorting into predictable order"
        self.transcripts.sort(key=lambda t: (t.id,))
        self.geneLoci.sort(key=lambda loci: (loci.chrom, loci.chromStart))

    def hasCds(self):
        for trans in self.transcripts:
            if trans.hasCds():
                return True
        return False

    def isCoding(self):
        return self.bioType in bioTypesCoding

    def isNonCoding(self):
        return self.bioType in bioTypesNonCoding

    def isPseudo(self):
        return self.bioType in bioTypesPseudo

    def isProblem(self):
        return self.bioType in bioTypesProblem

    def isHLA(self):
        "is this an HLA gene?"
        return (self.name.find("HLA-") == 0)

    __orGeneSymRe = re.compile("^OR[0-9].+")

    def isOlfactoryReceptor(self):
        "is this an olfactory receptor gene?"
        return self._orGeneSymRe.match(self.name) is not None

    def getFunction(self):
        return getFunctionForBioType(self.bioType)

    def getMethod(self):
        return sourceToMethod(self.source)

    def getExtendedMethod(self):
        """get extended method that indicates merged genes.  Must have
        loaded the Gene_source metadata"""
        if self.extendedMethod is None:
            raise GencodeGenesException("Gene_source metadata must be loaded to get gene extended method: " + self.id)
        return self.extendedMethod

    def obtainLocus(self, chrom):
        """obtain the GencodeGeneLocus object for the specified chromosome, or
        create one"""
        for locus in self.geneLoci:
            if locus.chrom == chrom:
                return locus
        locus = GencodeGeneLocus(self, chrom)
        self.geneLoci.append(locus)
        return locus

class GencodeGenes(object):
    "object to information and optional genePreds of all gencode genes"

    def __init__(self):
        self.transcriptsById = dict()
        self.genesById = dict()
        # build in a lazy manner
        self.transcriptLociByRange = None
        self.geneLociByRange = None

    def getTranscript(self, transcriptId):
        "get a transcript object, or none if it doesn't exist"
        return self.transcriptsById.get(transcriptId)

    def getTranscriptRequired(self, transcriptId):
        "get a transcript object, or exception if it doesn't exist"
        trans = self.transcriptsById.get(transcriptId)
        if trans is None:
            raise GencodeGenesException("transcript not found in attributes: " + transcriptId)
        return trans

    def getTranscripts(self):
        "get an iterator over all transcript objects"
        return self.transcriptsById.values()

    def obtainTranscript(self, transcriptId):
        "get or create a transcript object"
        trans = self.getTranscript(transcriptId)
        if trans is None:
            trans = self.transcriptsById[transcriptId] = GencodeTranscript(transcriptId)
        return trans

    def getGene(self, geneId):
        "get a gene object, or none if it doesn't exist"
        return self.genesById.get(geneId)

    def getGeneRequired(self, geneId):
        "get a gene object, or none if it doesn't exist"
        gene = self.genesById.get(geneId)
        if gene is None:
            raise GencodeGenesException("gene not found in attributes: " + geneId)
        return gene

    def obtainGene(self, geneId):
        "get or create a gene object"
        gene = self.getGene(geneId)
        if gene is None:
            gene = self.genesById[geneId] = GencodeGene(geneId)
        return gene

    def getGeneLoci(self):
        "generator over all GencodeGeneLocus objects"
        for gene in self.genesById.values():
            for geneLocus in gene.geneLoci:
                yield geneLocus

    def getOverlappingGeneLoci(self, chrom, chromStart, chromEnd, strand=None):
        """Get GeneLoci overlapping range.  This covers entry range of gene,
        including introns"""
        if self.geneLociByRange is None:
            self._buildGeneLociByRange()
        return self.geneLociByRange.overlapping(chrom, chromStart, chromEnd, strand)

    def getTranscriptLoci(self):
        "generator over all GencodeTranscriptLocus objects"
        for trans in self.transcriptsById.values():
            for transLocus in trans.transcriptLoci:
                yield transLocus

    def getOverlappingTranscriptLoci(self, chrom, chromStart, chromEnd, strand=None):
        """"get TranscriptLoci overlapping range. This only overlap with
        exons"""
        if self.transcriptLociByRange is None:
            self._buildTranscriptLociByRange()
        return self.transcriptLociByRange.overlapping(chrom, chromStart, chromEnd, strand)

    def getTranscriptsSortByLocus(self):
        """get a list of transcript objects sorted to optimize access by
        location and test consistency."""
        return sorted(self.transcriptsById.values(),
                      key=lambda t: (t.transcriptLoci[0].gp.chrom, t.transcriptLoci[0].gp.txStart, t.transcriptLoci[0].gp.txEnd, t.transcriptLoci[0].gp.name))

    def getTranscriptsSortById(self):
        """get a list of transcript objects sorted by id."""
        return sorted(self.transcriptsById.values(),
                      key=lambda t: (t.id,))

    def getTranscriptsSorted(self):
        """get a list of transcript objects sorted first by gene id, then transcript id."""
        return sorted(self.transcriptsById.values(),
                      key=lambda t: (t.gene.id, t.id))

    def _buildTranscriptLociByRange(self):
        "construct the transcriptLociByRange index when needed"
        self.transcriptLociByRange = RangeFinder()
        for transLocus in self.getTranscriptLoci():
            gp = transLocus.gp
            for exon in gp.exons:
                self.transcriptLociByRange.add(gp.chrom, exon.start, exon.end, transLocus, gp.strand)

    def _buildGeneLociByRange(self):
        "construct the geneLociByRange index when needed"
        self.geneLociByRange = RangeFinder()
        for geneLocus in self.getGeneLoci():
            self.geneLociByRange.add(geneLocus.chrom, geneLocus.chromStart, geneLocus.chromEnd, geneLocus, strand=geneLocus.strand)

    def _setInfoRowGene(self, info, gene):
        gene.name = info.geneName
        gene.bioType = info.geneType
        gene.havanaId = info.havanaGeneId

    def _loadInfoRowGene(self, info):
        gene = self.obtainGene(info.geneId)
        if gene.name is None:
            self._setInfoRowGene(info, gene)
        elif not ((gene.name == info.geneName) and (gene.bioType == info.geneType)):
            raise GencodeGenesException("inconsistent gene meta-data from multiple info rows: " + info.geneId)
        return gene

    def _setInfoRowTranscript(self, info, gene, trans):
        trans.name = info.transcriptName
        trans.bioType = info.transcriptType
        trans.havanaId = info.havanaTranscriptId
        trans.ccdsId = info.ccdsId
        trans.level = info.level
        trans.transcriptSupportLevel = info.tsl
        trans.proteinId = info.proteinId
        trans.transcriptRank = info.transcriptRank

    def _linkTranscriptToGene(self, gene, trans):
        trans.gene = gene
        gene.transcripts.append(trans)
        for transLocus in trans.transcriptLoci:
            geneLocus = gene.obtainLocus(transLocus.gp.chrom)
            geneLocus.add(transLocus)
            transLocus.geneLocus = geneLocus

    def _loadInfoRowTranscript(self, info, gene):
        # allows for rows being accidental duplicated
        trans = self.obtainTranscript(info.transcriptId)
        if trans.name is None:
            self._setInfoRowTranscript(info, gene, trans)
        elif not ((trans.name == info.transcriptName) and (trans.bioType == info.transcriptType)):
            raise GencodeGenesException("inconsistent transcript meta-data from multiple info rows: " + info.transcriptId)
        # accumulate tags (PAR tag not on all copies).  May not have it tag if loading from database
        # also may or may not be split into geneTags and transcriptTags
        for tagFld in ("tags", "geneTags", "transcriptTags"):
            tags = getattr(info, tagFld, None)
            if tags is not None:
                trans.tags |= tags
        if trans.gene is None:
            self._linkTranscriptToGene(gene, trans)

        # gene source is HAVANA unless all transcripts are ENSEMBL
        if gene.source is None:
            gene.source = trans.source
        elif (gene.source is GencodeSource.ENSEMBL) and (trans.source is GencodeSource.HAVANA):
            gene.source = trans.source

    def _loadInfoRow(self, info, onlyExisting):
        if (not onlyExisting) or (info.transcriptId in self.transcriptsById):
            try:
                gene = self._loadInfoRowGene(info)
                self._loadInfoRowTranscript(info, gene)
            except Exception as ex:
                raise GencodeGenesException("error loading gencode info for %s" % (info.transcriptId,)) from ex

    def loadInfoFromFile(self, gencodeInfo, onlyExisting=True):
        """load file produced by the gencodeGtfToAttrs script.  If onlyExisting
        is true, only entries for transcripts already loaded from genePreds
        area included.
        """
        infoTypeMap = {"geneType": BioType,
                       "transcriptType": BioType,
                       "source": GencodeSource,
                       "level": int,
                       "geneTags": BioTags,
                       "transcriptTags": BioTags,
                       "havanaGene": _noneIfEmpty,
                       "havanaTranscript": _noneIfEmpty,
                       "ccdsId": _noneIfEmpty}
        for info in TsvReader(gencodeInfo, typeMap=infoTypeMap):
            self._loadInfoRow(info, onlyExisting)

    def _checkLociOverlap(self, gp, transcript):
        for locus in transcript.transcriptLoci:
            if gp.chrom == locus.gp.chrom:
                raise GencodeGenesException("multiple annotations for \"%s\" on \"%s\"" % (gp.name, gp.chrom))

    def _loadGenePred(self, gp):
        "add a genePred"
        try:
            trans = self.obtainTranscript(gp.name)
            self._checkLociOverlap(gp, trans)
            transLocus = GencodeTranscriptLocus(trans, gp)
            trans.transcriptLoci.append(transLocus)
        except Exception as ex:
            raise GencodeGenesException("error loading genePred for %s at %s:%d-%d" % (gp.name, gp.chrom, gp.txStart, gp.txEnd)) from ex

    def loadGenePredsFromFile(self, gpFile, chroms=None):
        "load genePreds from a file, optionally filtering for a list or set of chromosome"
        for gp in GenePredReader(gpFile):
            if (chroms is None) or (gp.chrom in chroms):
                self._loadGenePred(gp)

    def _checkTransHasGene(self, transcript):
        "a transcript would not have a gene if it was in the genePred, but not info"
        if transcript.gene is None:
            raise GencodeGenesException("no gene id associated with transcript \"" + transcript.id + "\"")

    def loadTranscriptSourceFromFile(self, transcriptSource):
        "load transcript source metadata from a file"
        for row in TabFileReader(transcriptSource):
            trans = self.getTranscript(row[0])
            if trans is not None:
                trans.extendedMethod = sourceToExtendedMethod(row[1])

    def loadGeneSourceFromFile(self, geneSource):
        "load gene source metadata from a file"
        for row in TabFileReader(geneSource):
            gene = self.getGene(row[0])
            if gene is not None:
                gene.extendedMethod = sourceToExtendedMethod(row[1])

    def loadTagsFromFile(self, tags):
        "load tags file.  Used with table dumps when tags are not in info file. "
        for row in TabFileReader(tags):
            trans = self.getTranscript(row[0])
            if trans is not None:
                trans.tags.add(GencodeTag(row[1]))

    def check(self, errFh=None):
        """check the that all loci have associated transcripts/gene information.
        If errFh is supplied, then output all problems before raising an exception."""
        errCnt = 0
        for transcript in self.transcriptsById.values():
            try:
                self._checkTransHasGene(transcript)
            except GencodeGenesException as ex:
                errCnt += 1
                if errFh is not None:
                    errFh.write("Error: " + str(ex) + "\n")
                else:
                    raise ex
        if errCnt != 0:
            raise GencodeGenesException("missing gene information for " + str(errCnt) + " loci, check info file")

    def finish(self, errFh=None):
        """run checking and sort internal lists so that results are deterministic"""
        for gene in self.genesById.values():
            gene.finish()
        self.check(errFh)

    @staticmethod
    def loadFromFiles(gpFile=None, gencodeInfo=None, geneSource=None, transcriptSource=None, tags=None, errFh=None, chroms=None, onlyExisting=True):
        "factory method to load from files (see load functions)"
        gencode = GencodeGenes()
        if gpFile is not None:
            gencode.loadGenePredsFromFile(gpFile, chroms=chroms)
        else:
            onlyExisting = False  # force loading only from info
        if gencodeInfo is not None:
            gencode.loadInfoFromFile(gencodeInfo, onlyExisting=onlyExisting)
        if geneSource is not None:
            gencode.loadGeneSourceFromFile(geneSource)
        if transcriptSource is not None:
            gencode.loadTranscriptSourceFromFile(transcriptSource)
        if tags is not None:
            gencode.loadTagsFromFile(tags)
        gencode.finish(errFh=errFh)
        return gencode
