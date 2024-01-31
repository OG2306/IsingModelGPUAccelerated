#include "Control.h"
#include <cassert>

int main(int argc, const char** argv)
{
	assert(argc == 2);
	int runCommand = std::atoi(argv[1]);

	switch (runCommand)
	{
	case ISING_GPU_USER_INPUT_RUN:
		IsingGPUUserInputRun();
		break;
	case ISING_GPU_HARDCODED_RUN:
		IsingGPUHardcodedRun();
		break;
	case ISING_CPU_USER_INPUT_RUN:
		IsingCPUUserInputRun();
		break;
	case ISING_CPU_HARDCODED_RUN:
		IsingCPUHardcodedRun();
		break;
	case ISING_LOAD_AND_PLOT_BINDER_CUMULANT_DATA_USER_INPUT_RUN:
		IsingLoadAndPlotBinderCumulantDataUserInputRun();
		break;
	case ISING_GPU_HARDCODED_MULTIPLE_GRIDS_AND_AUTO_SAVE_RUN:
		IsingGPUHardcodedMultipleGridsAndAutoSaveRun();
		break;
	case ISING_CPU_HARDCODED_MULTIPLE_GRIDS_AND_AUTO_SAVE_RUN:
		IsingCPUHardcodedMultipleGridsAndAutoSaveRun();
		break;
	case ISING_LOAD_AND_PLOT_BINDER_CUMULANT_DATA_HARDCODED_RUN:
		IsingLoadAndPlotBinderCumulantDataHardcodedRun();
		break;
	default:
		break;
	}

	return 0;
}