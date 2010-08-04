CREATE TABLE rdmrRaw (
    chrom varchar(255) not null,        # Chromosome gene is on
    chromStart int unsigned not null,   # Start position in chromosome
    chromEnd int unsigned not null,     # End position in chromosome
    fibroblast float not null,		# fibroblast
    iPS float not null,			# iPS 
    absArea float not null,		# absArea 
    gene varchar(40),			# gene
    dist2gene int,			# dist2gene
    relation2gene varchar(40),		# relation2gene
    dist2island int,			# dist2island
    relation2island varchar(40),        # relation2island
    fdr float,				# fdr
    KEY(gene)
);
