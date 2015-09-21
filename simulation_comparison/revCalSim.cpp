/* Code to take a run number, retrieve it's runperiod, and contruct the 
weighted spectra which would be seen as a reconstructed energy on one side
of the detector. Also applies the trigger functions */

#include <vector>
#include <cstdlib>
#include <iostream>
#include <fstream>

#include <TRandom3.h>
#include <TTree.h>
#include <TFile.h>
#include <TH1F.h>
#include <TF1.h>
#include <TH1D.h>
#include <TChain.h>
#include <TMath.h>

using namespace std;

vector < vector < Double_t > > getTriggerFunctionParams(Int_t XeRunPeriod, Int_t nParams) {
  Char_t file[500];
  sprintf(file,"%s/trigger_functions_XePeriod_%i.dat",getenv("TRIGGER_FUNC"),XeRunPeriod);
  ifstream infile(file);
  vector < vector <Double_t> > func;
  func.resize(2,vector <Double_t> (nParams,0.));
  //cout << "made it here\n";
  for (Int_t side = 0; side<2; side++) {
    Int_t param = 0;
    while (param<nParams) {
      infile >> func[side][param];
      cout << func[side][param] << " ";
      param++;
    }
    cout << endl;
  }
  infile.close();
  return func;
}

Double_t triggerProbability(vector <Double_t> params, Double_t En) {
  Double_t prob = params[0]+params[1]*TMath::Erf((En-params[2])/params[3])
    + params[4]*TMath::Gaus(En,params[5],params[6]);
  return prob;
}

UInt_t getSrcRunPeriod(Int_t runNumber) {
  UInt_t calibrationPeriod=0;
  if (runNumber <= 17297) {
    calibrationPeriod=1;
  }
  else if (runNumber <= 17439) {
    calibrationPeriod=2;
  }
  else if (runNumber <= 17734) {
    calibrationPeriod=3;
  }
  else if (runNumber <= 17955) {
    calibrationPeriod=4;
  }
  else if (runNumber <= 18386) {
    calibrationPeriod=5;
  }
  else if (runNumber <= 18683) {
    calibrationPeriod=6;
  }
  else if (runNumber <= 18994) {
    calibrationPeriod=7;
  }
  else if (runNumber <= 19239) {
    calibrationPeriod=8;
  }
  else if (runNumber <= 19544) {
    calibrationPeriod=9;
  }
  else if (runNumber <= 20000) {
    calibrationPeriod=11;
  }
  return calibrationPeriod;
}

UInt_t getXeRunPeriod(Int_t runNumber) {
  UInt_t calibrationPeriod=0;
  if (runNumber <= 18080) {
    calibrationPeriod=2;
  }
  else if (runNumber <= 18389) {
    calibrationPeriod=3;
  }
  else if (runNumber <= 18711) {
    calibrationPeriod=4;
  }
  else if (runNumber <= 19238) {
    calibrationPeriod=5;
  }
  //else if (runNumber <= 19872) {
  //calibrationPeriod=6;
  //}
  else if (runNumber <= 20000) {
    calibrationPeriod=7;
  }
  else {
    cout << "Bad run number\n";
    exit(0);}

  return calibrationPeriod;
}

vector <Int_t> getPMTQuality(Int_t runNumber) {
  //Read in PMT quality file
  cout << "Reading in PMT Quality file ...\n";
  vector <Int_t>  pmtQuality (8,0);
  Char_t temp[200];
  sprintf(temp,"../residuals/PMT_runQuality_master.dat"); 
  ifstream pmt;
  pmt.open(temp);
  Int_t run_hold;
  while (pmt >> run_hold >> pmtQuality[0] >> pmtQuality[1] >> pmtQuality[2]
	 >> pmtQuality[3] >> pmtQuality[4] >> pmtQuality[5]
	 >> pmtQuality[6] >> pmtQuality[7]) {
    if (run_hold==runNumber) break;
    if (pmt.fail()) break;
  }
  pmt.close();
  if (run_hold!=runNumber) {
    cout << "Run not found in PMT quality file!" << endl;
    exit(0);
  }
  return pmtQuality;
}

vector < Double_t > GetAlphaValues(Int_t runPeriod)
{
  Char_t temp[500];
  vector < Double_t > alphas (8,0.);
  sprintf(temp,"../nPE_Kev_%i.dat",runPeriod);
  ifstream infile;
  infile.open(temp);
  Int_t i = 0;

  while (infile >> alphas[i]) i++;
  return alphas;
}

void revCalSimulation (Int_t runNumber, string source) 
{
  Char_t outputfile[500];
  //sprintf(outputfile,"%s/revCalSim_%i.root",getenv("REVCALSIM"),runNumber);
  sprintf(outputfile,"revCalSim_%i.root",runNumber);
  TFile *outfile = new TFile(outputfile, "RECREATE");
  vector <Int_t> pmtQuality = getPMTQuality(runNumber); // Get the quality of the PMTs for that run
  UInt_t calibrationPeriod = getSrcRunPeriod(runNumber); // retrieve the calibration period for this run
  UInt_t XePeriod = getXeRunPeriod(runNumber);
  vector <Double_t> alphas = GetAlphaValues(calibrationPeriod); // fill vector with the alpha (nPE/keV) values for this run period
  vector < vector <Double_t> > triggerFunc = getTriggerFunctionParams(XePeriod,7); // 2D vector with trigger function for East side and West side in that order

  //PMT histograms with smeared and weighted energies
  vector <TH1D*> pmt (8,0);
  //Final histograms which are the weighted average of the previous histrograms data
  vector <TH1D*> finalEn (6,0);
  finalEn[0] = new TH1D("finalE0", "Simulated Weighted Sum East Type 0", 400, 0., 1200.);
  finalEn[1] = new TH1D("finalW0", "Simulated Weighted Sum West Type 0", 400, 0., 1200.);
  finalEn[2] = new TH1D("finalE1", "Simulated Weighted Sum East Type 1", 400, 0., 1200.);
  finalEn[3] = new TH1D("finalW1", "Simulated Weighted Sum West Type 1", 400, 0., 1200.);
  finalEn[4] = new TH1D("finalE23", "Simulated Weighted Sum East Type 2/3", 400, 0., 1200.);
  finalEn[5] = new TH1D("finalW23", "Simulated Weighted Sum West Type 2/3", 400, 0., 1200.);
  Char_t name[200];
  Char_t file[500];

  Char_t temp[500];

  //Read in simulated data
  TChain *chain = new TChain("anaTree");
  for (int i=0; i<250; i++) {
    sprintf(temp,"/extern/mabrow05/ucna/geant4work/output/10mil_2011-2012/%s/analyzed_%i.root",source.c_str(),i);
    //sprintf(temp,"../../../data/analyzed_%i.root",i);
    chain->AddFile(temp);
  }
  Int_t PID;
  Double_t EdepQ[2], MWPCEnergy[2], time[2];
  //Double_t EdepQE, EdepQW, MWPCEnergyE, MWPCEnergyW;
  Double_t E_sm[8]={0.}; //Hold the smeared energy
  Double_t weight[8]={0.}; // Hold the weight calculated based upon alpha
  chain->SetBranchAddress("EdepQ",EdepQ);
  chain->SetBranchAddress("MWPCEnergy",MWPCEnergy);
  chain->SetBranchAddress("time",time);
  //chain->GetBranch("EdepQ")->GetLeaf("EdepQE")->SetAddress(&EdepQE);
  //chain->GetBranch("EdepQ")->GetLeaf("EdepQW")->SetAddress(&EdepQW);
  //chain->GetBranch("MWPCEnergy")->GetLeaf("MWPCEnergyE")->SetAddress(&MWPCEnergyE);
  //chain->GetBranch("MWPCEnergy")->GetLeaf("MWPCEnergyW")->SetAddress(&MWPCEnergyW);

  //Trigger booleans
  bool EastScintTrigger, WestScintTrigger, EMWPCTrigger, WMWPCTrigger;
  Double_t MWPCThreshold=0.;

  TRandom3 *rand = new TRandom3(0);
  Int_t nevents = chain->GetEntries();
  cout << "events = " << nevents << endl;
 
  
  //Create PMT histograms
  for (UInt_t p=0; p<8; p++) {
    sprintf(name,"PMT %i",p);
    pmt[p] = new TH1D(name,name, 400, 0., 1200.);
  }
  
  //Read in events and determine evt type based on triggers
  for (Int_t evt=0; evt<nevents; evt++) {
    EastScintTrigger = WestScintTrigger = EMWPCTrigger = WMWPCTrigger = false;
    chain->GetEvent(evt);
   
    //MWPC triggers
    if (MWPCEnergy[0]>MWPCThreshold) EMWPCTrigger=true;
    if (MWPCEnergy[1]>MWPCThreshold) WMWPCTrigger=true;
   
    for (UInt_t p=0; p<4; p++) {
      E_sm[p] = rand->Gaus(EdepQ[0],sqrt(EdepQ[0]/alphas[p]));
      if (pmtQuality[p]) {
	weight[p] = alphas[p]/E_sm[p];
	//cout << p << " " << E_sm << " " << weight << endl;
	//pmt[p]->Fill(E_sm[p]);
      }
      else {weight[p]=0.;}
    }
    Double_t numer=0., denom=0.;
    for (UInt_t p=0;p<4;p++) {
      numer+=E_sm[p]*weight[p];
      denom+=weight[p];
    }
    //Now we apply the trigger probability
    Double_t totalEnE = numer/denom;
    Double_t triggProb = triggerProbability(triggerFunc[0],totalEnE);
    //Fill histograms if event passes trigger function
    if (rand->Rndm(0)<triggProb) {
      EastScintTrigger=true;
      //finalEn[0]->Fill(totalEn);
      for (UInt_t p=0;p<4;p++) {
	if (pmtQuality[p]) pmt[p]->Fill(E_sm[p]);
      }	
    }
    
    //West Side
    for (UInt_t p=4; p<8; p++) {
      E_sm[p] = rand->Gaus(EdepQ[1],sqrt(EdepQ[1]/alphas[p]));
      if (pmtQuality[p]) {
	weight[p] = alphas[p]/E_sm[p];
	//cout << p << " " << E_sm << " " << weight << endl;
	//pmt[p]->Fill(E_sm[p]);
      }
      else {weight[p]=0.;}
    }
    //Calculate the total weighted energy
    numer=denom=0.;
    for (UInt_t p=4;p<8;p++) {
      numer+=E_sm[p]*weight[p];
      denom+=weight[p];
    }
    //Now we apply the trigger probability
    Double_t totalEnW = numer/denom;
    triggProb = triggerProbability(triggerFunc[1],totalEnW);
    //Fill histograms if event passes trigger function
    if (rand->Rndm(0)<triggProb) {
      WestScintTrigger = true;
      //finalEn[1]->Fill(totalEn);
      for (UInt_t p=4;p<8;p++) {
	if (pmtQuality[p]) pmt[p]->Fill(E_sm[p]);
      }
    }
    
    //Fill proper total event histogram based on event type
    if (EastScintTrigger && EMWPCTrigger && !WestScintTrigger && !WMWPCTrigger) {
      finalEn[0]->Fill(totalEnE);
    }
    if (WestScintTrigger && WMWPCTrigger && !EastScintTrigger && !EMWPCTrigger) {
      finalEn[1]->Fill(totalEnW);
    }
    if (EastScintTrigger && EMWPCTrigger && WestScintTrigger && WMWPCTrigger) {
      if (time[0]<time[1]) finalEn[2]->Fill(totalEnE);
      if (time[0]>time[1]) finalEn[3]->Fill(totalEnW);
    }
    if (EastScintTrigger && EMWPCTrigger && !WestScintTrigger && WMWPCTrigger) {
      finalEn[4]->Fill(totalEnE);
    }
    if (!EastScintTrigger && EMWPCTrigger && WestScintTrigger && WMWPCTrigger) {
      finalEn[5]->Fill(totalEnW);
    }   
    
    cout << "filled event " << evt << endl;
  }
  delete chain;
  outfile->Write();
  outfile->Close();
}

int main(int argc, char *argv[]) {
  string source = string(argv[2]);
  revCalSimulation(atoi(argv[1]),source);
}
  //vector <TF1*> func (2,0);
  
  //TCanvas *c1 = new TCanvas();
  //finalE->Draw();

  //TCanvas *c2 = new TCanvas();
  //finalW->Draw();

  //Fit the 8 individual PMTs for comparison when doing linearity curves
  /*sprintf(outputfile,"fits/%i_%s_weightedSimPeaks_PMTbyPMT.dat",runNumber, source.c_str());
  ofstream peakDat(outputfile);
  for (UInt_t n=0;n<8;n++) {
    if (pmtQuality[n]) {
      Int_t maxBin = pmt[n]->GetMaximumBin();
      Double_t peak = pmt[n]->GetXaxis()->GetBinCenter(maxBin);
      Double_t maxBinContent = pmt[n]->GetBinContent(maxBin);
      Double_t high = 0., low=0.;
      for (int i=maxBin; i<400; i++) {
	if (pmt[n]->GetBinContent(i+1) < 0.5*maxBinContent || pmt[n]->GetXaxis()->GetBinCenter(i+1)>1170.) {
	  high= pmt[n]->GetXaxis()->GetBinCenter(i+1);
	  break;
	}
      }
      for (int i=maxBin; i<400; i--) {
	if (pmt[n]->GetBinContent(i-1) < 0.5*maxBinContent) {
	  low= pmt[n]->GetXaxis()->GetBinCenter(i-1);
	  break;
	}
      }
      
      TF1 *func = new TF1("func", "gaus", low, high);
      //h->SetParameter(1,peak);
      pmt[n]->Fit("func", "LRQ");
      cout << func->GetParameter(1) << endl;
      peakDat << func->GetParameter(1) << " ";
      delete func;
    }
    else {
      peakDat << 0. << " ";
      cout << "BAD PMT\n";
    }   
    
    if (source=="Bi207") {
      if (pmtQuality[n]) {  
	pmt[n]->GetXaxis()->SetRangeUser(200., 600.);
	Int_t maxBin = pmt[n]->GetMaximumBin();
	Double_t peak = pmt[n]->GetXaxis()->GetBinCenter(maxBin);
	Double_t maxBinContent = pmt[n]->GetBinContent(maxBin);
	Double_t high = 0., low=0.;
	
	for (int i=maxBin; i<400; i++) {
	  if (pmt[n]->GetBinContent(i+1) < 0.5*maxBinContent || pmt[n]->GetXaxis()->GetBinCenter(i+1)>670.) {
	    high= pmt[n]->GetXaxis()->GetBinCenter(i+1);
	    if (high>550.) {high = 550.; peak = 450.;} // In case the 2 peaks are too close together due to low nPE
	    break;
	  } 
	}
	for (int i=maxBin; i<400; i--) {
	  if (pmt[n]->GetBinContent(i-1) < 0.5*maxBinContent || pmt[n]->GetXaxis()->GetBinCenter(i)<230.) {
	    low= pmt[n]->GetXaxis()->GetBinCenter(i-1);
	    break;
	  }
	}
	pmt[n]->GetXaxis()->SetRange(0,400);
	TF1 *func = new TF1("func", "gaus", low, high);
	func->SetParameter(1,peak);
	pmt[n]->Fit("func", "LRQ+");
	cout << func->GetParameter(1) << endl;
	peakDat << func->GetParameter(1) << " ";
	delete func;
      }    
      else {
	peakDat << 0. << " ";
	cout << "BAD PMT\n";
      }
    }
    cout << "Finished PMT " << n << endl;
    if (n<7) peakDat << endl;
  }

  peakDat.close();


  sprintf(outputfile,"fits/%i_%s_weightedSimPeaks.dat",runNumber, source.c_str());
  peakDat.open(outputfile);
  
  //Fit the final weighted histograms
  for (UInt_t n=0;n<2;n++) {

    Int_t maxBin = finalEn[n]->GetMaximumBin();
    Double_t peak = finalEn[n]->GetXaxis()->GetBinCenter(maxBin);
    Double_t maxBinContent = finalEn[n]->GetBinContent(maxBin);
    Double_t high = 0., low=0.;
    for (int i=maxBin; i<400; i++) {
      if (finalEn[n]->GetBinContent(i+1) < 0.5*maxBinContent) {
	high= finalEn[n]->GetXaxis()->GetBinCenter(i+1);
	break;
      }
    }
    for (int i=maxBin; i<400; i--) {
      if (finalEn[n]->GetBinContent(i-1) < 0.5*maxBinContent) {
	low= finalEn[n]->GetXaxis()->GetBinCenter(i-1);
	break;
      }
    }
    
    TF1 *func = new TF1("func", "gaus", low, high);
    //h->SetParameter(1,peak);
    finalEn[n]->Fit("func", "LRQ");
    cout << func->GetParameter(1) << endl;
    peakDat << func->GetParameter(1) << " ";
    delete func;
  }
  
  if (source=="Bi207") {
    peakDat << endl;
    for (UInt_t n=0;n<2;n++) {
      finalEn[n]->GetXaxis()->SetRangeUser(200., 700.);
      Int_t maxBin = finalEn[n]->GetMaximumBin();
      Double_t peak = finalEn[n]->GetXaxis()->GetBinCenter(maxBin);
      Double_t maxBinContent = finalEn[n]->GetBinContent(maxBin);
      Double_t high = 0., low=0.;
      
      for (int i=maxBin; i<400; i++) {
	if (finalEn[n]->GetBinContent(i+1) < 0.5*maxBinContent) {
	  high= finalEn[n]->GetXaxis()->GetBinCenter(i+1);
	  break;
	}
      }
      for (int i=maxBin; i<400; i--) {
	if (finalEn[n]->GetBinContent(i-1) < 0.5*maxBinContent) {
	  low= finalEn[n]->GetXaxis()->GetBinCenter(i-1);
	  break;
	}
      }
      finalEn[n]->GetXaxis()->SetRange(0,400);
      TF1 *func = new TF1("func", "gaus", low, high);
      //h->SetParameter(1,peak);
      finalEn[n]->Fit("func", "LRQ+");
      cout << func->GetParameter(1) << endl;
      peakDat << func->GetParameter(1) << " ";
      delete func;
    }
    }*/
  
