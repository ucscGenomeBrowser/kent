CREATE TABLE cosmicRaw  (
source varchar(255),
cosmic_mutation_id varchar(255),
gene_name varchar(255),
accession_number varchar(255),
mut_description varchar(255),
mut_syntax_cds blob,
mut_syntax_aa varchar(255),
chromosome varchar(255),
grch37_start varchar(255),
grch37_stop  varchar(255),
mut_nt blob,
mut_aa blob,
tumour_site varchar(255),
mutated_samples varchar(255),
examined_samples varchar(255),
mut_freq varchar(255),

key cosmic_mutation_id (cosmic_mutation_id)
);
