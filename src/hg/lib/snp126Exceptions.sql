CREATE TABLE snp126Exceptions (
    chrom varchar(255) not null,	# Reference sequence chromosome or scaffold
    chromStart int unsigned not null,	# Start position in chrom
    chromEnd int unsigned not null,	# End position in chrom
    name varchar(255) not null,	# Reference SNP identifier or Affy SNP name
    exception varchar(255) not null	# Exception found for this SNP
);
