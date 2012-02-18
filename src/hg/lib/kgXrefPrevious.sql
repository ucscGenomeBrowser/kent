# this is a copy from kgXref.sql to supply a description for the rn4
# table kgXrefPrevious

#Link together a Known Gene ID and a gene alias
CREATE TABLE kgXrefPrevious (
    kgID varchar(40) not null,	# Known Gene ID
    mRNA varchar(40),		# mRNA ID
    spID varchar(40),		# UniProt protein Accession number
    spDisplayID varchar(40),	# UniProt display ID
    geneSymbol varchar(40),	# Gene Symbol
    refseq varchar(40),		# RefSeq ID
    protAcc varchar(40),	# NCBI protein Accession number
    description varchar(255),	# Description
              #Indices
    KEY(kgID),
    KEY(mRNA),
    KEY(spID),
    KEY(spDisplayID),
    KEY(geneSymbol),
    KEY(refseq),
    KEY(protAcc),
);
