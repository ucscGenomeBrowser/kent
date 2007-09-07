TABLE patient
	"Table patient"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string DataExtractDt;		"Date data was downloaded from the CALGB database"
	string Inst_ID;		"Registering Institution"
	)

TABLE patientInfo
	"Table patientInfo"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string DataExtractDt;		"Date data was downloaded from the CALGB database"
	string Inst_ID;		"Registering Institution"
	string AgeCat;		"Patient Age Category"
	float Age;		"Patient Age"
	string Race_id;		"Patient Race (1 case is 136 for white, black and American Indian or Alaska Native)"
	string Sstat;		"Survival Status"
	int SurvDtD;		"Survival date (time from study entry to death or last follow-up)"
	)

TABLE chemo
	"Table chemo"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string Chemo;		"Neo-Adjuvant Chemotherapy Regimen"
	string ChemoCat;		"Chemotherapy Group Category"
	string DoseDenseAnthra;		"Dose Dense Anthracycline Therapy?"
	string DoseDenseTaxane;		"Dose Dense Taxane Therapy?"
	string Tam;		"Tamoxifen received"
	string Herceptin;		"Herceptin received"
	)

TABLE onStudy
	"Table onStudy"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string MenoStatus;		"Menopausal Status"
	string SentinelNodeSample;		"Was Sentinel node sampling performed pre-treatment?"
	string SentinelNodeResult;		"Sentinel Node Biopsy Results pretreatment"
	string HistTypeInvOS;		"Histologic Type of Invasive Tumor (On-Study)"
	string HistologicGradeOS;		"Combined Histologic Grade - On-study (According to SBR/Elston Classification)"
	int ER_TS;		"Estrogen Receptor Status - Total Score Total Score = ER_PS+ ER_IS Considered Allred Score; 3 is positive"
	int PgR_TS;		"Progesterone Receptor Status - Total Score Total Score = PgR_PgS+ PgR_IS Considered Allred Score; 3 is positive"
	string Her2CommunityPos;		"Her2 Summary as measured in the Community"
	string Her2CommunityMethod;		"Her2 Summary method as measured in the Community"
	)

TABLE postSurgery
	"Table postSurgery"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string SurgeryLumpectomy;		"Surgery Procedure Performed was Partial mastectomy/lumpectomy/excisional biopsy"
	string SurgeryMastectomy;		"SurgeryMastectomy Surgery Procedure Performed was Mastectomy, NOS"
	string InitLump_FupMast;		"Initial Lumpectomy Surgery followed by Mastectomy Surgery at a later date"
	string Surgery;		"Did patient have extensive Primary Surgery immediately following chemotherapy?"
	string DCISonly;		"DCIS only thing left following surgery"
	float PTumor1Szcm_Micro;		"Primary Tumor Pathological Tumor"
	string HistologicTypePS;		"Histologic Type of Primary Tumor (Post-Surgery)"
	int HistologicGradePS;		"Combined Histologic Grade -Post-Surgery (According to SBR/Elston Classification)"
	int NumPosNodes;		"Total Number positive Axillary + Sentinel (post) Nodes, post-chemotherapy"
	string NodesExamined;		"Total Number of Axillary + Sentinel (post) nodes Examined, postchemotherapy"
	string PathologyStage;		"Pathology Assessment Staging"
	string ReasonNoSurg;		"Principal Reason Breast Conserving Surgery Not Performed"
	)

TABLE followUp
	"Table followUp"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string RtTherapy;		"Has patient received adjuvant radiation therapy (prior to treatment failure or second primary cancer)? "
	string RtBreast;		"Radiation to Breast"
	string RtBoost;		"Radiation to Boost"
	string RtAxilla;		"Radiation to Axilla"
	string RtSNode;		"Radiation to Supraclavicular node"
	string RtIMamNode;		"Radiation to Internal Mammary node"
	string RTChestW;		"Radiation to Chest Wall"
	string RtOther;		"Radiation to Other Site"
	)

TABLE respEval
	"Table respEval"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	float TSizeClinical;		"Size of Primary Tumor (cm) - Clinical Assessment at Baseline"
	float NSizeClinical;		"Size of Largest Palpable Node (cm) - Clinical Assessment at Baseline"
	string StageTe;		"Disease Stage T (metastasis) Baseline"
	string StageNe;		"Disease Stage N (metastasis) Baseline"
	string StageMe;		"Disease Stage M (metastasis) Baseline"
	string ClinicalStage;		"Clinical Staging at Baseline"
	string ClinRespT1_T2;		"Clinical Response Baseline to Early Treatment"
	string ClinRespT1_T3;		"Clinical Response Baseline to Inter-Regimen"
	string ClinRespT1_T4;		"Clinical Response Baseline to Pre-Surgery"
	)

TABLE mr
	"Table mr"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string ChemoCat;		"CHEMOCAT"
	string DoseDenseAnthra;		"DOSEDENSEANTHRA"
	string DoseDenseTaxane;		"DOSEDENSETAXANE"
	string LES_T1;		"Lesion type at T1 Pre-Treatment - Baseline."
	string LES_T2;		"Lesion type at the T2 Early Treatment timepoint (as indicated on the M4 form)."
	string LES_T3;		"Lesion type at the T3 Between Treatment Regimes timepoint (as indicated on the M4 form)."
	string LES_T4;		"Lesion type at the T4 Pre-Surgery timepoint (as indicated on the M4 form)."
	int LD_T1;		"Longest Diameter (LD) in the cancer mass at T1 Pre-Treatment - Baseline."
	int LD_T2;		"Longest Diameter (LD) in the cancer mass at the T2 Early Treatment timepoint."
	int LD_T3;		"Longest Diameter (LD) in the cancer mass at the T3 Between Treatment Regimes timepoint."
	int LD_T4;		"Longest Diameter (LD) in the cancer mass at the T4 Pre-Surgery timepoint."
	float LD_T1_T2_PERCT;		"The percentage of Longest Dimension (LD) change between T1 and T2"
	float LD_T1_T3_PERCT;		"The percentage of Longest Dimension (LD) change between T1 and T3"
	float LD_T1_T4_PERCT;		"The percentage of Longest Dimension (LD) change between T1 and T4"
	float LD_T2_T3_PERCT;		"The percentage of Longest Dimension (LD) change between T2 and T3"
	float LD_T2_T4_PERCT;		"The percentage of Longest Dimension (LD) change between T2 and T4"
	float LD_T3_T4_PERCT;		"The percentage of Longest Dimension (LD) change between T3 and T4"
	string Mri_Pattern_Code;		"MRI Pattern Code"
	string Mri_Pattern_Desc;		"MRI Pattern Description"
	)

TABLE cdna
	"Table cdna"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string Cdna_T1;		"CDNA at T1"
	string Cdna_T2;		"CDNA at T2"
	string Cdna_T3;		"CDNA at T3"
	string Cdna_T4;		"CDNA at T4"
	)

TABLE agi
	"Table agi"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string Agi_T1;		"AGI at T1"
	string Agi_T2;		"AGI at T2"
	string Agi_T3;		"AGI at T3"
	string Agi_T4;		"AGI at T4"
	)

TABLE ihc
	"Table ihc"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string Ihc_T1;		"IHC at T1"
	string Ihc_T2;		"IHC at T2"
	string Ihc_T3;		"IHC at T3"
	string Ihc_T4;		"IHC at T4"
	)

TABLE fish
	"Table fish"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string Fish_T1;		"FISH at T1"
	string Fish_T2;		"FISH at T2"
	string Fish_T3;		"FISH at T3"
	string Fish_T4;		"FISH at T4"
	)

TABLE labTrack
	"Table labTrack"
	(
	string ispyId;		"I-SPY identifier uniquely corresponds 1 to 1 to the CALGB patient identifier"
	string trackId;		"Lab Track ID"
	string coreType;		"Core Type"
	string timePoint;		"Time Point"
	string section;		"Section"
	)

