#pragma once
#include "TMultiGraph.h"
#include "TLegend.h"
#include <vector>

enum eIsingRunCommands
{
	ISING_GPU_USER_INPUT_RUN,
	ISING_GPU_HARDCODED_RUN,
	ISING_CPU_USER_INPUT_RUN,
	ISING_CPU_HARDCODED_RUN,
	ISING_LOAD_AND_PLOT_BINDER_CUMULANT_DATA_USER_INPUT_RUN,
	ISING_GPU_HARDCODED_MULTIPLE_GRIDS_AND_AUTO_SAVE_RUN,
	ISING_CPU_HARDCODED_MULTIPLE_GRIDS_AND_AUTO_SAVE_RUN,
	ISING_LOAD_AND_PLOT_BINDER_CUMULANT_DATA_HARDCODED_RUN
};

struct sIsingParameters
{
	uint32_t isingL;
	double startBeta;
	double endBeta;
	double betaDecrement;
	uint32_t numberOfSweepsPerTemperature;
	uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts;
	uint32_t sweepsPerSpinSumSample;
	const char* GPUOrCPUIdentifierText;
};

void IsingGPUUserInputRun();

void IsingGPUHardcodedRun();

void IsingCPUUserInputRun();

void IsingCPUHardcodedRun();

void IsingLoadAndPlotBinderCumulantDataUserInputRun();

void IsingGPUHardcodedMultipleGridsAndAutoSaveRun();

void IsingCPUHardcodedMultipleGridsAndAutoSaveRun();

void IsingLoadAndPlotBinderCumulantDataHardcodedRun();

void SaveBinderCumulantData(const char* filename, sIsingParameters isingParameters, double computationTime, std::vector<double>& betaValues, std::vector<double>& binderCumulants);

void LoadAndAddBinderCumulantDataToRootMultiGraph(const char* filename, TMultiGraph* rootMultiGraph, TLegend* rootMultiGraphLegend, int numberUsedToSetGraphMarkerStyleAndColor);