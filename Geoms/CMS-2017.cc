//-------------------
// CMS 2017 geometry
//-------------------

#include "Config.h"
#include "Debug.h"
#include "TrackerInfo.h"
#include "mkFit/SteeringParams.h"
#include "mkFit/HitStructures.h"

#include <functional>

using namespace mkfit;

namespace
{
#include "CMS-2017.acc"

  void SetupSteeringParams_Iter0(IterationConfig& ic)
  {
    ic.m_region_order[0] = TrackerInfo::Reg_Transition_Pos;
    ic.m_region_order[1] = TrackerInfo::Reg_Transition_Neg;
    ic.m_region_order[2] = TrackerInfo::Reg_Endcap_Pos;
    ic.m_region_order[3] = TrackerInfo::Reg_Endcap_Neg;
    ic.m_region_order[4] = TrackerInfo::Reg_Barrel;

    {
      SteeringParams &sp = ic.m_steering_params[TrackerInfo::Reg_Endcap_Neg];
      sp.reserve_plan(3 + 3 + 6 + 18);
      sp.fill_plan(0, 1, false, true); // bk-fit only
      sp.append_plan(2, true);         // pick-up only
      sp.append_plan(45, false);
      sp.append_plan(46, false);
      sp.append_plan(47, false);
      sp.fill_plan(48, 53); // TID,  6 layers
      sp.fill_plan(54, 71); // TEC, 18 layers
      sp.finalize_plan();
    }
    {
      SteeringParams &sp = ic.m_steering_params[TrackerInfo::Reg_Transition_Neg];
      sp.reserve_plan(3 + 4 + 6 + 6 + 8 + 18);
      sp.fill_plan(0, 1, false, true); // bk-fit only
      sp.append_plan(2, true);
      sp.append_plan(3, false);
      sp.append_plan(45, false);
      sp.append_plan(46, false);
      sp.append_plan(47, false);
      sp.fill_plan(4, 9);   // TIB,  6 layers
      sp.fill_plan(48, 53); // TID,  6 layers
      sp.fill_plan(10, 17); // TOB,  8 layers
      sp.fill_plan(54, 71); // TEC, 18 layers
      sp.finalize_plan();
    }
    {
      SteeringParams &sp = ic.m_steering_params[TrackerInfo::Reg_Barrel];
      sp.reserve_plan(3 + 1 + 6 + 8);
      sp.fill_plan(0, 1, false, true); // bk-fit only
      sp.append_plan(2, true);         // pickup-only
      sp.append_plan(3, false);
      sp.fill_plan(4, 9);   // TIB, 6 layers
      sp.fill_plan(10, 17); // TOB, 8 layers
      sp.finalize_plan();
    }
    {
      SteeringParams &sp = ic.m_steering_params[TrackerInfo::Reg_Transition_Pos];
      sp.reserve_plan(3 + 4 + 6 + 6 + 8 + 18);
      sp.fill_plan(0, 1, false, true); // bk-fit only
      sp.append_plan(2, true);         // pickup-only
      sp.append_plan(3, false);
      sp.append_plan(18, false);
      sp.append_plan(19, false);
      sp.append_plan(20, false);
      sp.fill_plan(4, 9);   // TIB,  6 layers
      sp.fill_plan(21, 26); // TID,  6 layers
      sp.fill_plan(10, 17); // TOB,  8 layers
      sp.fill_plan(27, 44); // TEC, 18 layers
      sp.finalize_plan();
    }
    {
      SteeringParams &sp = ic.m_steering_params[TrackerInfo::Reg_Endcap_Pos];
      sp.reserve_plan(3 + 3 + 6 + 18);
      sp.fill_plan(0, 1, false, true); // bk-fit only
      sp.append_plan(2, true);         // pickup-only
      sp.append_plan(18, false);
      sp.append_plan(19, false);
      sp.append_plan(20, false);
      sp.fill_plan(21, 26); // TID,  6 layers
      sp.fill_plan(27, 44); // TEC, 18 layers
      sp.finalize_plan();
    }
  }

  void SetupIterationParams(IterationParams& ip, unsigned int it=0)
  {
    if (it == 0)
    {
      ip.nlayers_per_seed = 4;
      ip.maxCandsPerSeed  = 5;
      ip.maxHolesPerCand  = 4;
      ip.maxConsecHoles   = 1;
      ip.chi2Cut          = 30;
      ip.chi2CutOverlap   = 3.5;
      ip.pTCutOverlap     = 1;
    }
    else if (it == 1)
    {   
      ip.nlayers_per_seed = 3;
      ip.maxCandsPerSeed  = 5;
      ip.maxHolesPerCand  = 4;
      ip.maxConsecHoles   = 1;
      ip.chi2Cut          = 30; 
      ip.chi2CutOverlap   = 3.5;
      ip.pTCutOverlap     = 1;
    }  
  }

  std::function<IterationConfig::partition_seeds_foo> PartitionSeeds0 =
  [](const TrackerInfo &trk_info, const TrackVec &in_seeds, const EventOfHits &eoh,
     IterationSeedPartition &part)
  {
    // Seeds are placed into eta regions and sorted on region + eta.

    const int size = in_seeds.size();

    for (int i = 0; i < size; ++i)
    {
      const Track &S = in_seeds[i];

      const bool z_dir_pos = S.pz() > 0;

      HitOnTrack hot = S.getLastHitOnTrack();
      // MIMI ACHTUNG -- here we assume seed hits have already been remapped.
      // This was true at that time :)
      float eta = eoh[hot.layer].GetHit(hot.index).eta();
      // float  eta = S.momEta();

      // Region to be defined by propagation / intersection tests
      TrackerInfo::EtaRegion reg;

      // Hardcoded for cms ... needs some lists of layers (hit/miss) for brl / ecp tests.
      // MM: Check lambda functions/std::function
      const LayerInfo &outer_brl = trk_info.outer_barrel_layer();

      const LayerInfo &tib1 = trk_info.m_layers[4];
      const LayerInfo &tob1 = trk_info.m_layers[10];

      const LayerInfo &tecp1 = trk_info.m_layers[27];
      const LayerInfo &tecn1 = trk_info.m_layers[54];

      const LayerInfo &tec_first = z_dir_pos ? tecp1 : tecn1;

      // If a track hits outer barrel ... it is in the barrel (for central, "outgoing" tracks).
      // This is also true for cyl-cow.
      // Better check is: hits outer TIB, misses inner TEC (but is +-z dependant).
      // XXXX Calculate z ... then check is inside or less that first EC z.
      // There are a lot of tracks that go through that crack.

      // XXXX trying a fix for low pT tracks that are in barrel after half circle
      float maxR = S.maxReachRadius();
      float z_at_maxr;

      bool  can_reach_outer_brl = S.canReachRadius(outer_brl.m_rout);
      float z_at_outer_brl;
      bool  misses_first_tec;
      if (can_reach_outer_brl)
      {
        z_at_outer_brl = S.zAtR(outer_brl.m_rout);
        if (z_dir_pos)
          misses_first_tec = z_at_outer_brl < tec_first.m_zmin;
        else
          misses_first_tec = z_at_outer_brl > tec_first.m_zmax;
      }
      else
      {
        z_at_maxr = S.zAtR(maxR);
        if (z_dir_pos)
          misses_first_tec = z_at_maxr < tec_first.m_zmin;
        else
          misses_first_tec = z_at_maxr > tec_first.m_zmax;
      }

      if (/*can_reach_outer_brl &&*/ misses_first_tec)
      // outer_brl.is_within_z_limits(S.zAtR(outer_brl.r_mean())))
      {
        reg = TrackerInfo::Reg_Barrel;
      }
      else
      {
        // This should be a list of layers
        // CMS, first tib, tob: 4, 10

        if ((S.canReachRadius(tib1.m_rin) && tib1.is_within_z_limits(S.zAtR(tib1.m_rin))) ||
            (S.canReachRadius(tob1.m_rin) && tob1.is_within_z_limits(S.zAtR(tob1.m_rin))))
        {
          // transition region ... we are still hitting barrel layers

          reg = z_dir_pos ? TrackerInfo::Reg_Transition_Pos : TrackerInfo::Reg_Transition_Neg;
        }
        else
        {
          // endcap ... no barrel layers will be hit anymore.

          reg = z_dir_pos ? TrackerInfo::Reg_Endcap_Pos : TrackerInfo::Reg_Endcap_Neg;
        }
      }

      part.m_region[i]     = reg;
      part.m_sort_score[i] = 5.0f * (reg - 2) + eta;
    }
  };


  void Create_CMS_2017(TrackerInfo& ti, IterationsInfo& ii, bool verbose)
  {
    Config::nTotalLayers     = 18 + 2 * 27;

    Config::useCMSGeom       = true;

    Config::finding_requires_propagation_to_hit_pos = true;
    Config::finding_inter_layer_pflags = PropagationFlags(PF_use_param_b_field | PF_apply_material);
    Config::finding_intra_layer_pflags = PropagationFlags(PF_none);
    Config::backward_fit_pflags        = PropagationFlags(PF_use_param_b_field | PF_apply_material);
    Config::forward_fit_pflags         = PropagationFlags(PF_use_param_b_field | PF_apply_material);
    Config::seed_fit_pflags            = PropagationFlags(PF_none);
    Config::pca_prop_pflags            = PropagationFlags(PF_none);

    ti.set_eta_regions(0.9, 1.7, 2.45, false);
    ti.create_layers(18, 27, 27);

    ii.resize(3);
    ii[0].set_iteration_index_and_track_algorithm(0, (int) TrackBase::TrackAlgorithm::initialStep);
    ii[0].set_num_regions_layers(5, 72);

    Create_CMS_2017_AutoGen(ti, ii);

    SetupSteeringParams_Iter0(ii[0]);
    SetupIterationParams(ii[0].m_params, 0);
    ii[0].m_partition_seeds = PartitionSeeds0;

    ii[1].Clone(ii[0]);
    SetupIterationParams(ii[1].m_params, 1);//3 seed hits is the only difference
    ii[1].set_iteration_index_and_track_algorithm(1, (int) TrackBase::TrackAlgorithm::highPtTripletStep);
    ii[1].set_seed_cleaning_params(2.0, 0.018, 0.018, 0.018, 0.018, 0.018, 0.05, 0.018, 0.05); //maybe have he parameters stored somewhere
    ii[2].Clone(ii[0]);
    ii[2].set_iteration_index_and_track_algorithm(2, (int) TrackBase::TrackAlgorithm::lowPtQuadStep);
    ii[2].set_seed_cleaning_params(0.5, 0.018, 0.018, 0.018, 0.018, 0.05, 0.05, 0.05, 0.05);

    if (verbose)
    {
      printf("==========================================================================================\n");
    }

    printf("CMS-2017 -- Create_TrackerInfo finished\n");

    if (verbose)
    {
      printf("==========================================================================================\n");
      for (auto &i : ti.m_layers)  i.print_layer();
      printf("==========================================================================================\n");
    }
  }
}


void* TrackerInfoCrator_ptr = (void*) Create_CMS_2017;
