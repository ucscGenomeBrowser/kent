package edu.ucsc.genome.qa.lib;

/* This container will hold the coords for a contig */

public class Contig {

  // data
  public String contigName;
  public int chromStart;
  public int chromEnd;
  public int fragStart;
  public int fragEnd;

  // constructors
  public Contig(String nameVar, int chromStartVar, int chromEndVar, int fragStartVar, int fragEndVar) {
    contigName = nameVar;
    chromStart= chromStartVar;
    chromEnd = chromEndVar;
    fragStart= fragStartVar;
    fragEnd = fragEndVar;
  }

}
