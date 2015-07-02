#include "buildtest.h"
#include "KalmanUtils.h"
#include "Propagation.h"
#include "BinInfoUtils.h"
#include "Event.h"
#include "Debug.h"

#include <cmath>
#include <iostream>

//typedef std::pair<Track, TrackState> cand_t;
typedef Track cand_t;
typedef TrackVec::const_iterator TrkIter;

#ifndef TBB
typedef std::vector<cand_t> candvec;
#else
#include "tbb/tbb.h"
#include <mutex>
// concurrent_vector is only needed if we parallelize the candidate loops;
// not needed if we only parallelize over seeds.
//typedef tbb::concurrent_vector<cand_t> candvec;
typedef std::vector<cand_t> candvec;
static std::mutex evtlock;
#endif
typedef candvec::const_iterator canditer;

void extendCandidate(const Event& ev, const cand_t& cand, candvec& tmp_candidates, unsigned int ilay, unsigned int validationArray[], bool debug);

static bool sortByHitsChi2(const cand_t& cand1, const cand_t& cand2)
{
  if (cand1.nHits()==cand2.nHits()) return cand1.chi2()<cand2.chi2();
  return cand1.nHits()>cand2.nHits();
}

void processCandidates(Event& ev, candvec& candidates, unsigned int ilay, const bool debug)
{
  auto& evt_track_candidates(ev.candidateTracks_);

  candvec tmp_candidates;
  tmp_candidates.reserve(3*candidates.size()/2);

  if (candidates.size() > 0) {
    std::vector<unsigned int> candBranches; // validation info --> store the n of branches per candidate, i.e. how many hits passed chi2 cut (subset of candHits)  
    unsigned int tmp_candidates_size = 0; // need dummy counter to keep track of branches      

    // these will be filled by validationArray pass to extendCandidate
    unsigned int validationArray[2] = {0,0}; // pass this to extend candidate to collect info
    std::vector<unsigned int> candEtaPhiBins; // validation info --> store n Eta/Phi Sectors explored by extending candidate per candidate (validationArray[0])
    std::vector<unsigned int> candHits; // validation info --> store n Hits explored per candidate. (validationArray[1])
    
    //loop over running candidates
    for (auto&& cand : candidates) {
      tmp_candidates_size = tmp_candidates.size(); // validation

      extendCandidate(ev, cand, tmp_candidates, ilay, validationArray, debug);

      candEtaPhiBins.push_back(validationArray[0]); // validation
      candHits.push_back(validationArray[1]); // validation
      candBranches.push_back(tmp_candidates.size() - tmp_candidates_size); // validation
    }

    ev.validation_.fillBuildTree(ilay, candidates.size(), candEtaPhiBins, candHits, candBranches);

    if (tmp_candidates.size()>Config::maxCand) {
      dprint("huge size=" << tmp_candidates.size() << " keeping best "<< Config::maxCand << " only");
      std::partial_sort(tmp_candidates.begin(),tmp_candidates.begin()+(Config::maxCand-1),tmp_candidates.end(),sortByHitsChi2);
      tmp_candidates.resize(Config::maxCand); // thread local, so ok not thread safe
    } else if (tmp_candidates.size()==0) {
      dprint("no more candidates, saving best");
      // save the best candidate from the previous iteration and then swap in
      // the empty new candidate list; seed will be skipped on future iterations
      auto&& best = std::min_element(candidates.begin(),candidates.end(),sortByHitsChi2);
#ifdef TBB
      std::lock_guard<std::mutex> evtguard(evtlock); // should be rare
#endif
      evt_track_candidates.push_back(*best);
    }
    dprint("swapping with size=" << tmp_candidates.size());
    candidates.swap(tmp_candidates);
    tmp_candidates.clear();
  }
}

void buildTracksBySeeds(Event& ev)
{
  auto& evt_track_candidates(ev.candidateTracks_);
  const auto& evt_lay_hits(ev.layerHits_);
  const auto& evt_seeds(ev.seedTracks_);
  bool debug(false);

  std::vector<candvec> track_candidates(evt_seeds.size());
  for (auto iseed = 0U; iseed < evt_seeds.size(); iseed++) {
    const auto& seed(evt_seeds[iseed]);
    track_candidates[iseed].push_back(seed);
  }

#ifdef TBB
  //loop over seeds
  parallel_for( tbb::blocked_range<size_t>(0, evt_seeds.size()), 
      [&](const tbb::blocked_range<size_t>& seediter) {
    for (auto iseed = seediter.begin(); iseed != seediter.end(); ++iseed) {
      const auto& seed(evt_seeds[iseed]);
#else
    for (auto iseed = 0U; iseed != evt_seeds.size(); ++iseed) {
      const auto& seed(evt_seeds[iseed]);
#endif
      dprint("processing seed # " << seed.SimTrackIDInfo().first << " par=" << seed.parameters());
      TrackState seed_state = seed.state();
      //seed_state.errors *= 0.01;//otherwise combinatorics explode!!!
      //should consider more than 1 candidate...
      auto&& candidates(track_candidates[iseed]);
      for (unsigned int ilay=Config::nlayers_per_seed;ilay<evt_lay_hits.size();++ilay) {//loop over layers, starting from after the seed
        dprint("going to layer #" << ilay << " with N cands=" << track_candidates.size());
        processCandidates(ev, candidates, ilay, debug);
      }
      //end of layer loop
    }//end of process seeds loop
#ifdef TBB
  });
#endif
  for (const auto& cand : track_candidates) {
    if (cand.size()>0) {
      // only save one track candidate per seed, one with lowest chi2
      //std::partial_sort(cand.begin(),cand.begin()+1,cand.end(),sortByHitsChi2);
      auto&& best = std::min_element(cand.begin(),cand.end(),sortByHitsChi2);
      evt_track_candidates.push_back(*best);
    }
  }
}		

void buildTracksByLayers(Event& ev)
{
  auto& evt_track_candidates(ev.candidateTracks_);
  const auto& evt_lay_hits(ev.layerHits_);
  const auto& evt_seeds(ev.seedTracks_);
  bool debug(false);

  std::vector<candvec> track_candidates(evt_seeds.size());
  for (auto iseed = 0U; iseed < evt_seeds.size(); iseed++) {
    const auto& seed(evt_seeds[iseed]);
    track_candidates[iseed].push_back(seed);
  }

  //loop over layers, starting from after the seed
  for (auto ilay = Config::nlayers_per_seed; ilay < evt_lay_hits.size(); ++ilay) {
    dprint("going to layer #" << ilay << " with N cands = " << track_candidates.size());

#ifdef TBB
    //loop over seeds
    parallel_for( tbb::blocked_range<size_t>(0, evt_seeds.size()), 
        [&](const tbb::blocked_range<size_t>& seediter) {
      for (auto iseed = seediter.begin(); iseed != seediter.end(); ++iseed) {
        const auto& seed(evt_seeds[iseed]);
        auto&& candidates(track_candidates[iseed]);
        processCandidates(ev, candidates, ilay, debug);
      }
    }); //end of process seeds loop
#else
    //process seeds
    for (auto iseed = 0U; iseed != evt_seeds.size(); ++iseed) {
      const auto& seed(evt_seeds[iseed]);
      auto&& candidates(track_candidates[iseed]);
      processCandidates(ev, candidates, ilay, debug);
    }
#endif
  } //end of layer loop

  //std::lock_guard<std::mutex> evtguard(evtlock);
  for (const auto& cand : track_candidates) {
    if (cand.size()>0) {
      // only save one track candidate per seed, one with lowest chi2
      //std::partial_sort(cand.begin(),cand.begin()+1,cand.end(),sortByHitsChi2);
      auto&& best = std::min_element(cand.begin(),cand.end(),sortByHitsChi2);
      evt_track_candidates.push_back(*best);
    }
  }
}

  void extendCandidate(const Event& ev, const cand_t& cand, candvec& tmp_candidates, unsigned int ilayer, 
		       unsigned int validationArray[], bool debug)
{
  const Track& tkcand = cand;
  const TrackState& updatedState = cand.state();
  const auto& evt_lay_hits(ev.layerHits_);
  const auto& segLayMap(ev.segmentMap_[ilayer]);
  //  debug = true;

  dprint("processing candidate with nHits=" << tkcand.nHits());
#ifdef LINEARINTERP
  TrackState propState = propagateHelixToR(updatedState,ev.geom_.Radius(ilayer));
#else
#ifdef TBB
#error "Invalid combination of options (thread safety)"
#endif
  TrackState propState = propagateHelixToLayer(updatedState,ilayer,ev.geom_);
#endif // LINEARINTERP
#ifdef CHECKSTATEVALID
  if (!propState.valid) {
    return;
  }
#endif
  const float predx  = propState.parameters.At(0);
  const float predy  = propState.parameters.At(1);
  const float predz  = propState.parameters.At(2);
#ifdef DEBUG
  if (debug) {
    std::cout << "propState at hit#" << ilayer << " r/phi/z : " << sqrt(pow(predx,2)+pow(predy,2)) << " "
              << std::atan2(predy,predx) << " " << predz << std::endl;
    dumpMatrix(propState.errors);
  }
#endif

#ifdef ETASEG  
  const float eta  = getEta(std::sqrt(getRad2(predx,predy)),predz);
  const float deta = std::sqrt(std::abs(getEtaErr2(predx,predy,predz,propState.errors.At(0,0),propState.errors.At(1,1),propState.errors.At(2,2),propState.errors.At(0,1),propState.errors.At(0,2),propState.errors.At(1,2))));
  const float nSigmaDeta = std::min(std::max(Config::nSigma*deta,(float) Config::minDEta), (float) Config::maxDEta); // something to tune -- minDEta = 0.0
  const auto etaBinMinus = getEtaPartition(normalizedEta(eta-nSigmaDeta));
  const auto etaBinPlus  = getEtaPartition(normalizedEta(eta+nSigmaDeta));
#else
  const auto etaBinMinus = 0U;
  const auto etaBinPlus  = 0U;
#endif
  const float phi    = getPhi(predx,predy); //std::atan2(predy,predx); 
  const float dphi = std::sqrt(std::abs(getPhiErr2(predx,predy,propState.errors.At(0,0),propState.errors.At(1,1),propState.errors.At(0,1))));
  const float nSigmaDphi = std::min(std::max(Config::nSigma*dphi,(float) Config::minDPhi), (float) Config::maxDPhi);
  const auto phiBinMinus = getPhiPartition(normalizedPhi(phi-nSigmaDphi));
  const auto phiBinPlus  = getPhiPartition(normalizedPhi(phi+nSigmaDphi));

#ifdef DEBUG
  if (debug) {
    std::cout << "propState at layer: " << ilayer << " r/phi/eta : " << std::sqrt(getRad2(predx,predy)) << " "
              << phi << " " << eta << std::endl;
    dumpMatrix(propState.errors);
  }
#endif

  //nsectors for validation
  if (phiBinPlus >= phiBinMinus){ // count the number of eta/phi bins explored
    validationArray[0] = (etaBinPlus-etaBinMinus+1)*(phiBinPlus-phiBinMinus+1);
  }
  else{
    validationArray[0] = (etaBinPlus-etaBinMinus+1)*(Config::nPhiPart-phiBinMinus+phiBinPlus+1);
  }
    
  std::vector<unsigned int> cand_hit_indices = getCandHitIndices(etaBinMinus,etaBinPlus,phiBinMinus,phiBinPlus,segLayMap);
  validationArray[1] = cand_hit_indices.size(); // validation info --> tried to be as minimally invasive as possible --> if worried about performance, ifdef this out and change the funciton call for extend candidate ... a bit excessive

#ifdef LINEARINTERP
    const float minR = ev.geom_.Radius(ilayer);
    float maxR = minR;
    for (auto&& cand_hit_idx : cand_hit_indices){
      const float candR = evt_lay_hits[ilayer][cand_hit_idx].r();
      if (candR > maxR) maxR = candR;
    }
    const float deltaR = maxR - minR;
    dprint("min, max: " << minR << ", " << maxR);
    const TrackState propStateMin = propState;
    const TrackState propStateMax = propagateHelixToR(updatedState,maxR);
#ifdef CHECKSTATEVALID
    if (!propStateMax.valid) {
      return;
    }
#endif
#endif
  
    for (auto&& cand_hit_idx : cand_hit_indices){
      const Hit hitCand = evt_lay_hits[ilayer][cand_hit_idx];
      const MeasurementState hitMeas = hitCand.measurementState();

#ifdef LINEARINTERP
      const float ratio = (hitCand.r() - minR)/deltaR;
      propState.parameters = (1.0-ratio)*propStateMin.parameters + ratio*propStateMax.parameters;
      dprint(std::endl << ratio << std::endl << propStateMin.parameters << std::endl << propState.parameters << std::endl
                       << propStateMax.parameters << std::endl << propStateMax.parameters - propStateMin.parameters
                       << std::endl << std::endl << hitMeas.parameters);
#endif
      const float chi2 = computeChi2(propState,hitMeas);
    
      if ((chi2<Config::chi2Cut)&&(chi2>0.)) {//fixme 
        dprint("found hit with index: " << cand_hit_idx << " chi2=" << chi2);
        const TrackState tmpUpdatedState = updateParameters(propState, hitMeas);
        Track tmpCand(tmpUpdatedState,tkcand.hitsVector(),tkcand.chi2(),tkcand.seedID()); //= tkcand.clone();
        tmpCand.addHit(hitCand,chi2);
        tmp_candidates.push_back(tmpCand);
      }
    }//end of consider hits on layer loop

  //add also the candidate for no hit found
  if (tkcand.nHits()==ilayer) {//only if this is the first missing hit
    dprint("adding candidate with no hit");
    tmp_candidates.push_back(tkcand);
  }
}
