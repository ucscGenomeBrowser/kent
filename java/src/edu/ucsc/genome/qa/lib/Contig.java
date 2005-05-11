package edu.ucsc.genome.qa.lib;

/* This container will hold the coords for a contig */

public class Contig {

  // data
  public String contigName;
  public int startPos;
  public int endPos;

  // constructors
  public Contig(String nameVar, int startVar, int endVar) {
    contigName = nameVar;
    startPos = startVar;
    endPos = endVar;
  }

}
