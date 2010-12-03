# A xref table for rgdGene2
CREATE TABLE rgdGene2Xref (
    rgdGeneId varchar(40) not null,	# RGD gene ID
    infoType  varchar(40) not null,	# type of info for this record
    info text,				# actual info

    key rgdGene(rgdGeneId),
    key info(info(20))
);
