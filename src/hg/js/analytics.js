// analytics.js - Javascript for use in static html page footer code

// Copyright (C) 2020 The Regents of the University of California

document.body.onload = addGtag;

function addGtag() {
  var hostName = location.hostname;
  var analyticsKey = "G-EGJY1746VM";
  if (hostName.search("genome.ucsc") > -1) {
    analyticsKey = "G-P5EV0BL192";
  } else if (hostName.search("hgw[0-9]") > -1) {
    analyticsKey = "G-P5EV0BL192";
  } else if (hostName.search("genome-euro") > -1) {
    analyticsKey = "G-P5EV0BL192";
  } else if (hostName.search("genome-asia") > -1) {
    analyticsKey = "G-P5EV0BL192";
  } else if (hostName.search("genome-test") > -1) {
    analyticsKey = "G-RT83HV74PS";
  } else if (hostName.search("hgwdev.gi") > -1) {
    analyticsKey = "G-RT83HV74PS";
  } else if (hostName.search("hgwbeta") > -1) {
    analyticsKey = "G-X4EVCTYJ3E";
  } else if (hostName.search("genome-preview") > -1) {
    analyticsKey = "G-S8DG7ZEQDB";
  } else if (hostName.search("hgwalpha") > -1) {
    analyticsKey = "G-S8DG7ZEQDB";
  } else if (hostName.search("hgdownload") > -1) {
    analyticsKey = "G-PWFD1NPDNM";
  } else if (hostName.search("hgwdev-hiram") > -1) {
    analyticsKey = "G-GTG3B0HBYJ";
  }

  var gtagScript = document.createElement("script");
  gtagScript.src = "https://www.googletagmanager.com/gtag/js?id=";
  gtagScript.src += analyticsKey;
  document.body.appendChild(gtagScript);

  var gtagCall = document.createElement("script");
  var gtagCode = document.createTextNode(
'window.dataLayer = window.dataLayer || [];' +
'function gtag(){dataLayer.push(arguments);}' +
'gtag("js", new Date());' + 'gtag("config", "' + analyticsKey + '");'
  );
  gtagCall.appendChild(gtagCode);
  document.body.appendChild(gtagCall);
}
