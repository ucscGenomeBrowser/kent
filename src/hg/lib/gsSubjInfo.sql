#Subject Info for GSID AIDS vaccine DB project
CREATE TABLE gsSubjInfo (
    subjId varchar(255) not null,	# Subject ID
    seqAvail char(1) not null,		# Sequence Data Status
    age varchar(255) not null,		# Age
    gender varchar(255) not null,	# Gender
    race varchar(255) not null,		# Race
    weight varchar(255) not null,	# Weight (kg)
    immunStatus varchar(255) not null,	# Immunization Status
    geography varchar(255) not null,	# Geographic information
    injections int not null,		# Total number of injections prior to infection
    riskFactor varchar(255) not null,	# Risk factor
    dateInfect varchar(255) not null,	# Estimate d HIV-1 infection Date 
    daysInfectF int  not null,		# Days of infection relative to first injection date
    daysInfectL int not null,		# Days of infection relative to last negative date
              #Indices
    PRIMARY KEY(subjId)
);
