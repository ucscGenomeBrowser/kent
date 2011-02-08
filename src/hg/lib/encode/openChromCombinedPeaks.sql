#Locations of regions of open chromatin as determined by DNaseI HS and FAIRE experiments
CREATE TABLE K562_OpenChromCombinedPeaks (
    chrom varchar(255) not null,	# Human chromosome number
    chromStart int unsigned not null,	# Start position in genoSeq
    chromEnd int unsigned not null,	# End position in genoSeq
    name varchar(255) not null,	# Name of cytogenetic band
    score int(10) unsigned not null, # Always 1000
    strand char(1) not null, # Always .
    thickStart int(10) unsigned not null, # Same as chromStart
    thickEnd int(10) unsigned not null, # Same as chromEnd     
    color int(10) unsigned not null, # Encodes validated, open chromatin, DNase-only, FAIRE-only, ChIP-only
    pValue float unsigned not null, # Combined DNase-seq and FAIRE-seq -log10(p-value)
    dnaseSignal float unsigned not null, # Max signal value for DNase-seq
    dnasePvalue float unsigned not null, # DNase-seq -log10(p-value) 
    faireSignal float unsigned not null, # Max signal value for FAIRE-seq
    fairePvalue float unsigned not null, # FAIRE-seq -log10(p-value) 
    polIISignal float, # Max signal value for PolII ChIP-seq
    polIIPvalue float, # PolII ChIP-seq -log10(p-value) 
    ctcfSignal float, # Max signal value for CTCF ChIP-seq
    ctcfPvalue float, # CTCF ChIP-seq -log10(p-value) 
    cmycSignal float, # Max signal value for c-Myc ChIP-seq
    cmycPvalue float, # c-Myc ChIP-seq -log10(p-value) 
    ocCode int(10) unsigned not null, # Open Chromatin code for validated (=1), DNase-only (=2), FAIRE-only (=3), ChIP-only (=4)

              #Indices
    PRIMARY KEY(chrom(12),chromStart),
    UNIQUE(chrom(12),chromStart)
);
