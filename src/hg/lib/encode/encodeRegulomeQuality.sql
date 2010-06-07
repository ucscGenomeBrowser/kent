CREATE TABLE encodeRegulomeQualityXXX (
    bin smallint not null,
    chrom varchar(255) not null,	# Chromosome
    chromStart int unsigned not null,	# Start position in chromosome
    chromEnd int unsigned not null,	# End position in chromosome
    name varchar(255) not null,	        # Name
    score int unsigned not null,	# Quality score 
    #Indices
    INDEX chrom (chrom(12),chromStart),
    INDEX name (name)
);
