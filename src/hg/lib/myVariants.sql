# An item in a myVariants type track.
CREATE TABLE myVariants (
    bin int unsigned not null,    # Bin for range index
    chrom varchar(255) not null,  # Reference sequence chromosome or scaffold
    chromStart int unsigned not null,    # Start position in chromosome
    chromEnd int unsigned not null,      # End position in chromosome
    name varchar(255) not null,   # Name of item - up to 16 chars
    score int unsigned not null,  # 0-1000.  Higher numbers are darker.
    strand char(1) not null,      # + or - for strand
    thickStart int unsigned not null,    # Start of thick part
    thickEnd int unsigned not null,      # End position of thick part
    itemRgb int unsigned not null,   # RGB 8 bits each as in bed
    blockCount int unsigned not null,    # Number of blocks
    blockSizes longblob not null,        # Comma separated list of block sizes
    chromStarts longblob not null,       # Start positions relative to chromStart
    description longblob not null,   # Longer item description
    db varchar(255) not null,        # database name of this annotation
    ref varchar(255) not null,       # reference allele
    alt varchar(255) not null,       # alternate allele
    project varchar(255) not null,   # project name for grouping variants
    mouseover varchar(255) not null, # short mouseover text for hover display
    itemType varchar(16) not null default 'snv',  # transcript|snv|cnv
    cnvType varchar(32) not null default '',      # gnomAD term, empty for snv/transcript
    id int auto_increment,           # Unique ID for item
              #Indices
    PRIMARY KEY(id),
    INDEX(chrom(16),bin),
    INDEX(db),
    INDEX(project)
) ENGINE=InnoDB;
