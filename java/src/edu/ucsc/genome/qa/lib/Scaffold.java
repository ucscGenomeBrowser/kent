package edu.ucsc.genome.qa.lib;
import java.util.ArrayList;
import java.util.Iterator;

/* This container will hold the contigs for a scaffold */

public class Scaffold {

  // data
  public String name;
  public int startPos;
  public int endPos;
  public ArrayList contigs;

  // constructors
  // create with initial contig value
  // endPos will usually be updated
  public Scaffold(String nameVar, int startPosVar, int endPosVar, Contig contigVar) {
    name = nameVar;
    startPos = startPosVar;
    endPos = endPosVar;
    contigs = new ArrayList();
    contigs.add(contigVar);
  }

  public void updateEndPos(int endPosVar) {
    endPos = endPosVar;
  }
 
  public void addContig(Contig contigVar) {
    contigs.add(contigVar);
  }

  void print() {
    ArrayList al = contigs;
    Iterator iter = al.iterator();
    System.out.println(name);
    System.out.println("startPos = " + startPos);
    System.out.println("endPos = " + endPos);
    while (iter.hasNext()) {
      Contig contig = (Contig) iter.next();
      System.out.println("  " + contig.contigName);
    }
  }

}
