# Add scientificName column to dbDb table
# This will have the format:  Genus species

ALTER TABLE dbDb ADD COLUMN scientificName VARCHAR(255);

# Add entries for all organisms currently in table

UPDATE dbDb SET scientificName='Papio hamadryas' WHERE organism='Baboon';
UPDATE dbDb SET scientificName='Felis catus' WHERE organism='Cat';
UPDATE dbDb SET scientificName='Gallus gallus' WHERE organism='Chicken';
UPDATE dbDb SET scientificName='Pan troglodytes' WHERE organism='Chimpanzee';
UPDATE dbDb SET scientificName='Ciona intestinalis' WHERE organism='Ciona intestinalis';
UPDATE dbDb SET scientificName='Bos taurus' WHERE organism='Cow';
UPDATE dbDb SET scientificName='Canis familiaris' WHERE organism='Dog';
UPDATE dbDb SET scientificName='Takifugu rubripes' WHERE organism='Fugu';
UPDATE dbDb SET scientificName='Homo sapiens' WHERE organism='Human';
UPDATE dbDb SET scientificName='Mus musculus' WHERE organism='Mouse';
UPDATE dbDb SET scientificName='Plasmodium falciparum' WHERE organism='P. falciparum';
UPDATE dbDb SET scientificName='Pyrobaculum aerophilum' WHERE organism='P. aerophilum';
UPDATE dbDb SET scientificName='Sus scrofa' WHERE organism='Pig';
UPDATE dbDb SET scientificName='Rattus norvegicus' WHERE organism='Rat';
UPDATE dbDb SET scientificName='SARS coronavirus' WHERE organism='SARS';
UPDATE dbDb SET scientificName='Tetraodon nigroviridis' WHERE organism='Tetraodon';
UPDATE dbDb SET scientificName='Caenorhabditis elegans' WHERE genome='C. elegans';
UPDATE dbDb SET scientificName='Caenorhabditis briggsae' WHERE genome='C. briggsae';
UPDATE dbDb SET scientificName='Danio rerio' WHERE organism='Zebrafish';
