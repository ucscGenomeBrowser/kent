package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;

/**
 *  Keeps chromosome information.
 *
 */
public class ChromLibrary {

 /**
  *  Gets the smallest chromosome for the assembly.
  *
  *  @param   chroms  The list of chromosomes in the assembly.
  *  @param   debug   Run debugger if true.  Prints to command line.
  *
  *  @return The smallest chromsome for the assembly.
  */
  public static ChromInfo getSmallestChrom(ArrayList chroms, boolean debug) {

    //  set start at 100 million and no chromname
    ChromInfo smallest = new ChromInfo("placeholder", 100000000);

    // iterate through chroms looking for smaller chroms
    Iterator chromIter = chroms.iterator();
    while (chromIter.hasNext()) {
      ChromInfo ci = (ChromInfo) chromIter.next();
      if (debug) {
        System.out.println("size = " + ci.size);
      }
      if (ci.size < smallest.size) {
        smallest = ci;
      }
    }

    return (smallest);

  }
}
