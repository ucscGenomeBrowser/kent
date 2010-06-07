CREATE TABLE refSeqKg (
    refseq     varchar(40) not null,	# RefSeq ID
    kgId       varchar(40) not null,	# UCSC Known Gene ID
    cdsOverlap float,			# CDS overlap fraction
#Indices
    KEY(refseq),
    KEY(kgId)
);
