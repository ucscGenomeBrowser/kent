package edu.ucsc.genome.qa.lib;
import java.io.*;
import java.util.*;


/*
 * Open and print to error file
 *
 */
public class ErrorFile {

 /*
  *  Makes Buffered PrintWriters for extensions: 
  *    .ok, .errors., msg, .msg2.
  *  Format = genome.process.ext
  * 
  *  @param msgFile    The string to look for in HTML stream.
  *  @param msg2File   The second string to look for in HTML stream.
  *  @returns   PrintWriters for output files.
  */
  public static ArrayList errorFile (String assembly, String process,
                                    String msgFile, String msg2File) {

    String errorFile = new String(assembly + "." + 
                                 process + ".errors");
    String outFile   = new String(assembly + "." +
                                 process + ".ok");
    String msgFile   = new String(assembly + "." +
                                 process + ".msg");
    String msg2File  = new String(assembly + "." +
                                 process + ".msg2");

    try {
      FileWriter     fwOk   = new FileWriter(outFile);
      BufferedWriter bwOk   = new BufferedWriter(fwOk);
      PrintWriter    outOk  = new PrintWriter(bwOk);

      FileWriter     fwErr  = new FileWriter(errorFile);
      BufferedWriter bwErr  = new BufferedWriter(fwErr);
      PrintWriter    outErr = new PrintWriter(bwErr);

      FileWriter     fwMsg  = new FileWriter(msgFile);
      BufferedWriter bwMsg  = new BufferedWriter(fwMsg);
      PrintWriter    outMsg = new PrintWriter(bwMsg);

      FileWriter     fwMsg2  = new FileWriter(msg2File);
      BufferedWriter bwMsg2  = new BufferedWriter(fwMsg2);
      PrintWriter    outMsg2 = new PrintWriter(bwMsg2);

    } catch (IOException e) {
        System.out.println("Couldn't open output file:" + errorFile);
        System.out.println("   or                     " +   outFile);
    }
  }

  public static void closeFiles (){
      outOk.close();
      outErr.close();
      outMsg.close();
      outMsg2.close();
    }
}
