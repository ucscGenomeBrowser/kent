#!/usr/bin/env python3
"""Extract the Dixon 2012 and Schmitt 2016 TAD calls from the journal supplement
spreadsheets into per-sample BED files.  refs #21599

usage: extractTadsSupplements.py dixonTableS3.xls schmittTableS3.xlsx outDir

Writes, in sheet row order (sorting happens in the build script):
  outDir/dixon_hESC.hg18.bed, dixon_IMR90.hg18.bed     (BED4, name = cell type)
  outDir/mm9_mesc_domains.bed, mm9_cortex_domains.bed  (BED4, name = dom<N>, for buildDixonMouse.sh)
  outDir/schmitt_<CODE>.hg19.bed for the 21 sheets     (BED4, name = sheet code)
"""
import sys, os
import xlrd, openpyxl

def writeBed(path, rows, nameFn):
    with open(path, "w") as f:
        for i, (chrom, start, end) in enumerate(rows):
            f.write("%s\t%d\t%d\t%s\n" % (chrom, int(start), int(end), nameFn(i)))

def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    dixonXls, schmittXlsx, outDir = sys.argv[1:]
    os.makedirs(outDir, exist_ok=True)
    book = xlrd.open_workbook(dixonXls)
    for sheet, out, nameFn in [
            ("hESC Combined", "dixon_hESC.hg18.bed", lambda i: "hESC"),
            ("IMR90 Combined", "dixon_IMR90.hg18.bed", lambda i: "IMR90"),
            ("mESC Combined", "mm9_mesc_domains.bed", lambda i: "dom%d" % i),
            ("Cortex Combined", "mm9_cortex_domains.bed", lambda i: "dom%d" % i)]:
        sh = book.sheet_by_name(sheet)
        rows = [sh.row_values(r)[:3] for r in range(sh.nrows)]
        rows = [r for r in rows if str(r[0]).startswith("chr")]
        writeBed(os.path.join(outDir, out), rows, nameFn)
    wb = openpyxl.load_workbook(schmittXlsx, read_only=True)
    for code in wb.sheetnames:
        rows = [r[:3] for r in wb[code].iter_rows(values_only=True)
                if r[0] is not None and str(r[0]).startswith("chr")]
        writeBed(os.path.join(outDir, "schmitt_%s.hg19.bed" % code), rows, lambda i: code)

if __name__ == "__main__":
    main()
