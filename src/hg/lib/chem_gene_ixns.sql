CREATE TABLE chem_gene_ixns (
ChemicalName blob,
ChemicalId varchar(40),
CasRegistryNumber blob,
GeneSymbol varchar(40),
GeneId varchar(40),
Organism blob,
OrganismId blob,
Interaction blob,
PubMedIds blob,

key(GeneSymbol)

);
