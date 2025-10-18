#!/usr/bin/env python3
import sys
import numpy as np

# from chatGPT
viridis_colors = [
    "68,1,84",
    "71,20,102",
    "71,37,117",
    "69,54,129",
    "63,69,135",
    "57,85,139",
    "50,98,141",
    "44,112,142",
    "39,124,142",
    "34,137,141",
    "31,150,139",
    "31,163,134",
    "41,175,127",
    "61,187,116",
    "85,198,102",
    "116,208,84",
    "149,215,63",
    "186,222,39",
    "220,226,24",
    "253,231,36"
]

allVals = []
allRows = []

for line in sys.stdin:
    if line.strip() == "" or line.startswith("transcript"):
        continue  # skip empty or header lines

    fields = line.strip().split("\t")

    # Parse fields
    chrom = "chr"+fields[3].split(":")[0]
    start = int(fields[3].split(":")[1])
    end = int(fields[4].split(":")[1])

    trans, gene, gene_id = fields[:3]
    start_aa, stop_aa, obs, exp, oe, chisq, pval = fields[5:]

    oe = float(oe)
    exp = float(exp)
    chisq = float(chisq)
    pval = float(pval)

    # Compute score
    #score = 1000 if oe > 1.0 else oe * 1000
    score = 0
    allVals.append(oe)

    # Print formatted output
    row = [chrom, start, end, gene+"-"+start_aa+"-"+stop_aa, str(int(score)), ".", start, end, "0", gene, gene_id, trans, start_aa, stop_aa, obs, exp, oe, chisq, pval]
    allRows.append(row)

viridis_colors = list(reversed(viridis_colors))

#allVals.sort(reverse=True)

# Compute quantile bins
numbers = allVals
num_colors = len(viridis_colors)
# Compute quantiles for bin edges
quantile_edges = np.quantile(numbers, np.linspace(0, 1, num_colors + 1))

# Assign each number to a quantile bin
quantile_indices = np.digitize(numbers, quantile_edges, right=True) - 1

# Clip indices to valid range
quantile_indices = np.clip(quantile_indices, 0, num_colors - 1)

# Annotate each number with corresponding Viridis color
valToCol = dict([(num, viridis_colors[idx]) for num, idx in zip(numbers, quantile_indices)])

for row in allRows:
    oe = row[-3]
    color = valToCol[oe]
    row = [str(x) for x in row]
    row[8] = color
    print("\t".join(row))

