#!/usr/bin/env python3
"""Convert NMDetective-AI BED9+6 into a colored BED9+7 ready for bigBed.

Input columns (NMDetectiveAI_MANE.bed):
    chrom chromStart chromEnd name score strand thickStart thickEnd itemRgb
    prediction geneName transcriptId aaPosition refCodon mutCodon

Output columns:
    same nine BED9 fields with itemRgb computed from prediction, score
    rescaled into [0,1000], plus the six annotation columns and a mouseOver.
"""
import sys


def color_for(pred: float) -> str:
    """Diverging Okabe-Ito ramp: blue (NMD-evading) -> grey -> vermillion (triggering)."""
    EVADE = (0, 114, 178)
    TRIG = (213, 94, 0)
    GREY = (200, 200, 200)
    if pred <= 0:
        t = min(1.0, -pred / 1.0)
        r = int(GREY[0] + (EVADE[0] - GREY[0]) * t)
        g = int(GREY[1] + (EVADE[1] - GREY[1]) * t)
        b = int(GREY[2] + (EVADE[2] - GREY[2]) * t)
    else:
        t = min(1.0, pred / 1.0)
        r = int(GREY[0] + (TRIG[0] - GREY[0]) * t)
        g = int(GREY[1] + (TRIG[1] - GREY[1]) * t)
        b = int(GREY[2] + (TRIG[2] - GREY[2]) * t)
    return f"{r},{g},{b}"


PRED_MIN = -1.2
PRED_MAX = 1.5


def remap_score(pred: float) -> int:
    s = int(round((pred - PRED_MIN) * 1000.0 / (PRED_MAX - PRED_MIN)))
    return max(0, min(1000, s))


def main(in_path: str, out_path: str) -> None:
    kept = 0
    skipped = 0
    bad_codon = 0
    with open(in_path) as fh, open(out_path, "w") as out:
        for line in fh:
            if line.startswith("#"):
                continue
            f = line.rstrip("\n").split("\t")
            if len(f) != 15:
                skipped += 1
                continue
            (chrom, start, end, name, _score, strand,
             tstart, tend, _rgb,
             pred_s, gene, tx, aa, refc, mutc) = f
            try:
                pred = float(pred_s)
            except ValueError:
                skipped += 1
                continue
            if mutc not in ("TAA", "TAG", "TGA"):
                bad_codon += 1
            rgb = color_for(pred)
            score = remap_score(pred)
            verdict = (
                "predicted NMD-triggering"
                if pred >= 0.43
                else (
                    "predicted NMD-evading"
                    if pred <= -0.17
                    else "intermediate / uncertain"
                )
            )
            mouse = (
                f"{gene} {refc}&gt;{mutc} at codon {aa}<br>"
                f"NMDetective-AI prediction: {pred:.3f} ({verdict})<br>"
                f"Transcript: {tx}"
            )
            # Upstream name is "<gene>:<refCodon><aa><mutCodon>"; drop the gene
            # prefix so the label shows just the codon change (e.g. AAG2TAG).
            short_name = name.split(":", 1)[1] if ":" in name else name
            out.write("\t".join([
                chrom, start, end, short_name, str(score), strand,
                tstart, tend, rgb,
                f"{pred:.4f}", gene, tx, aa, refc, mutc, mouse,
            ]) + "\n")
            kept += 1
    sys.stderr.write(
        f"kept={kept} skipped={skipped} non_stop_mut={bad_codon}\n"
    )


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: makeBigBed.py <in.bed> <out.bed>")
    main(sys.argv[1], sys.argv[2])
