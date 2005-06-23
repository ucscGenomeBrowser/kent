package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/* Container for chrom, start, end, sequence */

public class SequenceCheckTarget {

  // data
  public String chrom;
  public int start;
  public int end;
  public String sequence;

  // constructors
  public SequenceCheckTarget(String chromVar, int startVar, int endVar, String sequenceVar) {

    chrom = chromVar;
    start = startVar;
    end = endVar;
    sequence = sequenceVar;
  }

  public boolean match(String newChrom, int newStart, int newEnd, String newSequence) {
    if (!chrom.equals(newChrom)) return (false);
    if (start != newStart) return (false);
    if (end != newEnd) return (false);
    if (!sequence.equals(newSequence)) return (false);
    return (true);
  }

  public void print() {
    System.out.println(chrom + ":" + start + "-" + end);
  }

}
