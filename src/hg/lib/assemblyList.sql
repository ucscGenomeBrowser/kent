# assemblyList.sql was originally generated by the autoSql program, which also 
# generated assemblyList.c and assemblyList.h.  This creates the database representation of
# an object which can be loaded and saved from RAM in a fairly 
# automatic way.

#listing all UCSC genomes, and all NCBI assemblies, with search priority, and status if browser available or can be requested
CREATE TABLE assemblyList (
    name varchar(255),	# UCSC genome: dbDb name or GenArk/NCBI accession
    priority int unsigned,	# assigned search priority
    commonName varchar(511),	# a common name
    scientificName varchar(511),	# binomial scientific name
    taxId int unsigned,	# Entrez taxon ID: www.ncbi.nlm.nih.gov/taxonomy/?term=xxx
    clade varchar(255),	# approximate clade: primates mammals birds fish ... etc ...
    description varchar(1023),	# other description text
    browserExists tinyint unsigned,	# 1 == this assembly is available at UCSC, 0 == can be requested
    hubUrl varchar(511),	# path name to hub.txt: GCF/000/001/405/GCF_000001405.39/hub.txt
    year int unsigned,	# year of assembly construction
    refSeqCategory varchar(31),	# one of: reference, representative or na
    versionStatus varchar(15),	# one of: latest, replaced or suppressed
    assemblyLevel varchar(15),	# one of: complete, chromosome, scaffold or contig
    FULLTEXT gIdx (name, commonName, scientificName, clade, description, refSeqCategory, versionStatus, assemblyLevel),
              #Indices
    PRIMARY KEY(name)
);
