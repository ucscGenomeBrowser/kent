package edu.ucsc.genome.qa.linkCheck;
import edu.ucsc.genome.qa.lib.*;
import java.io.*;
import java.net.*;
import com.meterware.httpunit.*;
import org.xml.sax.*;


/**
 * LinkCheck.java
 * Purpose: check all links on a web page and report ok links and
 *   error links in separate files
 * Accepts command line input:  path in htdocs, file[.html], [dateString]
 * Use zero for path if at top level
 * @author Heather Trumbower
 * @author Robert Kuhn
 * @version 1.3
 * @date 12/03/03
 */
class LinkCheck {
  public static void main(String[] args) {
    WebConversation wc = new WebConversation();

    /* path / file pairs

    String localPath = new String("");
    String file      = new String("downloads");
    String file      = new String("mirror");
    String file      = new String("cite");
    String file      = new String("staff");
    String file      = new String("contacts");

    String localPath = new String("license/");
    String file      = new String("index");

    String localPath = new String("goldenPath/");
    String file      = new String("gbdDescriptions");
    String file      = new String("credits");
    String file      = new String("pubs");
    String file      = new String("stats");
    String file      = new String("releaseLog");
    String file      = new String("newsarch");
    String file      = new String("textBrowser");

    String localPath = new String("goldenPath/customTracks/");
    String file      = new String("custTracks");

    String localPath = new String("goldenPath/help/");
    String file      = new String("hgTracksHelp");

    String localPath = new String("FAQ/");
    String file      = new String("index");
    String file      = new String("FAQblat");
    String file      = new String("FAQcite");
    String file      = new String("FAQcustom");
    String file      = new String("FAQdisplay");
    String file      = new String("FAQdownloads");
    String file      = new String("FAQlicense");
    String file      = new String("FAQlink");
    String file      = new String("FAQreleases");
    String file      = new String("FAQtracks");

     */

    // set parameters for naming file to check here and using as variable
    String localPath = "";
    String file      = "";
    String yymmmdd   = "today";

    if (args.length < 2) {
       System.out.println("\n  Checks all links on a web page.");
       System.out.println("  Usage: path in htdocs, file[.html], [dateString].");
       System.out.println("  Use zero for path if at htdocs level.\n");
       System.exit(-1);
     } else {
       // command line db
       if (args[0].equals("0")) {
         //  System.out.println("got a zero as input: " + args[0]);
         localPath = "";
       } else {
         // System.out.println("no zero as input: " + args[0]);
         localPath = args[0] + "/";
       }
       file      = args[1];
       
       if (args.length == 3) {
         yymmmdd   = args[2];
       }
       // clean off extra "/" if present
       localPath = localPath.replaceAll("//", "/");
     }

    // replace / with . for output file name
    String outputPath = localPath.replaceAll("/", ".");


    // System.out.println("localPath: " + localPath + "\n");
    // System.out.println("outputPath: " + outputPath + "\n");



    // take this as command line arg or read from data file
    String baseURL = "http://genome.ucsc.edu/" + localPath + file;
    if (! baseURL.endsWith("html")) {
      // System.out.println("no html ending");
      baseURL = baseURL + ".html";
    } else {
      // System.out.println("has html ending");
      // Will take *.shtml
      // clean off html or shtml for output filename
      file = file.replaceAll(".shtml", "");
      file = file.replaceAll(".html", "");
    }
    WebRequest req = new GetMethodWebRequest(baseURL);

    try {
      // open files for good links and bad
      FileWriter fw1     = new FileWriter(outputPath + file + "." + yymmmdd);
      FileWriter fw2     = new FileWriter(outputPath + file + "." + yymmmdd + ".errors");
      BufferedWriter bw1 = new BufferedWriter(fw1);
      BufferedWriter bw2 = new BufferedWriter(fw2);
      PrintWriter ok     = new PrintWriter(bw1);
      PrintWriter err    = new PrintWriter(bw2);

      int countskip = 0;
      int countok   = 0;
      int counterr  = 0;
      ok.println("Checking file: " + baseURL);

      // read from datafile
      WebResponse page = wc.getResponse(req);
      WebLink linkarray[] = page.getLinks();
      for (int i=0; i < linkarray.length; i++) {

        // get the page each time in the loop
        page = wc.getCurrentPage();

	// check the response code
	WebRequest detailreq = linkarray[i].getRequest();
        URL myURL = detailreq.getURL();
        String myProtocol = myURL.getProtocol();

        // skip "mailto" and other protocols
        if (myProtocol.equals("http") | myProtocol.equals("https")) {
          int code = Robot.getResponseCode(wc, linkarray[i]);
          System.out.println("Link = " + linkarray[i].asText());
          if (code == 200) {
             countok++;
	     ok.println("\n----------------------------------");
             ok.println("Link = " + linkarray[i].asText());
	     ok.println("          Response Code = " + code);
	     ok.println("----------------------------------");
           } else {
	     counterr++;
             if (counterr == 1) {
               err.println("Checking file: " + baseURL);
             }
	       err.println("\n----------------------------------");
               err.println("Link = " + linkarray[i].asText());
               err.println("URL = " + myURL);
	       err.println("          Response Code = " + code);
	       err.println("----------------------------------");
	     }
	} else {
	  countskip++;
            ok.println("\n----------------------------------");
            ok.println("Link = " + linkarray[i].asText());
	    ok.println("Skipping");
            ok.println("----------------------------------");
	}
      }
      System.out.println("---------------------------------");
      int count = linkarray.length;
      ok.println("Count of links checked  = " + count);
      ok.println("Count of ok links       = " + countok);
      ok.println("Count of error links    = " + counterr);
      ok.println("Count of skipped links  = " + countskip);
      if (counterr > 0) {
        err.println("Count of links checked = " + count);
        err.println("Count of error links   = " + counterr);
      }
      System.out.println("htdocs/" + outputPath + file);
      System.out.println("Count of error links   = " + counterr + "\n");
      ok.close();
      err.close();

    } catch (MalformedURLException e) {
      System.err.println(e.getMessage());
    } catch (SAXException e) {
      System.err.println(e.getMessage());
    } catch (IOException e) {
      System.err.println(e.getMessage());
    }
  }
}
