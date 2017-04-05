#include "BetaDecayTools.hh"
#include "DataTree.hh"
#include <TString.h>
#include <TH1D.h>
#include <TGraphErrors.h>
#include <TMath.h>
#include <TLeaf.h>

#include <iostream>
#include <fstream>


std::vector <int>  readOctetFile(int octet) {

  std::vector <int> runs;
  
  char fileName[200];
  sprintf(fileName,"%s/All_Octets/octet_list_%i.dat",getenv("OCTET_LIST"),octet);
  std::ifstream infile(fileName);
  std::string runTypeHold;
  int runNumberHold;
  int numRuns = 0;
  // Populate the map with the runType and runNumber from this octet
  while (infile >> runTypeHold >> runNumberHold) {

    if (runTypeHold=="A2" || runTypeHold=="A5" || runTypeHold=="A7" || runTypeHold=="A10" || 
	runTypeHold=="B2" || runTypeHold=="B5" || runTypeHold=="B7" || runTypeHold=="B10" )  {
     
      runs.push_back(runNumberHold);

    }
    numRuns++;
  }

  infile.close();
 
  std::cout << "Read in octet file for octet " << octet << "\n";
  return runs;

};

int main(int argc, char *argv[]) {

  
  if (argc!=2) {
    std::cout << "Usage: ./endpointGain.exe [octet]\n";
    //std::cout << "The code will produce comparisons for every octet in the range given,\non an octet-by-octet basis, and as a whole, using the Super-Sum\n";
    exit(0);
  }
  

  int octet = atoi(argv[1]);


  int nBins = 100;
  double minRange = 0.;
  double maxRange = 1000.;
  
  std::vector <int> runs = readOctetFile(octet);

  

  ////////////////// Begin with data files /////////////////////

  // Vectors for creating the individual runs events and errors
  std::vector < std::vector < std::vector <Double_t> > > pmtSpec;
  std::vector < std::vector < std::vector <Double_t> > > pmtSpecErr;
    
  pmtSpec.resize(runs.size(),std::vector<std::vector<Double_t>>(8,std::vector<Double_t>(nBins,0.)));
  pmtSpecErr.resize(runs.size(),std::vector<std::vector<Double_t>>(8,std::vector<Double_t>(nBins,0.)));

  // Now loop over each run to determine the individual spectra, then fill their appropriate bins in the vector

  TH1D *spec[8]; // All 8 PMTs signals
  TH1D *simspec[8]; // All 8 PMTs signals

  int nRun = 0;
  
  for ( auto rn : runs ) {

    for ( int i=0; i<8; ++i ) {
      spec[i] = new TH1D(TString::Format("PMT%i",i),TString::Format("PMT %i",i),
		      nBins, minRange, maxRange);
    }
    
    // DataTree structure
    DataTree t;

    // Input ntuple
    char tempIn[500];
    sprintf(tempIn, "%s/replay_pass3_%i.root", getenv("REPLAY_PASS3"),rn);
    
    t.setupInputTree(std::string(tempIn),"pass3");

    unsigned int nevents = t.getEntries();

    //t.getEvent(nevents-1);
    //totalTimeOFF += t.Time;
    //totalBLINDTimeOFF[0] += t.TimeE;
    //totalBLINDTimeOFF[1] += t.TimeW;

    double r2E = 0.; //position of event squared
    double r2W = 0.;
    
    for (unsigned int n=0 ; n<nevents ; n++ ) {

      t.getEvent(n);

      r2E = t.xE.center*t.xE.center + t.yE.center*t.yE.center;
      r2W = t.xW.center*t.xW.center + t.yW.center*t.yW.center;

      if ( t.PID==1 && t.Side<2 && t.Type==0 && t.Erecon>0. ) {

	if ( r2E > 50. || r2W > 50. ) continue;

	if ( t.Side==0 ) {
	  spec[0]->Fill(t.ScintE.e1);
	  spec[1]->Fill(t.ScintE.e2);
	  spec[2]->Fill(t.ScintE.e3);
	  spec[3]->Fill(t.ScintE.e4);
	}

	if ( t.Side==1 ) {
	  spec[4]->Fill(t.ScintW.e1);
	  spec[5]->Fill(t.ScintW.e2);
	  spec[6]->Fill(t.ScintW.e3);
	  spec[7]->Fill(t.ScintW.e4);
	}

      }
    }

    for ( int p=0; p<8; ++p ) {
      for ( int bin=1; bin<=nBins; ++bin ) {
	pmtSpec[nRun][p][bin-1] = (double)spec[p]->GetBinContent(bin);
	pmtSpecErr[nRun][p][bin-1] = spec[p]->GetBinError(bin);
      }
    }

    for ( int i=0; i<8; ++i ) { 
      // std::cout << "deleting spec[" << i << "]\n";
      delete spec[i];
    }
    nRun++;
    
  }

  

  ////////////// Now for simulation //////////////////
  // Vectors for creating the individual runs events and errors
  std::vector < std::vector < std::vector <Double_t> > > simSpec;
  std::vector < std::vector < std::vector <Double_t> > > simSpecErr;
    
  simSpec.resize(runs.size(),std::vector<std::vector<Double_t>>(8,std::vector<Double_t>(nBins,0.)));
  simSpecErr.resize(runs.size(),std::vector<std::vector<Double_t>>(8,std::vector<Double_t>(nBins,0.)));

  

  // Now loop over each run to determine the individual spectra, then fill their appropriate bins in the vector

  nRun = 0;
  
  for ( auto rn : runs ) {

    for ( int i=0; i<8; ++i ) {
      simspec[i] = new TH1D(TString::Format("SIM%i",i),TString::Format("SIM %i",i),
			 nBins, minRange, maxRange);
    }
    
    TFile *f = new TFile(TString::Format("%s/beta/revCalSim_%i_Beta.root",getenv("REVCALSIM"),rn), "READ");
    TTree *Tin = (TTree*)f->Get("revCalSim");

    std::cout << "Reading from " << TString::Format("%s/beta/revCalSim_%i_Beta.root",getenv("REVCALSIM"),rn).Data() << "\n";
    
    Double_t e0,e1,e2,e3,e4,e5,e6,e7;
    MWPC xE, yE, xW, yW;
    int PID, Side, Type;
    Double_t Erecon;
    
    
    Tin->SetBranchAddress("PID", &PID);
    Tin->SetBranchAddress("type", &Type);
    Tin->SetBranchAddress("side", &Side); 
    Tin->SetBranchAddress("Erecon",&Erecon);
    Tin->SetBranchAddress("xE",&xE);
    Tin->SetBranchAddress("yE",&yE);
    Tin->SetBranchAddress("xW",&xW);
    Tin->SetBranchAddress("yW",&yW);
    Tin->GetBranch("PMT")->GetLeaf("Evis0")->SetAddress(&e0);
    Tin->GetBranch("PMT")->GetLeaf("Evis1")->SetAddress(&e1);
    Tin->GetBranch("PMT")->GetLeaf("Evis2")->SetAddress(&e2);
    Tin->GetBranch("PMT")->GetLeaf("Evis3")->SetAddress(&e3);
    Tin->GetBranch("PMT")->GetLeaf("Evis4")->SetAddress(&e4);
    Tin->GetBranch("PMT")->GetLeaf("Evis5")->SetAddress(&e5);
    Tin->GetBranch("PMT")->GetLeaf("Evis6")->SetAddress(&e6);
    Tin->GetBranch("PMT")->GetLeaf("Evis7")->SetAddress(&e7);
    /*
      Tin->GetBranch("xE")->GetLeaf("center")->SetAddress(&EmwpcX);
      Tin->GetBranch("yE")->GetLeaf("center")->SetAddress(&EmwpcY);
      Tin->GetBranch("xW")->GetLeaf("center")->SetAddress(&WmwpcX);
      Tin->GetBranch("yW")->GetLeaf("center")->SetAddress(&WmwpcY);
      Tin->GetBranch("xE")->GetLeaf("nClipped")->SetAddress(&xE_nClipped);
      Tin->GetBranch("yE")->GetLeaf("nClipped")->SetAddress(&yE_nClipped);
      Tin->GetBranch("xW")->GetLeaf("nClipped")->SetAddress(&xW_nClipped);
      Tin->GetBranch("yW")->GetLeaf("nClipped")->SetAddress(&yW_nClipped);
      Tin->GetBranch("xE")->GetLeaf("mult")->SetAddress(&xE_mult);
      Tin->GetBranch("yE")->GetLeaf("mult")->SetAddress(&yE_mult);
      Tin->GetBranch("xW")->GetLeaf("mult")->SetAddress(&xW_mult);
      Tin->GetBranch("yW")->GetLeaf("mult")->SetAddress(&yW_mult);*/
    
    
    double r2E = 0.; //position of event squared
    double r2W = 0.;

    UInt_t nevents = Tin->GetEntriesFast();
    
    for (unsigned int n=0 ; n<nevents ; n++ ) {
      
      Tin->GetEvent(n);
      
      r2E = xE.center*xE.center + yE.center*yE.center;
      r2W = xW.center*xW.center + yW.center*yW.center;

      if ( PID==1 && Side<2 && Type==0 && Erecon>0. ) {

	if ( r2E > 50. || r2W > 50. ) continue;

	
	if ( Side==0 ) {
	  simspec[0]->Fill(e1);
	  simspec[1]->Fill(e2);
	  simspec[2]->Fill(e3);
	  simspec[3]->Fill(e4);
	}

	if ( Side==1 ) {
	  simspec[4]->Fill(e4);
	  simspec[5]->Fill(e5);
	  simspec[6]->Fill(e6);
	  simspec[7]->Fill(e7);
	}

      }
    }

    for ( int p=0; p<8; ++p ) {
      for ( int bin=1; bin<=nBins; ++bin ) {
	simSpec[nRun][p][bin-1] = (double)simspec[p]->GetBinContent(bin);
	simSpecErr[nRun][p][bin-1] = simspec[p]->GetBinError(bin);
      }
    }

    for ( int i=0; i<8; ++i ) { 
      // std::cout << "deleting spec[" << i << "]\n";
      delete simspec[i];
    }
    nRun++;
    delete f;
    
  }

  //Now we take the weighted average over the runs in the octet
  
  TFile *fout = new TFile(TString::Format("PMT_octet_%i.root",octet),"RECREATE");
  
  std::cout << "Made output rootfile...\n\n";

  // Data //

  for ( int i=0; i<8; ++i ) {

    spec[i] = new TH1D(TString::Format("PMT%i",i),TString::Format("PMT %i",i),
		    nBins, minRange, maxRange);

  }
 
  for ( int p=0; p<8; ++p ) {
    for ( int bin=1; bin<=nBins; ++bin ) {

      double numer = 0.;
      double denom = 0.;
      
      for ( UInt_t i=0; i<runs.size(); ++i ) {
	double weight = pmtSpec[i][p][bin-1]>0. ? 1./(pmtSpecErr[i][p][bin-1]*pmtSpecErr[i][p][bin-1]) : 1.;
	numer += pmtSpec[i][p][bin-1]*weight;
	denom += weight;
      }

      spec[p]->SetBinContent(bin,numer/denom);
      spec[p]->SetBinError(bin,TMath::Sqrt(1./denom));
    }
  }

  // Sim //
  for ( int i=0; i<8; ++i ) {

    simspec[i] = new TH1D(TString::Format("SIM%i",i),TString::Format("SIM %i",i),
		    nBins, minRange, maxRange);

  }
 
  for ( int p=0; p<8; ++p ) {
    for ( int bin=1; bin<=nBins; ++bin ) {

      double numer = 0.;
      double denom = 0.;
      
      for ( UInt_t i=0; i<runs.size(); ++i ) {
	double weight = simSpec[i][p][bin-1]>0. ? 1./(simSpecErr[i][p][bin-1]*simSpecErr[i][p][bin-1]) : 1.;
	numer += simSpec[i][p][bin-1]*weight;
	denom += weight;
      }

      simspec[p]->SetBinContent(bin,numer/denom);
      simspec[p]->SetBinError(bin,TMath::Sqrt(1./denom));
    }
  }
  
  
  /////////////////////////// Kurie Fitting ////////////////////

  KurieFitter kf, simkf;

  TGraphErrors kurie[8];
  TGraphErrors simkurie[8];

  for ( int i=0; i<8; ++i ) {

    kf.FitSpectrum(spec[i],120., 580., 1.); //150., 635.
    std::cout << "Data Endpoint: " << kf.returnK0() << "\n";
  
    kurie[i] = kf.returnKuriePlot();
    kurie[i].SetName(TString::Format("data%i",i));
    kurie[i].Write();

    simkf.FitSpectrum(simspec[i],120., 580., 1.); //150., 635.
    std::cout << "Sim Endpoint: " << simkf.returnK0() << "\n";
  
    simkurie[i] = simkf.returnKuriePlot();
    simkurie[i].SetName(TString::Format("sim%i",i));

    simkurie[i].Write();
  }
  

  fout->Write();
  delete fout;

  return 0;
  
}

  
