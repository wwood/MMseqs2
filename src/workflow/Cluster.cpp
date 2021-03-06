#include "Parameters.h"
#include <cassert>
#include <cascaded_clustering.sh.h>
#include <clustering.sh.h>
#include <Util.h>

#include "DBWriter.h"
#include "CommandCaller.h"
#include "Debug.h"
#include "FileUtil.h"


void setWorkflowDefaults(Parameters *p) {
    p->spacedKmer = true;
    p->covThr = 0.8;
    p->evalThr = 0.001;
    p->alignmentMode = Parameters::ALIGNMENT_MODE_SCORE_COV;
}

std::pair<float, bool> setAutomaticThreshold(float seqId) {
    float sens;
    bool cascaded = true;
    if (seqId <= 0.3) {
        sens = 6;
        cascaded = true;
    } else if (seqId > 0.8) {
        sens = 1.0;
    } else {
        sens = 1.0 + (1.0 * (0.8 - seqId) * 10);
    }
    if (sens <= 2.0) {
        cascaded = false;
    }
    return std::make_pair(sens, cascaded);
}

int clusteringworkflow(int argc, const char **argv, const Command& command) {
    Parameters& par = Parameters::getInstance();
    setWorkflowDefaults(&par);
    par.overrideParameterDescription((Command &)command, par.PARAM_RESCORE_MODE.uniqid, NULL, NULL, par.PARAM_RESCORE_MODE.category |MMseqsParameter::COMMAND_EXPERT );
    par.overrideParameterDescription((Command &)command, par.PARAM_MAX_REJECTED.uniqid, NULL, NULL, par.PARAM_MAX_REJECTED.category |MMseqsParameter::COMMAND_EXPERT );
    par.overrideParameterDescription((Command &)command, par.PARAM_MAX_ACCEPT.uniqid, NULL, NULL, par.PARAM_MAX_ACCEPT.category |MMseqsParameter::COMMAND_EXPERT );
    par.overrideParameterDescription((Command &)command, par.PARAM_KMER_PER_SEQ.uniqid, NULL, NULL, par.PARAM_KMER_PER_SEQ.category |MMseqsParameter::COMMAND_EXPERT );
    par.overrideParameterDescription((Command &)command, par.PARAM_S.uniqid, "sensitivity will be automatically determined but can be adjusted", NULL,  par.PARAM_S.category |MMseqsParameter::COMMAND_EXPERT);

    par.parseParameters(argc, argv, command, 3);
    if(FileUtil::directoryExists(par.db3.c_str())==false){
        Debug(Debug::WARNING) << "Tmp " << par.db3 << " folder does not exist or is not a directory.\n";
        if(FileUtil::makeDir(par.db3.c_str()) == false){
            Debug(Debug::WARNING) << "Could not crate tmp folder " << par.db3 << ".\n";
            EXIT(EXIT_FAILURE);
        }else{
            Debug(Debug::WARNING) << "Created dir " << par.db3 << "\n";
        }
    }
    bool parameterSet = false;
    bool compositionBiasSet = false;
    bool minDiagonalScore = false;

    for (size_t i = 0; i < par.clusteringWorkflow.size(); i++) {
        if (par.clusteringWorkflow[i].uniqid == par.PARAM_S.uniqid && par.clusteringWorkflow[i].wasSet) {
            parameterSet = true;
        }
        if (par.clusteringWorkflow[i].uniqid == par.PARAM_CASCADED.uniqid && par.clusteringWorkflow[i].wasSet) {
            parameterSet = true;
        }
        if (par.clusteringWorkflow[i].uniqid == par.PARAM_NO_COMP_BIAS_CORR.uniqid && par.clusteringWorkflow[i].wasSet) {
            compositionBiasSet = true;
        }
        if (par.clusteringWorkflow[i].uniqid == par.PARAM_MIN_DIAG_SCORE.uniqid && par.clusteringWorkflow[i].wasSet) {
            minDiagonalScore = true;
        }
    }

    if (compositionBiasSet == false){
        if(par.seqIdThr > 0.7){
            par.compBiasCorrection = 0;
        }
    }

    if (minDiagonalScore == false){
        if(par.seqIdThr > 0.7){
            par.minDiagScoreThr = 60;
        }
    }

    if (parameterSet == false) {
        std::pair<float, bool> settings = setAutomaticThreshold(par.seqIdThr);
        par.sensitivity = settings.first;
        par.cascaded = settings.second;
        Debug(Debug::WARNING) << "Set cluster settings automatic to s=" << par.sensitivity << " cascaded=" <<
        par.cascaded << "\n";
    }
    size_t hash = par.hashParameter(par.filenames, par.clusteringWorkflow);
    std::string tmpDir = par.db3+"/"+SSTR(hash);
    if(FileUtil::directoryExists(tmpDir.c_str())==false) {
        if (FileUtil::makeDir(tmpDir.c_str()) == false) {
            Debug(Debug::WARNING) << "Could not create sub tmp folder " << tmpDir << ".\n";
            EXIT(EXIT_FAILURE);
        }
    }
    par.filenames.pop_back();
    par.filenames.push_back(tmpDir);
    if(FileUtil::symlinkCreateOrRepleace(par.db3+"/latest", tmpDir) == false){
        Debug(Debug::WARNING) << "Could not link latest folder in tmp." << tmpDir << ".\n";
        EXIT(EXIT_FAILURE);
    }
//    FileUtil::errorIfFileExist(par.db2.c_str());
//    FileUtil::errorIfFileExist(par.db2Index.c_str());

    CommandCaller cmd;
//    FileUtil::writeFile(clustering_sh, clustering_sh_len);
    if(par.removeTmpFiles) {
        cmd.addVariable("REMOVE_TMP", "TRUE");
    }

    cmd.addVariable("RUNNER", par.runner.c_str());

    if (par.cascaded) {
        // save some values to restore them later
        float targetSensitivity = par.sensitivity;
        size_t maxResListLen = par.maxResListLen;

        int alphabetSize = par.alphabetSize;
        par.alphabetSize = Parameters::CLUST_LINEAR_DEFAULT_ALPH_SIZE;
        int kmerSize = par.kmerSize;
        par.kmerSize = Parameters::CLUST_LINEAR_DEFAULT_K;
        int maskMode = par.maskMode;
        par.maskMode = 0;
        int alignmentMode = par.alignmentMode;
        par.alignmentMode = Parameters::ALIGNMENT_MODE_SCORE_COV;
        cmd.addVariable("LINCLUST_PAR", par.createParameterString(par.linclustworkflow).c_str());
        par.alphabetSize = alphabetSize;
        par.kmerSize = kmerSize;
        par.maskMode = maskMode;
        par.alignmentMode = alignmentMode;
        // 1 is lowest sens
//        par.clusteringMode = Parameters::GREEDY;
        par.sensitivity = 1;
        par.maxResListLen = 20;
        int minDiagScoreThr = par.minDiagScoreThr;
        par.minDiagScoreThr = 0;
        par.diagonalScoring = 0;
        par.compBiasCorrection = 0;

        cmd.addVariable("PREFILTER0_PAR", par.createParameterString(par.prefilter).c_str());
        cmd.addVariable("ALIGNMENT0_PAR", par.createParameterString(par.align).c_str());
        cmd.addVariable("CLUSTER0_PAR",   par.createParameterString(par.clust).c_str());

        // set parameter for first step
//        par.clusteringMode = Parameters::SET_COVER;
        par.sensitivity = targetSensitivity / 3.0;
        par.maxResListLen = 100;
        par.diagonalScoring = 1;
        par.compBiasCorrection = 1;
        par.minDiagScoreThr = minDiagScoreThr;

        cmd.addVariable("PREFILTER1_PAR", par.createParameterString(par.prefilter).c_str());
        cmd.addVariable("ALIGNMENT1_PAR", par.createParameterString(par.align).c_str());
        cmd.addVariable("CLUSTER1_PAR", par.createParameterString(par.clust).c_str());

        // set parameter for second step
        par.sensitivity = targetSensitivity * (2.0 / 3.0);
        par.maxResListLen = 200;
        cmd.addVariable("PREFILTER2_PAR", par.createParameterString(par.prefilter).c_str());
        cmd.addVariable("ALIGNMENT2_PAR", par.createParameterString(par.align).c_str());
        cmd.addVariable("CLUSTER2_PAR", par.createParameterString(par.clust).c_str());

        // set parameter for last step
        par.sensitivity = targetSensitivity;
        par.maxResListLen = maxResListLen;
        cmd.addVariable("PREFILTER3_PAR", par.createParameterString(par.prefilter).c_str());
        cmd.addVariable("ALIGNMENT3_PAR", par.createParameterString(par.align).c_str());
        cmd.addVariable("CLUSTER3_PAR", par.createParameterString(par.clust).c_str());
        FileUtil::writeFile(tmpDir + "/cascaded_clustering.sh", cascaded_clustering_sh, cascaded_clustering_sh_len);
        std::string program(tmpDir + "/cascaded_clustering.sh");
        cmd.execProgram(program.c_str(), par.filenames);
    } else {
        // same as above, clusthash needs a smaller alphabetsize
        size_t alphabetSize = par.alphabetSize;
        par.alphabetSize = Parameters::CLUST_LINEAR_DEFAULT_ALPH_SIZE;
        size_t kmerSize = par.kmerSize;
        par.kmerSize = Parameters::CLUST_LINEAR_DEFAULT_K;
        cmd.addVariable("CLUSTLINEAR_PAR", par.createParameterString(par.kmermatcher).c_str());
        par.alphabetSize = alphabetSize;
        par.kmerSize = kmerSize;
        cmd.addVariable("PREFILTER_PAR", par.createParameterString(par.prefilter).c_str());
        cmd.addVariable("ALIGNMENT_PAR", par.createParameterString(par.align).c_str());
        cmd.addVariable("CLUSTER_PAR", par.createParameterString(par.clust).c_str());
        FileUtil::writeFile(tmpDir + "/clustering.sh", clustering_sh, clustering_sh_len);
        std::string program(tmpDir+ "/clustering.sh");
        cmd.execProgram(program.c_str(), par.filenames);
    }

    // Unreachable
    assert(false);

    return 0;
}
