// -*- C++ -*-
//
// Package:    DiffractiveForwardAnalysis/SinglePhotonTreeProducer
// Class:      SinglePhotonTreeProducer
//
/**\class SinglePhotonTreeProducer SinglePhotonTreeProducer.cc DiffractiveForwardAnalysis/SinglePhotonTreeProducer/plugins/SinglePhotonTreeProducer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Laurent Forthomme
//         Created:  Tue, 24 Jul 2018 12:24:16 GMT
//
//

#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "FWCore/Common/interface/TriggerNames.h"

#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"
#include "HLTrigger/HLTcore/interface/HLTPrescaleProvider.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "DataFormats/PatCandidates/interface/Photon.h"
#include "DataFormats/CTPPSReco/interface/CTPPSLocalTrackLite.h"
#include "DataFormats/CTPPSDetId/interface/CTPPSDetId.h"

#include "DiffractiveForwardAnalysis/GammaGammaLeptonLepton/interface/SinglePhotonEvent.h"

class SinglePhotonTreeProducer : public edm::one::EDAnalyzer<edm::one::SharedResources>
{
  public:
    explicit SinglePhotonTreeProducer( const edm::ParameterSet& );
    ~SinglePhotonTreeProducer();

    static void fillDescriptions( edm::ConfigurationDescriptions& descriptions );

  private:
    virtual void beginRun( const edm::Run&, const edm::EventSetup& );

    virtual void beginJob() override;
    virtual void analyze( const edm::Event&, const edm::EventSetup& ) override;
    virtual void endJob() override;

    TTree* tree_;
    gggx::SinglePhotonEvent evt_;

    std::string hltMenuLabel_;
    std::vector<std::string> triggersList_;
    bool runOnMC_;

    edm::EDGetTokenT<edm::TriggerResults> triggerResultsToken_;
    edm::EDGetTokenT<edm::View<pat::Photon> > photonToken_;
    edm::EDGetTokenT<edm::ValueMap<bool> > phoMediumIdMapToken_, phoTightIdMapToken_;
    edm::EDGetTokenT<edm::View<CTPPSLocalTrackLite> > ppsTracksToken_;

    HLTConfigProvider hltConfig_;
    HLTPrescaleProvider hltPrescale_;
};

SinglePhotonTreeProducer::SinglePhotonTreeProducer( const edm::ParameterSet& iConfig ) :
  hltMenuLabel_  ( iConfig.getParameter<std::string>( "hltMenuTag" ) ),
  triggersList_  ( iConfig.getParameter<std::vector<std::string> >( "triggersList" ) ),
  runOnMC_       ( iConfig.getParameter<bool>( "runOnMC" ) ),
  triggerResultsToken_( consumes<edm::TriggerResults>            ( iConfig.getParameter<edm::InputTag>( "triggerResults" ) ) ),
  photonToken_        ( consumes<edm::View<pat::Photon> >        ( iConfig.getParameter<edm::InputTag>( "photonsTag" ) ) ),
  phoMediumIdMapToken_( consumes<edm::ValueMap<bool> >           ( iConfig.getParameter<edm::InputTag>( "phoMediumIdMap" ) ) ),
  phoTightIdMapToken_ ( consumes<edm::ValueMap<bool> >           ( iConfig.getParameter<edm::InputTag>( "phoTightIdMap" ) ) ),
  ppsTracksToken_     ( consumes<edm::View<CTPPSLocalTrackLite> >( iConfig.getParameter<edm::InputTag>( "ppsTracksTag" ) ) ),
  hltPrescale_        ( iConfig, consumesCollector(), *this )
{
  usesResource( "TFileService" );
  edm::Service<TFileService> fs;
  tree_ = fs->make<TTree>( "ntp1", "ntp1" );
}


SinglePhotonTreeProducer::~SinglePhotonTreeProducer()
{}

void
SinglePhotonTreeProducer::analyze( const edm::Event& iEvent, const edm::EventSetup& iSetup )
{
  evt_.clear();

  //--- trigger information
  edm::Handle<edm::TriggerResults> hltResults;
  iEvent.getByToken( triggerResultsToken_, hltResults );
  const edm::TriggerNames& trigNames = iEvent.triggerNames( *hltResults );

  evt_.HLT_Name.reserve( trigNames.size() );
  for ( const auto& sel : triggersList_ ) {
    short trig_id = -1;
    for ( unsigned int i = 0; i < trigNames.size(); ++i )
      if ( trigNames.triggerNames().at( i ).find( sel ) != std::string::npos ) {
        trig_id = i;
        break;
      }
    if ( trig_id < 0 )
      continue;
    evt_.HLT_Name[evt_.nHLT] = trigNames.triggerNames().at( trig_id );
    evt_.HLT_Accept[evt_.nHLT] = hltResults->accept( trig_id );
    evt_.HLT_Prescl[evt_.nHLT] = 0.;
    // extract prescale value for this path
    if ( !runOnMC_ ) {
      int prescale_set = hltPrescale_.prescaleSet( iEvent, iSetup );
      evt_.HLT_Prescl[evt_.nHLT] = ( prescale_set < 0 )
        ? 0.
        : hltConfig_.prescaleValue(prescale_set, trigNames.triggerNames().at( trig_id ) );
    }
    evt_.nHLT++;
  }

  //--- photons collection retrieval
  edm::Handle<edm::View<pat::Photon> > photonColl;
  iEvent.getByToken( photonToken_, photonColl );

  // photon identification
  edm::Handle<edm::ValueMap<bool> > medium_id_decisions, tight_id_decisions;
  iEvent.getByToken( phoMediumIdMapToken_, medium_id_decisions );
  iEvent.getByToken( phoTightIdMapToken_, tight_id_decisions );

  for ( unsigned int i = 0; i < photonColl->size(); ++i ) {
    const edm::Ptr<pat::Photon> photon = photonColl->ptrAt( i );

    evt_.PhotonCand_pt[evt_.nPhotonCand] = photon->pt();
    evt_.PhotonCand_eta[evt_.nPhotonCand] = photon->eta();
    evt_.PhotonCand_phi[evt_.nPhotonCand] = photon->phi();
    evt_.PhotonCand_e[evt_.nPhotonCand] = photon->energy();
    evt_.PhotonCand_r9[evt_.nPhotonCand] = photon->r9();

    evt_.PhotonCand_mediumID[evt_.nPhotonCand] = medium_id_decisions->operator[]( photon );
    evt_.PhotonCand_tightID[evt_.nPhotonCand] = tight_id_decisions->operator[]( photon );

    evt_.PhotonCand_drtrue[evt_.nPhotonCand] = -999.;
    evt_.PhotonCand_detatrue[evt_.nPhotonCand] = -999.;
    evt_.PhotonCand_dphitrue[evt_.nPhotonCand] = -999.;

    if ( runOnMC_ ) {
      double photdr = 999., photdeta = 999., photdphi = 999.;
      double endphotdr = 999., endphotdeta = 999., endphotdphi = 999.;
      for ( unsigned int j = 0; j < evt_.nGenPhotCand; ++j ) { // matching with the 'true' photon object from MC
        photdeta = ( evt_.PhotonCand_eta[evt_.nPhotonCand]-evt_.GenPhotCand_eta[j] );
        photdphi = ( evt_.PhotonCand_phi[evt_.nPhotonCand]-evt_.GenPhotCand_phi[j] );
        photdr = sqrt( photdeta*photdeta + photdphi*photdphi );
        if ( photdr < endphotdr ) {
          endphotdr = photdr;
          endphotdeta = photdeta;
          endphotdphi = photdphi;
        }
      }
      evt_.PhotonCand_detatrue[evt_.nPhotonCand] = endphotdeta;
      evt_.PhotonCand_dphitrue[evt_.nPhotonCand] = endphotdphi;
      evt_.PhotonCand_drtrue[evt_.nPhotonCand] = endphotdr;
    }

    evt_.nPhotonCand++;
  }

  if ( evt_.nPhotonCand < 1 )
    return;

  //--- PPS local tracks
  edm::Handle<edm::View<CTPPSLocalTrackLite> > ppsTracks;
  iEvent.getByToken( ppsTracksToken_, ppsTracks );
  for ( const auto& trk : *ppsTracks ) {
    if ( evt_.nFwdTrkCand >= gggx::SinglePhotonEvent::MAX_FWDTRKCAND-1 ) {
      edm::LogWarning( "SinglePhotonTreeProducer" )
        << "maximum number of local tracks in RPs is reached! increase MAX_FWDTRKCAND="
        << gggx::SinglePhotonEvent::MAX_FWDTRKCAND;
      break;
    }
    const CTPPSDetId detid( trk.getRPId() );
    evt_.FwdTrkCand_arm[evt_.nFwdTrkCand] = detid.arm();
    evt_.FwdTrkCand_station[evt_.nFwdTrkCand] = detid.station();
    evt_.FwdTrkCand_pot[evt_.nFwdTrkCand] = detid.rp();
    evt_.FwdTrkCand_x[evt_.nFwdTrkCand] = trk.getX()/1.e3;
    evt_.FwdTrkCand_y[evt_.nFwdTrkCand] = trk.getY()/1.e3;
    evt_.FwdTrkCand_xSigma[evt_.nFwdTrkCand] = trk.getXUnc()/1.e3;
    evt_.FwdTrkCand_ySigma[evt_.nFwdTrkCand] = trk.getYUnc()/1.e3;
    evt_.nFwdTrkCand++;
    LogDebug( "SinglePhotonTreeProducer" )
      << "Proton track candidate with origin: ( " << trk.getX() << ", " << trk.getY() << " ) extracted!";
  }

  tree_->Fill();
}

void
SinglePhotonTreeProducer::beginRun( const edm::Run& iRun, const edm::EventSetup& iSetup )
{
  //--- HLT part
  bool changed = true;
  if ( !hltPrescale_.init( iRun, iSetup, hltMenuLabel_, changed ) )
    throw cms::Exception( "GammaGammaLL" ) << " prescales extraction failure with process name " << hltMenuLabel_;
  // Initialise HLTConfigProvider
  hltConfig_ = hltPrescale_.hltConfigProvider();
  if ( !hltConfig_.init( iRun, iSetup, hltMenuLabel_, changed ) )
    throw cms::Exception( "GammaGammaLL" ) << " config extraction failure with process name " << hltMenuLabel_;
  else if ( hltConfig_.size() == 0 )
    edm::LogError( "GammaGammaLL" ) << "HLT config size error";
}

void
SinglePhotonTreeProducer::beginJob()
{
  evt_.attach( tree_, runOnMC_ );
}

void
SinglePhotonTreeProducer::endJob()
{}

void
SinglePhotonTreeProducer::fillDescriptions( edm::ConfigurationDescriptions& descriptions )
{
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault( desc );
}

//define this as a plug-in
DEFINE_FWK_MODULE( SinglePhotonTreeProducer );
