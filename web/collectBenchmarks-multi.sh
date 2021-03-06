#! /bin/bash

###########
## Input ##
###########

dir=${1:-"benchmarks"}
suite=${2:-"forConf"} # which set of benchmarks to run: full, forPR, forConf
useARCH=${3:-0}

###################
## Configuration ##
###################
source xeon_scripts/common-variables.sh ${suite} ${useARCH}
source xeon_scripts/init-env.sh
export MIMI="CE mimi"
declare -a val_builds=(MIMI)

######################################
## Move Physics Performance Results ##
######################################

# Make SimTrack Validation directories
simdir=("SIMVAL_MTV" "SIMVAL_MTV_SEED" "SIMVAL_MTV_iter4" "SIMVAL_MTV_SEED_iter4" "SIMVAL_MTV_iter22" "SIMVAL_MTV_SEED_iter22" "SIMVAL_MTV_iter23" "SIMVAL_MTV_SEED_iter23")
simval=("SIMVAL" "SIMVALSEED" "SIMVAL_iter4" "SIMVALSEED_iter4" "SIMVAL_iter22" "SIMVALSEED_iter22" "SIMVAL_iter23" "SIMVALSEED_iter23" )

for((i=0;i<${#simdir[@]};++i));do

mkdir -p ${dir}/${simdir[i]}
mkdir -p ${dir}/${simdir[i]}/logx
mkdir -p ${dir}/${simdir[i]}/diffs
mkdir -p ${dir}/${simdir[i]}/nHits
mkdir -p ${dir}/${simdir[i]}/score

# Move text file dumps for SimTrack Validation
for build in "${val_builds[@]}"
do echo ${!build} | while read -r bN bO
    do
        vBase=${val_arch}_${sample}_${bN}
        mv "validation"_${vBase}_${simval[i]}/"totals_validation"_${vBase}_${simval[i]}.txt ${dir}/${simdir[i]}
    done
done

# Move dummy CMSSW text file (SimTrack Validation)
vBase=${val_arch}_${sample}_CMSSW
mv validation_${vBase}_${simval[i]}/totals_validation_${vBase}_${simval[i]}.txt ${dir}/${simdir[i]}

# Move rate plots for SimTrack Validation
for rate in eff ineff_brl ineff_trans ineff_ec dr fr
do
    for pt in 0p0 0p9 2p0
    do
        for var in phi eta nLayers
        do
            mv ${val_arch}_${sample}_${rate}_${var}_"build"_"pt"${pt}_${simval[i]}.png ${dir}/${simdir[i]}
        done
    done

    # only copy pt > 0 for pt rate plots
    for var in pt pt_zoom
    do
        mv ${val_arch}_${sample}_${rate}_${var}_"build"_"pt0p0"_${simval[i]}.png ${dir}/${simdir[i]}
    done

    mv ${val_arch}_${sample}_${rate}_"pt_logx"_"build"_"pt0p0"_${simval[i]}.png ${dir}/${simdir[i]}/logx
done

# Move kinematic diff plots for SimTrack Validation
for coll in bestmatch allmatch
do
    for var in nHits invpt phi eta
    do
        for pt in 0p0 0p9 2p0
        do
            mv ${val_arch}_${sample}_${coll}_"d"${var}_"build"_"pt"${pt}_${simval[i]}.png ${dir}/${simdir[i]}/diffs
        done
    done
done

# Move track quality plots for SimTrack Validation (nHits,score)
for coll in allreco fake bestmatch allmatch
do
    for pt in 0p0 0p9 2p0
    do
        for qual in nHits score
        do
            mv ${val_arch}_${sample}_${coll}_${qual}_"build"_"pt"${pt}_${simval[i]}.png ${dir}/${simdir[i]}/${qual}
        done
    done
done
done

# Final message
echo "Finished collecting benchmark plots into ${dir}!"

find ${dir}  -mindepth 0 -type d -exec cp web/index.php {} \;

rm -rf log_*.txt
rm -rf *.root
rm -rf *.png
rm -rf validation_*

