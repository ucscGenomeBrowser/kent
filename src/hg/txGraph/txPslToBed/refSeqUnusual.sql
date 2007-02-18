CREATE TABLE jkgRefSeqUnusual (
    bin        smallint(5) unsigned NOT NULL,
    chrom varchar(255) not null,	# chromosome
    chromStart int unsigned not null,	# Start position in chromosome
    chromEnd int unsigned not null,	# End position in chromosome
    name varchar(255) not null,	# Short description of unusual condition + accession
    type varchar(255) not null, # Short description of unusual condition
    accession varchar(255) not null, # Associated refSeq accession
    description varchar(255) not null, # Longer description of unusual condition
              #Indices
    INDEX(chrom,bin)
    );
