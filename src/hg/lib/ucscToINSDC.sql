# UCSC to INSDC chromosome name translation
CREATE TABLE ucscToINSDC (
    chrom varchar(255) not null,	# Human chromosome or FPC contig
    chromStart int unsigned not null,	# Start position in chromosome
    chromEnd int unsigned not null,	# End position in chromosome
    name varchar(255) not null,		# INSDC name at www.insdc.org
              #Indices
    PRIMARY KEY(chrom(21))
);
