#!/usr/bin/env python3
"""Dump the sheets we need from the Rosenski 2025 supplementary workbook
(41467_2025_57433_MOESM4_ESM.xlsx) to plain TSV files.

Every sheet in that workbook has a one-line title in row 1, the column header in
row 2 and the data from row 3 on, so we skip row 1 and treat row 2 as the header.

Usage: kaplanImprintXlsxToTsv.py <workbook.xlsx> <outDir>
"""
import sys, os, importlib.abc

class _NoDefusedXml(importlib.abc.MetaPathFinder):
    """openpyxl prefers defusedxml, but the defusedxml installed on hgwdev is not
    compatible with this python/openpyxl combination and dies in iterparse. Hide
    it so openpyxl falls back to the standard library ElementTree."""
    def find_spec(self, name, path=None, target=None):
        if name.split(".")[0] == "defusedxml":
            raise ImportError(name)

sys.meta_path.insert(0, _NoDefusedXml())
import openpyxl

# sheet name in the workbook -> output file name
SHEETS = {
    "S2. iDMR bimodal boundari"  : "dataS2_icr.tsv",
    "S3. ASM SNPs"               : "dataS3_asmSnps.tsv",
    "S4. Parental-ASM SNPs"      : "dataS4_parentalAsmSnps.tsv",
    "S10. All imprinted genes m" : "dataS10_imprintedGenes.tsv",
    "S16. Escape of imprinting"  : "dataS16_escape.tsv",
    "Sheet24"                    : "dataS_gameteMeth.tsv",
}

def main():
    xlsxFname, outDir = sys.argv[1], sys.argv[2]
    os.makedirs(outDir, exist_ok=True)
    wb = openpyxl.load_workbook(xlsxFname, read_only=True)
    for sheetName, outName in SHEETS.items():
        ws = wb[sheetName]
        outPath = os.path.join(outDir, outName)
        rowCount = 0
        with open(outPath, "w") as ofh:
            for i, row in enumerate(ws.iter_rows(values_only=True)):
                if i == 0:                       # the sheet title line
                    continue
                cells = ["" if c is None else str(c).strip() for c in row]
                while cells and cells[-1] == "":  # trailing empty columns
                    cells.pop()
                if not cells:
                    continue
                ofh.write("\t".join(c.replace("\t", " ") for c in cells) + "\n")
                rowCount += 1
        print("%s: %d rows (incl. header) -> %s" % (sheetName, rowCount, outPath))

main()
