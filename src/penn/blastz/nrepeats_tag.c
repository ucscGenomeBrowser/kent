/* nrepeats.tag.c -- make tag file from RepeatMasker output
	"-primate" asks for primate-specific repeats, i.e., types of repeats
		found in primates but not rodents (but perhaps found in other
		mammals).
	"-rodent" asks for rodent-specific repeats,
	else asks for repeats that are neither primate- nor rodent-specific

	Some elements identified by RepeatMasker are hard-masked in both human
	and mouse sequences, namely:
		Satellite, Simple_repeat and Low_complexity elements
		"Recent" RNA genes (e.g., snRNAs), i.e., any RNA gene in a
			rodent sequence or a RNA in a human sequence that
			diverges at most 25% from the consensus.
		"Recent" L1 sequences that are not assigned to a particular
			type, i.e., those diverging at most 20% from the
			consensus sequence.
	All repeats of type SINE/Alu are considered -primate (though supposedly
		a handful of really old copies are shared with rodents).
	A 315 other human-specific types and 155 rodent-specific types are
		explicitly named.
	if the last command-line argument is "verbose", then RepeatMasker lines
		are echoed, instead of the repeat_tag form of output
*/


#include "util.h"

enum {
  NAME, TYPE, BUF_SIZE=500
};

char *primate[] = {
	"BaEV",
	"BC200",
	"Charlie3",
	"Charlie5",
	"FAM",
	"FLAM_A",
	"FLAM_C",
	"FRAM",
	"Harlequin",
	"HERVL-A1",
	"HERVL-A1-A1",
	"HERVL-A2",
	"HERVL-A2-A1",
	"HERVL-A2-A2",
	"HERV15",
	"HERV17",
	"HERV3",
	"HERV70",
	"HERV9",
	"HERVE",
	"HERVFH19",
	"HERVFH21",
	"HERVH",
	"HERVH48",
	"HERVI",
	"HERVIP10F",
	"HERVIP10FH",
	"HERVK",
	"HERVK11",
	"HERVK11D",
	"HERVK13",
	"HERVK14",
	"HERVK14C",
	"HERVK22",
	"HERVK3",
	"HERVK9",
	"HERVKC4",
	"HERVL",
	"HERVL18",
	"HERVL66",
	"HERVP71A",
	"HERVR",
	"HERVS71",
	"HSMAR1",
	"HSMAR2",
	"HUERS-P1",
	"HUERS-P2",
	"HUERS-P3",
	"HUERS-P3b",
	"L1HS",
	"L1M1",
	"L1M1",
	"L1M2",
	"L1M2a",
	"L1M3a",
	"L1M3b",
	"L1M3de",
	"L1M3f",
	"L1MA1",
	"L1MA2",
	"L1MA3",
	"L1MA4",
	"L1MA4A",
	"L1MA5",
	"L1MA5A",
	"L1MA6",
	"L1P",
	"L1P1",
	"L1P2",
	"L1P3",
	"L1P3",
	"L1P3b",
	"L1P4",
	"L1P5",
	"L1PA",
	"L1PA10",
	"L1PA11",
	"L1PA12",
	"L1PA12",
	"L1PA13",
	"L1PA13",
	"L1PA14",
	"L1PA15",
	"L1PA15-16",
	"L1PA16",
	"L1PA17",
	"L1PA2",
	"L1PA3",
	"L1PA4",
	"L1PA5",
	"L1PA6",
	"L1PA7",
	"L1PA8",
	"L1PA8A",
	"L1PB",
	"L1PB1",
	"L1PB2",
	"L1PB3",
	"L1PB4",
	"L1PBa",
	"L1PBb",
	"LOR1",
	"LOR1a",
	"LOR1b",
	"LTR1",
	"LTR10",
	"LTR10A",
	"LTR10B",
	"LTR10B1",
	"LTR10C",
	"LTR10D",
	"LTR10E",
	"LTR10F",
	"LTR11",
	"LTR12",
	"LTR13",
	"LTR13A",
	"LTR14",
	"LTR14A",
	"LTR14B",
	"LTR14C",
	"LTR15",
	"LTR17",
	"LTR18",
	"LTR18A",
	"LTR18B",
	"LTR19",
	"LTR19A",
	"LTR19B",
	"LTR19C",
	"LTR1B",
	"LTR2",
	"LTR21",
	"LTR21A",
	"LTR21B",
	"LTR22",
	"LTR22A",
	"LTR22B",
	"LTR23",
	"LTR24",
	"LTR24B",
	"LTR24C",
	"LTR25",
	"LTR26",
	"LTR26B",
	"LTR26E",
	"LTR27",
	"LTR2B",
	"LTR2C",
	"LTR3",
	"LTR30",
	"LTR31",
	"LTR35",
	"LTR36",
	"LTR38",
	"LTR38B",
	"LTR39",
	"LTR3A",
	"LTR3B",
	"LTR3B1",
	"LTR4",
	"LTR42",
	"LTR43",
	"LTR45",
	"LTR45B",
	"LTR46",
	"LTR47",
	"LTR47A",
	"LTR47B",
	"LTR48",
	"LTR48B",
	"LTR49",
	"LTR5",
	"LTR5_Hs",
	"LTR51",
	"LTR54",
	"LTR54B",
	"LTR56",
	"LTR57",
	"LTR5B",
	"LTR6",
	"LTR60",
	"LTR61",
	"LTR62",
	"LTR64",
	"LTR64a",
	"LTR66",
	"LTR6A",
	"LTR6B",
	"LTR7",
	"LTR70",
	"LTR71",
	"LTR71A",
	"LTR8",
	"LTR8A",
	"LTR9",
	"LTR9B",
	"MADE1",
	"MER1",
	"MER101",
	"MER101B",
	"MER107",
	"MER11",
	"MER11A",
	"MER11B",
	"MER11C",
	"MER11D",
	"MER1A",
	"MER1B",
	"MER21",
	"MER21A",
	"MER21B",
	"MER30",
	"MER30B",
	"MER31A",
	"MER34B",
	"MER39",
	"MER39B",
	"MER4",
	"MER41",
	"MER41A",
	"MER41B",
	"MER41C",
	"MER41D",
	"MER41E",
	"MER46",
	"MER46A",
	"MER46B",
	"MER46C",
	"MER48",
	"MER4A",
	"MER4A1",
	"MER4B",
	"MER4C",
	"MER4D",
	"MER4E",
	"MER4E1",
	"MER51",
	"MER51A",
	"MER51B",
	"MER52C",
	"MER57",
	"MER57A",
	"MER57B",
	"MER6",
	"MER6A",
	"MER6B",
	"MER61",
	"MER61A",
	"MER61B",
	"MER61C",
	"MER61D",
	"MER61E",
	"MER65",
	"MER65A",
	"MER65B",
	"MER65C",
	"MER66",
	"MER66A",
	"MER66B",
	"MER7",
	"MER72",
	"MER73",
	"MER75",
	"MER75B",
	"MER7A",
	"MER7B",
	"MER7C",
	"MER83",
	"MER83B",
	"MER83C",
	"MER84",
	"MER85",
	"MER9",
	"MER99",
	"MLT1A",
	"MLT1A1",
	"MLT1A2",
	"MLT2A",
	"MLT2A1",
	"MLT2A2",
	"MST",
	"MSTA",
	"MSTB",
	"MSTB1",
	"MSTB2",
	"MSTC",
	"MSTD",
	"PABL_A",
	"PABL_B",
	"PMER1",
	"PRIMA4_LTR",
	"PRIMA4",
	"PRIMA41",
	"PRIMAX",
	"pTR5",
	"SVA",
	"THE1",
	"THE1A",
	"THE1B",
	"THE1C",
	"Tigger1",
	"Tigger2",
	"Tigger2a",
	"Tigger3",
	"Tigger3a",
	"Tigger3b",
	"Tigger3c",
	"Tigger3(Golem)",
	"Tigger4",
	"Tigger4a",
	"Tigger4b",
	"Tigger4(Zombi)",
	"Tigger6a",
	"Tigger6b"
};

char *rodent[] = {
	"4.5SRNA",
	"B1F",
	"B1_MM",
	"B2L_S",
	"B2_Mm1",
	"B2_Mm2",
	"B3",
	"B3A",
	"B4",
	"B4A",
	"BC1_MM",
	"BGLII",
	"CYRA11_MM",
	"ETnERV",
	"ETnERV2",
	"ETnERV3",
	"FAM",
	"IAP-d",
	"IAPEy",
	"IAPEY_LTR",
	"IAPEz",
	"IAPLTR1_MM",
	"IAPLTR2_MM",
	"IAPLTR_MA",
	"ID1_MM",
	"ID2",
	"ID3",
	"ID4",
	"ID5",
	"ID6",
	"ID_B1",
	"ID_RN",
	"L1F",
	"L1R2_RN",
	"L1VL1",
	"L1VL2",
	"L1_CC",
	"L1_CP",
	"L1_Cric",
	"L1_MM",
	"L1_RN",
	"LLME",
	"LTRIS_MM",
	"Lx",
	"Lx2",
	"Lx2B",
	"Lx3",
	"Lx4",
	"Lx5",
	"Lx6",
	"Lx7",
	"Lx8",
	"Lx9",
	"MERVL",
	"MERVL_LTR",
	"MMAR1",
	"MMERGLN",
	"MMETn",
	"MMURS",
	"MMVL30",
	"MT",
	"MT2",
	"MT2A",
	"MT2B",
	"MT2C",
	"MTA",
	"MTB",
	"MTC",
	"MTD",
	"MTE",
	"MULV",
	"MURVY",
	"MYSERV",
	"MYSPL",
	"MYS_LTR",
	"NICER_RN",
	"ORR1",
	"ORR1A",
	"ORR1A1",
	"ORR1A2",
	"ORR1A3",
	"ORR1B",
	"ORR1B1",
	"ORR1B2",
	"ORR1C",
	"ORR1D",
	"PB1",
	"PB1D10",
	"PB1D7",
	"PB1D9",
	"RAL_RN",
	"RLTR1",
	"RLTR10",
	"RLTR10B",
	"RLTR10C",
	"RLTR10D",
	"RLTR11A",
	"RLTR11B",
	"RLTR12",
	"RLTR13A",
	"RLTR13B",
	"RLTR13Ba",
	"RLTR13C",
	"RLTR13D",
	"RLTR14",
	"RLTR2_MM",
	"RLTR3_MM",
	"RLTR4_MM",
	"RLTR5_MM",
	"RLTR6_MM",
	"RLTR7_RN",
	"RLTR8",
	"RMER9",
	"RLTR9A",
	"RLTR9B",
	"RLTR9C",
	"RLTRETN_MM",
	"RLTRMRS_PS",
	"RLTRSRC_MA",
	"RMER10",
	"RMER10A",
	"RMER10B",
	"RMER12",
	"RMER13",
	"RMER13A",
	"RMER13B",
	"RMER14",
	"RMER15",
	"RMER16",
	"RMER17",
	"RMER17A",
	"RMER17A2",
	"RMER17B",
	"RMER17C",
	"RMER19",
	"RMER19A",
	"RMER19B",
	"RMER1",
	"RMER1A",
	"RMER1B",
	"RMER2",
	"RMER20",
	"RMER3",
	"RMER4",
	"RMER4B",
	"RMER5",
	"RMER6",
	"RMER6A",
	"RMER6B",
	"RSINE1",
	"URR1",
	"URR1A",
	"URR1B",
	"YREP_MM",
	"ZP3AR_MM"
};

struct foo {
	int field;
	const char *substr;
	const char *val;
} Rpt[] = {
	/* If the string in col #2 is a substring of the field named by col #1,
	   then its PipMaker name is in col #3.  Use the first case that holds.
	*/
	{ NAME, "Alu",     "Alu" },
	{ NAME, "MIR",     "MIR" },
	{ TYPE, "L2",      "LINE2" },
	{ TYPE, "L1",      "LINE1" },
	{ TYPE, "LTR",     "LTR" },
	{ TYPE, "ERV",     "LTR" },
	{ NAME, "LTR",     "LTR" },
	{ NAME, "HERV",    "LTR" },
	{ TYPE, "DNA",     "DNA" },
	{ NAME, "B1",      "B1" },
	{ TYPE, "B2",      "B2" },
	{ TYPE, "SINE",    "SINE" },
	{ TYPE, "LINE",    "Other" },
	{ NAME, "MML",     "Other" },
	{ NAME, "BUR1",    "Other" },
	{ TYPE, "Other",   "Other" },
	{ TYPE, "Unknown", "Other" },
	{ TYPE, "RNA",     "RNA" }
};

int main(int argc, char **argv)
{
	char buf[BUF_SIZE], line[BUF_SIZE], *name, *type, *p, *q;
	int i, from, to, seq_len = 0, nr = 0, nl = 0, underlay = 0;
	int nrpts = sizeof(Rpt) / sizeof(Rpt[0]);
	int nprimate = sizeof(primate)/sizeof(char *);
	int nrodent = sizeof(rodent)/sizeof(char *);
	int want_rodent = 0, want_primate = 0, is_primate, is_rodent,
		verbose = 0;
	float substitutions, deletions, insertions, divergence;
	FILE *fp;

	argv0 = "nrepeats_tag";
	if (same_string(argv[argc-1], "underlay")) {
		underlay = 1;
		--argc;
	}
	if (same_string(argv[argc-1], "verbose")) {
		verbose = 1;
		--argc;
	}
	if (argc == 3) {
		if (same_string(argv[1], "-primate"))
			want_primate = 1;
		else if (same_string(argv[1], "-rodent"))
			want_rodent = 1;
		else
			fatalf("bad argument: %s", argv[1]);
	} else if (argc != 2)
	     fatalf("args = [-rodent] [-primate] RepeatMasker-file [verbose]\n");

	if (same_string(argv[argc-1], "-"))
		fp = stdin;
	else
		fp = ckopen(argv[argc-1], "r");

/*
  0   1   2   3     4 5  6       7 8   9       10    11 12 13
413 5.6 0.0 0.0 HUMAN 1 54 (92195) C Alu SINE/Alu (238) 62 9
*/
	printf("%%:repeats\n");
	while (fgets(buf, sizeof(buf), fp)) {
		char key;
		const char *wsp = " \t\n";

		++nl;
		/* Expect field[0] to be an integer */
		for (p = buf; *p == ' '; ++p)
			;
		if (!isdigit(*p))
			continue;

		/* Skip field[0..4] */
		strcpy(line, buf);
		if ((p = strtok(line, wsp)) == 0) continue;
		if ((p = strtok(0, wsp)) == 0) continue;
		substitutions = atof(p);
		if ((p = strtok(0, wsp)) == 0) continue;
		deletions = atof(p);
		if ((p = strtok(0, wsp)) == 0) continue;
		insertions = atof(p);
		if ((p = strtok(0, wsp)) == 0) continue;
		divergence = substitutions;	/* add indels?? */

		++nr;

		/* Expect field[5] and field[6] to be integers */
		if ((p = strtok(0, wsp)) == 0) continue;
/* extra field in Golden Path repeats file */
		if (fp == stdin)
			if ((p = strtok(0, wsp)) == 0) continue;
		if (sscanf(p, "%d", &from) != 1)
			fatalf("failed to convert start-point: %s", p);

		if ((p = strtok(0, wsp)) == 0) continue;
		if (sscanf(p, "%d", &to) != 1)
			fatalf("failed to convert end-point: %s", buf);

		if (from <= -1 || from >= to) {
			fprintf(stderr, "Addresses out of order: %d %d\n",
				from, to);
			continue;
		}
		if (seq_len > 0 && to > seq_len) {
			fprintf(stderr, "repeat position is %d ", to);
			fprintf(stderr, "whereas sequence length is %d\n",
				seq_len);
			fatal("error in seq1 or RepeatMasker file.");
		}
		/* Skip field[7] */
		if ((p = strtok(0, wsp)) == 0) continue;

		/* Expect field[8] to be "+", "-" or "C" */
		if ((p = strtok(0, wsp)) == 0) continue;
		key = *p;
		if (key != '+' && key != '-' && key != 'C')
			fatalf("%s\nImproper format of RepeatMasker file.",buf);

		/* Expect field[9] to be a name */
		if ((p = strtok(0, wsp)) == 0) continue;
		name = p;

		/* delete "-int" at end of any name */
		q = name + strlen(name) - 4;
		if (same_string(q, "-int"))
			*q = '\0';

		if (same_string(name, "L1")) {
		    if (((want_rodent || want_primate) && divergence <= 20.0) ||
			(!want_rodent && !want_primate && divergence > 20.0)) {
				if (verbose)
					printf("%s", buf);
				else
					printf("%d %d %s LINE1\n", from, to,
			   		(key == '+') ? "Right" : "Left");
			}
			continue;
		}

		/* Expect field[10] to be a type */
		if ((p = strtok(0, wsp)) == 0) continue;
		type = p;

		if ((strstr(type,"Low") || strstr(type,"Simple") ||
	     	   strstr(type,"Satellite"))) {
			if (want_primate || want_rodent) {
				if (verbose)
					printf("%s", buf);
				else
					printf("%d %d Simple\n", from, to);
			}
			continue;
		}

		if (strstr(type, "RNA")) {
			if (want_rodent ||
			   (want_primate && divergence <= 25.0) ||
			   (!want_rodent && !want_primate
			      && divergence > 25.0)) {
				if (verbose)
					printf("%s", buf);
				else
					printf("%d %d %s RNA\n", from, to,
			   		(key == '+') ? "Right" : "Left");
			}
			continue;
		}

		is_primate = 0;
		if (strncmp(name, "Alu", 3) == 0)
			is_primate = 1;
		for (i = 0; i < nprimate; ++i)
			if (!strncmp(name, primate[i], strlen(primate[i])))
				break;
		if (i < nprimate)
			is_primate = 1;

		for (i = 0; i < nrodent; ++i)
			if (!strncmp(name, rodent[i], strlen(rodent[i])))
				break;
		is_rodent = (i < nrodent);

		if (want_primate && !is_primate)
			continue;
		if (want_rodent && !is_rodent)
			continue;
		if (!want_primate && !want_rodent && (is_primate || is_rodent))
			continue;

		for (i = 0; i < nrpts; ++i)
			if ( (Rpt[i].field==NAME && strstr(name, Rpt[i].substr))
			 || (Rpt[i].field==TYPE && strstr(type, Rpt[i].substr)))
				break;
		if (i == nrpts) {
			fprintf(stderr, "Unknown repeat at %d-%d, ", from, to);
			fprintf(stderr, "name = %s and type = %s\n", name,type);
		} else if (verbose)
			printf("%s", buf);
		else if (underlay)
			printf("%d %d Ancient_repeat\n", from, to);
		else
			printf("%d %d %s %s\n", from, to,
			   (key == '+') ? "Right" : "Left", Rpt[i].val);
	}
	if (fp != stdin)
		fclose(fp);
	if (nl > 0 && nr == 0)
		fatal("no repeat elements were specified.");
	return 0;
}
