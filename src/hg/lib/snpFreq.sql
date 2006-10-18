CREATE TABLE snpFreq (
    chrom         varchar(15) not null,		# Chromosome
    chromStart    int(10) unsigned not null,	# Start position in chrom
    chromEnd      int(10) unsigned not null,	# End position in chrom
    name          varchar(15) not null,		# Reference SNP identifier 
    allele        varchar(255) not null,	# Nucleotides
    alleleFreq    float not null 		# Between 0.0 and 1.0
);
