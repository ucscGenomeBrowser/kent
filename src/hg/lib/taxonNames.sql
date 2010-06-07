CREATE TABLE taxonNames (
	id int not null,                # Taxon NCBI ID
	name varchar(255) not null,     # Binomial format name
	info varchar(255),              # other info
	nameType varchar(255) not null, # name type
	#Indices
	INDEX(id)
	);
