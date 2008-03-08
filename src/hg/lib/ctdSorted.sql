CREATE TABLE ctdSorted (
GeneSymbol varchar(40),
ChemicalId varchar(40),
count int,
ChemicalName blob,

key(GeneSymbol),
key(ChemicalId)
);
