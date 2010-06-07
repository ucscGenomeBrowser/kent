package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/* Container for chrom, start, end */

public class Coords {

  // data
  public String chrom;
  public int start;
  public int end;

  // constructors
  public Coords(String chromVar, int startVar, int endVar) {

    chrom = chromVar;
    start = startVar;
    end = endVar;
  }

  public Coords(String filename) {
    FileReader fin;
    BufferedReader br;
    String thisline = "";
    int pos = 0;
    Integer myint;

    try {
      fin = new FileReader(filename);
      br = new BufferedReader(fin);
      // only need to read the first line
      thisline = br.readLine();
      fin.close();
    } catch (Exception e) {
      System.out.println(e.getMessage());
      chrom = "chr1";
      start = 1;
      end = 1;
      return;
    }
    pos = thisline.indexOf("\t");
    chrom = thisline.substring(0, pos);
    thisline = thisline.substring(pos+1);
    pos = thisline.indexOf("\t");
    String startstr = thisline.substring(0, pos);
    myint = new Integer(startstr);
    start = myint.intValue();
    thisline = thisline.substring(pos+1);
    myint = new Integer(thisline);
    end = myint.intValue();
  }

  public boolean match(String newChrom, int newStart, int newEnd) {
    if (!chrom.equals(newChrom)) return (false);
    if (start != newStart) return (false);
    if (end != newEnd) return (false);
    return (true);
  }

  public void print() {
    System.out.println(chrom + ":" + start + "-" + end);
  }

}
