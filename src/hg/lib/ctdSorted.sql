CREATE TABLE ctdSorted (
GeneSymbol varchar(40),	# Gene Symbol
ChemicalId varchar(40), # Chemical ID
count int,		# count
ChemicalName blob,	# Chemical Name

key(GeneSymbol),
key(ChemicalId)
);
