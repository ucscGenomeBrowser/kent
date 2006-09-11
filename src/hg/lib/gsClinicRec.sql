# Clinical recrord of GSID AIDS Vaccine DB project
CREATE TABLE gsClinicRec (
    specimenId varchar(255) not null,		# Specimen Number
    subjId varchar(255) not null,		# Subject ID
    labCode varchar(255) not null,		# MB Lab Code
    dateCollection varchar(255) not null,	# Date of collection
    daysCollection int not null,		# Days of collection
    hivQuan int(10), not null,			# HIV RNA copies/mL
    cd4Count int(10), not null				# CD4 counts
    PRIMARY KEY(specimenId)
);
