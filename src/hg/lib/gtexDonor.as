table gtexDonor
"GTEx donor (subject) description"
(
	string name; "GTEX subject identifier (minus leading GTEX-), e.g. N7MS"
	string gender;  "Gender (M/F)"
	uint age;       "Subject age, to the decade (60 means 60-69 yrs)"
        int deathClass; "Hardy scale: 0=Ventilator, 1=Fast violent, 2=Fast natural, 3=Intermediate, 4=Slow.  -1 for unknown"
)
