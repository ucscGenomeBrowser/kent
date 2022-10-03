"""
records parsed from either GTF or GFF3 files.  This is not general
"""
class Attributes(object):
    "attributes and values"

    stdAttrs = frozenset(("gene_id", "gene_name", "gene_type", "gene_status", "transcript_id", "transcript_name", "transcript_type", "transcript_status", "havana_gene", "havana_transcript", "ccdsid", "level", "exon_id", "protein_id",
                          "transcript_support_level", "ID", "Parent", "hgnc_id"))
    singleValueAttrs = frozenset(stdAttrs)
    multiValueAttrs = frozenset(["tag", "ont"])
    ignoredAttrs = frozenset(["exon_number"])
    knownAttrs = frozenset(stdAttrs | singleValueAttrs | multiValueAttrs | ignoredAttrs)
    allFields = frozenset(knownAttrs | frozenset(("tags", "onts")))
    __slots__ = stdAttrs | singleValueAttrs | frozenset(("tags", "onts"))

    def __init__(self, attrVals):
        "parse the attributes and values into fields"
        for attr in Attributes.__slots__:
            setattr(self, attr, None)
        self.tags = set()
        self.onts = set()
        for attr, val in attrVals:
            self.__setAttrVal(attr, val)

    def __setAttrVal(self, attr, val):
        if attr == "tag":
            self.tags.add(val)
        elif attr == "ont":
            self.onts.add(val)
        elif attr == "level":
            self.level = int(val)
        elif (attr not in self.ignoredAttrs) and (attr in self.allFields):
            setattr(self, attr, val)


class Record(object):
    "one record of a GTF or GFF3"

    __slots__ = ("line", "lineNumber", "seqname", "source", "feature", "start", "end", "score", "strand", "phase", "attributes")

    def __init__(self, line, lineNumber, seqname, source, feature, start, end, score, strand, phase, attrVals):
        self.line = line
        self.lineNumber = lineNumber
        self.seqname = seqname
        self.source = source
        self.feature = feature
        self.start = start
        self.end = end
        self.score = score
        self.strand = strand
        self.phase = phase
        self.attributes = Attributes(attrVals)

    def __str__(self):
        return self.line
