CREATE TABLE snpArrayAffy250Nsp (
    bin integer unsigned not null,
    chrom varchar(255) not null,	# Chromosome
    chromStart int unsigned not null,	# Start position in chrom
    chromEnd int unsigned not null,	# End position in chrom
    name varchar(255) not null,	# Reference SNP identifier or Affy SNP name
    score int unsigned not null,	# Not used
    strand char(1) not null,	# Which DNA strand contains the observed alleles
    observed blob not null,
    rsId varchar(64) 		# dbSNP ID
);
