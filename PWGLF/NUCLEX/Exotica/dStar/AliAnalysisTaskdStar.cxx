#include "AliAnalysisTaskdStar.h"

// pdgcode dstar 900010020

// ROOT includes
#include <TChain.h>
#include <TClonesArray.h>
#include <TH2F.h>
#include <TList.h>
#include <TMath.h>
#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
using namespace ROOT::Math;
typedef ROOT::Math::LorentzVector<ROOT::Math::PtEtaPhiM4D<float>> FourVector_t ;
// ALIROOT includes
#include "AliPDG.h"
#include "AliVTrack.h"
#include "AliVVertex.h"
#include "AliVEvent.h"
#include "AliAODTrack.h"
#include "AliAODMCParticle.h"
#include "AliAODVertex.h"
#include "AliTPCPIDResponse.h"
#include "AliTOFPIDResponse.h"
#include "AliAnalysisManager.h"
#include "AliInputEventHandler.h"

// std includes
#include <climits>

///\cond CLASSIMP
ClassImp(AliAnalysisTaskdStar);
///\endcond

struct mother_struct{
  int id;
  bool pi_tof;
  bool deuteron_tof;
  int n_daughters;
  FourVector_t vec;
  bool operator==(const int &id_comp) const {return id==id_comp;}
};

/// Standard and default constructor of the class.
///
/// \param taskname Name of the task
/// \param partname Name of the analysed particle
///
AliAnalysisTaskdStar::AliAnalysisTaskdStar(const char* taskname) : AliAnalysisTaskSE(taskname),
fEventCut(false),
fList(),
fProduction(),
fReconstructed(),
fTotal(),
fRequireYmin(-0.5f),
fRequireYmax(0.5f),
fPID()
{
  fFilterBit = BIT(8);
  DefineInput(0, TChain::Class());
  DefineOutput(1, TList::Class());
}

/// Standard destructor
///
AliAnalysisTaskdStar::~AliAnalysisTaskdStar() {
  if (fList) delete fList;
  if (fPID) delete fPID;
}

/// This function creates all the histograms and all the objects in general used during the analysis
/// \return void
///
void AliAnalysisTaskdStar::UserCreateOutputObjects() {

  fList = new TList();
  fList->SetOwner(true);

  char   letter[2] = {'a','m'};
  string tpctof[2] = {"TPC","TOF"};
  string tpctofMC[3] = {"TPC","TPC_TOF","TPC_(TOF)"};
  string dStarState = "dStar(2380)";
  float low_mass_limit = 2.2;
  float up_mass_limit = 2.7;


  for (int iC = 0; iC < 2; ++iC) {

    fProduction[iC] = new TH2F(Form("fProduction_%s_%c", dStarState.data(),letter[iC]),";M (GeV/#it{c}^{2});#it{p}_{T} (GeV/#it{c});Counts",50,low_mass_limit,up_mass_limit,20,0,10);
    fList->Add(fProduction[iC]);

    fTotal[iC] = new TH2F(Form("fTotal_%s_%c",dStarState.data(),letter[iC]),";M (GeV/#it{c}^{2});#it{p}_{T} (GeV/#it{c});Counts",50,low_mass_limit,up_mass_limit,20,0,10);
    fList->Add(fTotal[iC]);

    for (int iT = 0; iT < 3; ++iT) {
      fReconstructed[iC][iT] = new TH2F(Form("fRec_%s_%c_ITS_%s",dStarState.data(),letter[iC],tpctofMC[iT].data()),";M (GeV/#it{c}^{2});#it{p}_{T} (GeV/#it{c});Counts",50,low_mass_limit,up_mass_limit,20,0,10);
      fList->Add(fReconstructed[iC][iT]);
    }
  }

  fNDaughters = new TH1F("nDaugthers","number of daughter per dTtar", 5, 0, 5);
  fList->Add(fNDaughters);

  AliPDG::AddParticlesToPdgDataBase();
  fEventCut.AddQAplotsToList(fList);

  PostData(1,fList);
}

/// This is the function that is evaluated for each event. The analysis code stays here.
///
/// \param options Deprecated parameter
/// \return void
///
void AliAnalysisTaskdStar::UserExec(Option_t *){
  /// The first check performed is on the particle type requested. If this type is AliPID::Unknown -
  /// the default one - the task does not process the information.
  AliVEvent *ev = InputEvent();
  if (!fEventCut.AcceptEvent(ev)) {
    PostData(1, fList);
    return;
  }

  /// To perform the majority of the analysis - and also this one - the standard PID handler is
  /// required.
  AliAnalysisManager *mgr = AliAnalysisManager::GetAnalysisManager();
  AliInputEventHandler* handl = (AliInputEventHandler*)mgr->GetInputEventHandler();
  fPID = handl->GetPIDResponse();

  TClonesArray *stack = nullptr;
  // get branch "mcparticles"
  stack = (TClonesArray*)ev->GetList()->FindObject(AliAODMCParticle::StdBranchName());
  if (!stack)
  ::Fatal("AliAnalysisTaskdStar::UserExec","MC analysis requested on a sample without the MC particle array.");

  /// Making the list of the dstar we want to measure
  //std::cout << "LISTING ALL THE MOTHERS: "<<std::endl<<std::endl;

  for (int iMC = 0; iMC < stack->GetEntriesFast(); ++iMC) {

    AliAODMCParticle *part = (AliAODMCParticle*)stack->UncheckedAt(iMC);
    const int pdg = TMath::Abs(part->GetPdgCode());
    const int iC = part->Charge() > 0 ? 1 : 0;
    if (pdg != 900010020) continue;
    FourVector_t moth_vec = {0.f,0.f,0.f,0.f}, tmp_vec = {0.f,0.f,0.f,0.f};

    for(int iD=0; iD<3; iD++){
      const int daughter_id = part->GetDaughter(0)+iD;
      AliAODMCParticle *daughter_part = (AliAODMCParticle*)stack->At(TMath::Abs(daughter_id));
      tmp_vec.SetCoordinates(daughter_part->Pt(),daughter_part->Eta(),daughter_part->Phi(),daughter_part->M());
      moth_vec+=tmp_vec;
    }

    int part_n_daughters = part->GetDaughter(1)-part->GetDaughter(0);
    fNDaughters->Fill(part_n_daughters); // check the number of daughter for dstar
    fProduction[iC]->Fill(part->M(),part->Pt());

    if ( part->Y() < fRequireYmin || part->Y() > fRequireYmax ) continue;
    fTotal[iC]->Fill(moth_vec.M(),moth_vec.Pt());
  }

  /// Checking how many dstar in acceptance are reconstructed well
  std::vector<mother_struct> mothers;
  mothers.reserve(40);
  for (Int_t iT = 0; iT < (Int_t)ev->GetNumberOfTracks(); ++iT) {
    AliAODTrack *track = static_cast<AliAODTrack*>(ev->GetTrack(iT));
    if (track->GetID() <= 0) continue;
    if (!track->TestFilterBit(fFilterBit)) continue;
    AliAODMCParticle *part = (AliAODMCParticle*)stack->At(TMath::Abs(track->GetLabel()));
    if (!part) continue;
    const int pdg = TMath::Abs(part->GetPdgCode());
    if (pdg != 211 && pdg != 1000010020) continue;

    const int mother_id = part->GetMother();
    AliAODMCParticle* mother = (mother_id >= 0) ? (AliAODMCParticle*)stack->At(mother_id) : nullptr;
    if (!mother) continue;
    const int mother_pdg = TMath::Abs(mother->GetPdgCode());

    //Check wheter the track belongs to a deuteron
    if (pdg == 1000010020 && mother_pdg == 900010020) {
      if(TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTPC,track,AliPID::kDeuteron))>3.) continue;
      FourVector_t tmp_vec = {(float)track->Pt(),(float)track->Eta(),(float)track->Phi(),(float)track->M(AliAODTrack::kDeuteron)};
      auto it = std::find(mothers.begin(),mothers.end(), mother_id);
      if (it == mothers.end()){
        mother_struct tmp_mum;
        tmp_mum.id = mother_id;
        tmp_mum.deuteron_tof = AliAnalysisTaskdStar::HasTOF(track) && (TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTOF,track,AliPID::kDeuteron))<3.);
        tmp_mum.n_daughters = 1;
        tmp_mum.vec = tmp_vec;
        mothers.push_back(tmp_mum);
      }
      else{
        it->n_daughters++;
        it->deuteron_tof *= AliAnalysisTaskdStar::HasTOF(track) && (TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTOF,track,AliPID::kDeuteron))<3.);
        it->vec+=tmp_vec;
      }
    }

    //Check wether the track belgons to a pion
    if (pdg == 211 && mother_pdg == 900010020) {
      if(TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTPC,track,AliPID::kPion))>3.) continue;
      FourVector_t tmp_vec = {(float)track->Pt(),(float)track->Eta(),(float)track->Phi(),(float)track->M(AliAODTrack::kPion)};
      auto it = std::find(mothers.begin(),mothers.end(), mother_id);
      if (it == mothers.end()){
        mother_struct tmp_mum;
        tmp_mum.id = mother_id;
        tmp_mum.pi_tof = AliAnalysisTaskdStar::HasTOF(track) && (TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTOF,track,AliPID::kPion))<3.);
        tmp_mum.n_daughters = 1;
        tmp_mum.vec = tmp_vec;
        mothers.push_back(tmp_mum);
      }
      else{
        it->n_daughters++;
        it->pi_tof *= AliAnalysisTaskdStar::HasTOF(track) && (TMath::Abs(fPID->NumberOfSigmas(AliPIDResponse::kTOF,track,AliPID::kPion))<3.);
        it->vec+=tmp_vec;
      }
    }

  } // End AOD track loop

  for (const auto& mum : mothers) {
    if (mum.n_daughters != 3) continue;
    if (mum.vec.Rapidity() < fRequireYmin || mum.vec.Rapidity() > fRequireYmax) continue;
    AliAODMCParticle *part = static_cast<AliAODMCParticle*>(stack->At(mum.id));
    const int iC = part->Charge() > 0 ? 1 : 0;
    const int pdg = TMath::Abs(part->GetPdgCode());
    const float pt_rec = mum.vec.Pt();
    const float mass_rec = mum.vec.M();
    fReconstructed[iC][0]->Fill(mass_rec,pt_rec);
    if(mum.pi_tof && mum.deuteron_tof) fReconstructed[iC][1]->Fill(mass_rec,pt_rec);
    if(mum.deuteron_tof) fReconstructed[iC][2]->Fill(mass_rec,pt_rec);
  }

  //  Post output data.
  PostData(1,fList);
}

/// This function checks whether a track has or has not a prolongation in the TOF.
///
/// \param track Track that has to be checked
/// \return true if the track has a matching hit in the TOF.
///
bool AliAnalysisTaskdStar::HasTOF(AliVTrack *track) {
  bool hasTOFout  = track->GetStatus() & AliVTrack::kTOFout;
  bool hasTOFtime = track->GetStatus() & AliVTrack::kTIME;
  return hasTOFout && hasTOFtime && (track->GetIntegratedLength() > 350.);
}
