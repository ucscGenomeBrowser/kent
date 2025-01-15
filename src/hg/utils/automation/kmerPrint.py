#!/cluster/software/bin/python3

import sys
import gzip

def countKmers(sequence, chrName, kmerSize, chrStart):
    """Count and print kmers from a sequence."""
    sequenceLen = len(sequence)
    for i in range(sequenceLen - kmerSize + 1):
        kmer = sequence[i:i + kmerSize]
        if all(base in "ACGT" for base in kmer):
            print(f"{kmer}\t{chrName}\t{chrStart}")
        chrStart += 1

def processFastaFiles(kmerSize, fastaFiles):
    """Process each FASTA file, reading sequences and generating kmers."""
    chrName = ""
    chrStart = 0
    for file in fastaFiles:
        sequence = ""
        if file == "stdin":
            fileHandle = sys.stdin
        elif file.endswith(".gz"):
            fileHandle = gzip.open(file, "rt")
        else:
            fileHandle = open(file, "r")

        with fileHandle as fh:
            for line in fh:
                line = line.strip()
                if not line or line.startswith("#"):  # Skip empty and comment lines
                    continue
                if line.startswith(">"):
                    if sequence:
                        countKmers(sequence, chrName, kmerSize, chrStart)
                        sequence = ""
                    chrName = line[1:]  # Remove ">"
                    chrStart = 0
                else:
                    sequence += line.upper()
            if sequence:
                countKmers(sequence, chrName, kmerSize, chrStart)
                sequence = ""

def main():
    """Main function to parse arguments and execute the script."""
    if len(sys.argv) < 3:
        sys.stderr.write(
            "usage: kmerPrint.py <N> <fastaFile.fa> [moreFastaFiles.fa] | gzip -c > result.gz\n"
            "where <N> is kmer size\n"
            "followed by fasta file names (can be 'stdin')\n"
        )
        sys.exit(255)

    kmerSize = int(sys.argv[1])
    fastaFiles = sys.argv[2:]

    processFastaFiles(kmerSize, fastaFiles)

if __name__ == "__main__":
    main()
