package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/**
 *  This container holds the rows from the chromInfo table 
 */
public class ChromInfo {

  // data
  public String chrom;
  public int size;

  // constructors
 /**
  *  Constructor creates the object of a chromosome, size pair.
  *
  *  @param chromVar  The chromosome number.
  *  @param sizeVar   The size of that chromosome.
  */
  public ChromInfo(String chromVar, int sizeVar) {

    chrom = chromVar;
    size = sizeVar;
  }

 /**
  *  Checks for certain out-of-bounds values in chrom and size.
  *
  *  @return  True if both parameters are within bounds.
  */
  public boolean validate() {
    if (chrom == null) return false;
    if (size <= 0) return false;
    return true;
  }
}
