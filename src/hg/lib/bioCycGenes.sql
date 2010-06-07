# BioCyc genes table.

CREATE TABLE genes (
UNIQUE_ID varchar(255),
NAME varchar(255),
PRODUCT_NAME varchar(255),
SWISS_PROT_ID varchar(255),
REPLICON varchar(255),
START_BASE varchar(255),
END_BASE varchar(255),
SYNONYMS1 varchar(255),
SYNONYMS2 varchar(255),
SYNONYMS3 varchar(255),
SYNONYMS4 varchar(255),
GENE_CLASS1 varchar(255),
GENE_CLASS2 varchar(255),
GENE_CLASS3 varchar(255),
GENE_CLASS4 varchar(255),

KEY (UNIQUE_ID),
KEY (NAME)
);
