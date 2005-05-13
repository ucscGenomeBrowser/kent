package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/* This container will hold the rows from the liftOverTestCase table */

public class LiftOverTestCase {

  // data
  public String id;
  public String comment;
  public String origAssembly;
  public String newAssembly;
  public String origChrom;
  public int origStart;
  public int origEnd;
  public String status;
  public String message;
  public String newChrom;
  public int newStart;
  public int newEnd;

  // constructors
  public LiftOverTestCase(String idVar, String commentVar, 
            String origAssemblyVar, String newAssemblyVar,
            String origChromVar, int origStartVar, int origEndVar,
	    String statusVar, String messageVar, String newChromVar,
	    int newStartVar, int newEndVar) {

    id = idVar;
    comment = commentVar;
    origAssembly = origAssemblyVar;
    newAssembly = newAssemblyVar;
    origChrom = origChromVar;
    origStart = origStartVar;
    origEnd = origEndVar;
    status = statusVar;
    message = messageVar;
    newChrom = newChromVar;
    newStart = newStartVar;
    newEnd = newEndVar;
  }

}
