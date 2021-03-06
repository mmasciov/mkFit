#! /bin/bash

###########
## Input ##
###########

suite=${1:-"forConf"} # which set of benchmarks to run: full, forPR, forConf, val, valMT1
style=${2:-"--mtv-like-val"} # option --mtv-like-val
inputBin=${3:-"104XPU50CCC_MULTI"}

###################
## Configuration ##
###################

source xeon_scripts/common-variables.sh ${suite}
source xeon_scripts/init-env.sh
export MIMI="CE mimi"
declare -a val_builds=(MIMI)
nevents=500

## Common file setup
case ${inputBin} in 
"104XPU50CCC_MULTI")
        echo "Inputs from 2018 initialStep/default PU 50 with CCC with multiple iterations and hit binary mask"
        dir=/data2/slava77/analysis/CMSSW_10_4_0_patch1_mkFit/pass-df52fcc
        subdir=/initialStep/default/11024.0_TTbar_13/AVE_50_BX01_25ns/RAW4NT  
        file=/memoryFile.fv5.clean.writeAll.CCC1620.recT.allSeeds.masks.201023-64302e5.bin
        ;;
*)
        echo "INPUT BIN IS UNKNOWN"
        exit 12
        ;;
esac

## Common executable setup
maxth=64
maxvu=16
maxev=32
if [[  "${suite}" == "valMT1" ]]
then
    maxth=1
    maxev=1
fi
seeds="--cmssw-n2seeds"
exe="./mkFit/mkFit --silent ${seeds} --num-thr ${maxth} --num-thr-ev ${maxev} --input-file ${dir}/${subdir}/${file} --num-events ${nevents} --remove-dup"

## Common output setup
tmpdir="tmp"
base=${val_arch}_${sample}

## flag to save sim info for matched tracks since track states not read in
siminfo="--try-to-save-sim-info"

## backward fit flag
bkfit="--backward-fit-pca"

## validation options: SIMVAL == sim tracks as reference, CMSSWVAL == cmssw tracks as reference
SIMVAL="SIMVAL --sim-val ${siminfo} ${bkfit} ${style}"
SIMVAL_SEED="SIMVALSEED --sim-val ${siminfo} ${bkfit} --mtv-require-seeds"
declare -a vals=(SIMVAL SIMVAL_SEED)

## plotting options
SIMPLOT="SIMVAL all 0 0 1"
SIMPLOTSEED="SIMVALSEED all 0 0 1"
SIMPLOT4="SIMVAL iter4 0 4 0"
SIMPLOTSEED4="SIMVALSEED iter4 0 4 0" 
SIMPLOT22="SIMVAL iter22 0 22 0"
SIMPLOTSEED22="SIMVALSEED iter22 0 22 0"
SIMPLOT23="SIMVAL iter23 0 23 0"
SIMPLOTSEED23="SIMVALSEED iter23 0 23 0"

declare -a plots=(SIMPLOT4 SIMPLOTSEED4 SIMPLOT22 SIMPLOTSEED22 SIMPLOT23 SIMPLOTSEED23 SIMPLOT SIMPLOTSEED)

## special cmssw dummy build
CMSSW="CMSSW cmssw SIMVAL --sim-val-for-cmssw ${siminfo} --read-cmssw-tracks ${style} --num-iters-cmssw 3"
CMSSW2="CMSSW cmssw SIMVALSEED --sim-val-for-cmssw ${siminfo} --read-cmssw-tracks --mtv-require-seeds --num-iters-cmssw 3"

###############
## Functions ##
###############

## validation function
function doVal()
{
    local bN=${1}
    local bO=${2}
    local vN=${3}
    local vO=${4}

    local oBase=${val_arch}_${sample}_${bN}
    local bExe="${exe} ${vO} --build-${bO}"
    
    echo "${oBase}: ${vN} [nTH:${maxth}, nVU:${maxvu}int, nEV:${maxev}]"
    ${bExe} >& log_${oBase}_NVU${maxvu}int_NTH${maxth}_NEV${maxev}_${vN}.txt || (echo "Crashed on CMD: "${bExe}; exit 2)
    
    if (( ${maxev} > 1 ))
    then
        # hadd output files from different threads for this test, then move to temporary directory
        hadd -O valtree.root valtree_*.root
        rm valtree_*.root
    fi
    mv valtree.root ${tmpdir}/valtree_${oBase}_${vN}.root
}		

## plotting function
function plotVal()
{
    local base=${1}
    local bN=${2}
    local pN=${3}
    local pO=${4}
    local iter=${5}
    local cancel=${6}     

    echo "Computing observables for: ${base} ${bN} ${pN} ${p0} ${iter} ${cancel}"
    bExe="root -b -q -l plotting/runValidation.C(\"_${base}_${bN}_${pN}\",${pO},${iter},${cancel})"
    echo ${bExe}

    ${bExe} || (echo "Crashed on CMD: "${bExe}; exit 3)
}

########################
## Run the validation ##
########################

## Compile once
make clean
mVal="-j 32 WITH_ROOT:=1 AVX_512:=1"
make ${mVal}
mkdir -p ${tmpdir}

## Special simtrack validation vs cmssw tracks
echo ${CMSSW} | while read -r bN bO vN vO
do
    doVal "${bN}" "${bO}" "${vN}" "${vO}"
done
## Special simtrack validation vs cmssw tracks
echo ${CMSSW2} | while read -r bN bO vN vO
do
    doVal "${bN}" "${bO}" "${vN}" "${vO}"
done

## Run validation for standard build options
for val in "${vals[@]}"
do echo ${!val} | while read -r vN vO
    do
	for build in "${val_builds[@]}"
	do echo ${!build} | while read -r bN bO
	    do
		doVal "${bN}" "${bO}" "${vN}" "${vO}"
	    done
	done
    done
done

## clean up
make clean ${mVal}
mv tmp/valtree_*.root .
rm -rf ${tmpdir}



## Compute observables and make images
for plot in "${plots[@]}"
do echo ${!plot} | while read -r pN suff pO iter cancel
    do
        ## Compute observables for special dummy CMSSW
	if [[ "${pN}" == "SIMVAL" || "${pN}" == "SIMVAL_"* ]]
	then
	    echo ${CMSSW} | while read -r bN bO val_extras
	    do
		plotVal "${base}" "${bN}" "${pN}" "${pO}" "${iter}" "${cancel}"
	    done
	fi
	if [[ "${pN}" == "SIMVALSEED"* ]]
	then
	    echo ${CMSSW2} | while read -r bN bO val_extras
	    do
		plotVal "${base}" "${bN}" "${pN}" "${pO}" "${iter}" "${cancel}"
	    done
	fi

	## Compute observables for builds chosen 
	for build in "${val_builds[@]}"
	do echo ${!build} | while read -r bN bO
	    do
		plotVal "${base}" "${bN}" "${pN}" "${pO}" "${iter}" "${cancel}"
	    done
	done
	
	## overlay histograms
	echo "Overlaying histograms for: ${base} ${vN}"
        if [[  "${suff}" == "all" ]]
        then
	    root -b -q -l plotting/makeValidation.C\(\"${base}\",\"_${pN}\",${pO},\"${suite}\"\)
        else
            root -b -q -l plotting/makeValidation.C\(\"${base}\",\"_${pN}_${suff}\",${pO},\"${suite}\"\)
        fi
    done
done

## Final cleanup
make distclean ${mVal}

## Final message
echo "Finished physics validation!"
