#include "Control.h"
#include "Setup.h"
#include <TApplication.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TMultiGraph.h>
#include <TLegend.h>
#include <vector>
#include <array>
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <limits>
#include <cmath>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <string>
#include <sstream>


void IsingGPUUserInputRun()
{
	sIsingParameters isingParameters;
	isingParameters.GPUOrCPUIdentifierText = "GPU";
	std::cout << "Enter the grid length: ";
	std::cin >> isingParameters.isingL;
	assert(isingParameters.isingL <= 2000);
	std::cout << "Enter the start value of beta: ";
	std::cin >> isingParameters.startBeta;
	assert(isingParameters.startBeta > 0.0);
	std::cout << "Enter the end value of beta (should be lower than the start value): ";
	std::cin >> isingParameters.endBeta;
	assert(isingParameters.endBeta < isingParameters.startBeta);
	std::cout << "Enter how much the value of beta is decremented for every set of sweeps: ";
	std::cin >> isingParameters.betaDecrement;
	assert(isingParameters.betaDecrement > 0.0 && isingParameters.betaDecrement <= (isingParameters.startBeta - isingParameters.endBeta));
	std::cout << "Enter the number of sweeps for every value of beta: ";
	std::cin >> isingParameters.numberOfSweepsPerTemperature;
	assert(isingParameters.numberOfSweepsPerTemperature <= 10'000'000);
	std::cout << "Enter how many sweeps to wait for every value of beta before spin sum sampling starts: ";
	std::cin >> isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts;
	assert(isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts < isingParameters.numberOfSweepsPerTemperature);
	std::cout << "Enter how many sweeps should happen per sample after the wait: ";
	std::cin >> isingParameters.sweepsPerSpinSumSample;
	assert(isingParameters.sweepsPerSpinSumSample <= (isingParameters.numberOfSweepsPerTemperature - isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts));
	std::cout << '\n';

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

	std::cout << "The computation has started...\n";

	int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((isingParameters.startBeta - isingParameters.endBeta) / isingParameters.betaDecrement);
	std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
	std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

	try
	{
		cSetup TheSetup(isingParameters.isingL, isingParameters.numberOfSweepsPerTemperature, isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts,
			isingParameters.sweepsPerSpinSumSample, COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN);

		double beta = isingParameters.startBeta;
		for (int i = 0; i < numberOfDataPointsForTheBinderCumulantPlot; i++)
		{
			DoTheIsingGridSweepsGPU(&TheSetup, isingParameters.isingL, beta, isingParameters.numberOfSweepsPerTemperature,
				isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts, isingParameters.sweepsPerSpinSumSample);

			betaValues[i] = beta;
			binderCumulants[i] = CalculateBinderCumulantGPU(&TheSetup, isingParameters.isingL);

			beta -= isingParameters.betaDecrement;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
	std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

	std::cout << "The computation has finished.\nCOMPUTATION TIME (seconds): " << (computationTime.count()) << '\n';

	// Ask the user to save the data
	char saveDataOrNotUserInput;
	std::cout << "\nSave data before displaying plot (Y/n)?\n";
	std::cin >> saveDataOrNotUserInput;

	if (saveDataOrNotUserInput == 89 || saveDataOrNotUserInput == 121)
	{
		std::string filename;
		std::cout << "Enter the filename: ";
		std::cin >> filename;
		SaveBinderCumulantData(filename.c_str(), isingParameters, computationTime.count(), betaValues, binderCumulants);
	}

	// Plot the Binder cumulant graph
	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta (GPU);#beta;Binder cumulant");
	TGraph* rootBinderCumulantGraph = new TGraph(numberOfDataPointsForTheBinderCumulantPlot, betaValues.data(), binderCumulants.data());
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.2);
	std::string legendEntryText = "L: ";
	legendEntryText.append(std::to_string(isingParameters.isingL));
	rootMultiGraphLegend->AddEntry(rootBinderCumulantGraph, legendEntryText.c_str(), "P");
	rootMultiGraph->Add(rootBinderCumulantGraph, "*");
	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();

	rootApp->Run();

	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootBinderCumulantGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingGPUHardcodedRun()
{
	eComputeShaderType computeShaderType = COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN;
	sIsingParameters isingParameters =
	{
		.isingL = 20,
		.startBeta = 0.50,
		.endBeta = 0.35,
		.betaDecrement = 0.01,
		.numberOfSweepsPerTemperature = 10000,
		.numberOfSweepsToWaitBeforeSpinSumSamplingStarts = 100,
		.sweepsPerSpinSumSample = 2,
		.GPUOrCPUIdentifierText = "GPU"
	};

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

	std::cout << "The computation has started...\n";

	int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((isingParameters.startBeta - isingParameters.endBeta) / isingParameters.betaDecrement);
	std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
	std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

	try
	{
		cSetup TheSetup(isingParameters.isingL, isingParameters.numberOfSweepsPerTemperature, isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts,
			isingParameters.sweepsPerSpinSumSample, computeShaderType);

		double beta = isingParameters.startBeta;
		for (int i = 0; i < numberOfDataPointsForTheBinderCumulantPlot; i++)
		{
			DoTheIsingGridSweepsGPU(&TheSetup, isingParameters.isingL, beta, isingParameters.numberOfSweepsPerTemperature,
				isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts, isingParameters.sweepsPerSpinSumSample);

			betaValues[i] = beta;
			binderCumulants[i] = CalculateBinderCumulantGPU(&TheSetup, isingParameters.isingL);

			beta -= isingParameters.betaDecrement;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << '\n';
	}

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
	std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

	std::cout << "The computation has finished.\nCOMPUTATION TIME (seconds): " << (computationTime.count()) << '\n';

	// Ask the user to save the data
	char saveDataOrNotUserInput;
	std::cout << "\nSave data before displaying plot (Y/n)?\n";
	std::cin >> saveDataOrNotUserInput;

	if (saveDataOrNotUserInput == 89 || saveDataOrNotUserInput == 121)
	{
		std::string filename;
		std::cout << "Enter the filename: ";
		std::cin >> filename;
		SaveBinderCumulantData(filename.c_str(), isingParameters, computationTime.count(), betaValues, binderCumulants);
	}

	// Plot the Binder cumulant graph
	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta (GPU);#beta;Binder cumulant");
	TGraph* rootBinderCumulantGraph = new TGraph(numberOfDataPointsForTheBinderCumulantPlot, betaValues.data(), binderCumulants.data());
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.2);
	std::string legendEntryText = "L: ";
	legendEntryText.append(std::to_string(isingParameters.isingL));
	rootMultiGraphLegend->AddEntry(rootBinderCumulantGraph, legendEntryText.c_str(), "P");
	rootMultiGraph->Add(rootBinderCumulantGraph, "*");
	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();

	rootApp->Run();
	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootBinderCumulantGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingCPUUserInputRun()
{
	sIsingParameters isingParameters;
	isingParameters.GPUOrCPUIdentifierText = "CPU";
	std::cout << "Enter the grid length: ";
	std::cin >> isingParameters.isingL;
	assert(isingParameters.isingL <= 2000);
	std::cout << "Enter the start value of beta: ";
	std::cin >> isingParameters.startBeta;
	assert(isingParameters.startBeta > 0.0);
	std::cout << "Enter the end value of beta (should be lower than the start value): ";
	std::cin >> isingParameters.endBeta;
	assert(isingParameters.endBeta < isingParameters.startBeta);
	std::cout << "Enter how much the value of beta is decremented for every set of sweeps: ";
	std::cin >> isingParameters.betaDecrement;
	assert(isingParameters.betaDecrement > 0.0 && isingParameters.betaDecrement <= (isingParameters.startBeta - isingParameters.endBeta));
	std::cout << "Enter the number of sweeps for every value of beta: ";
	std::cin >> isingParameters.numberOfSweepsPerTemperature;
	assert(isingParameters.numberOfSweepsPerTemperature <= 10'000'000);
	std::cout << "Enter how many sweeps to wait for every value of beta before spin sum sampling starts: ";
	std::cin >> isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts;
	assert(isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts < isingParameters.numberOfSweepsPerTemperature);
	std::cout << "Enter how many sweeps should happen per sample after the wait: ";
	std::cin >> isingParameters.sweepsPerSpinSumSample;
	assert(isingParameters.sweepsPerSpinSumSample <= (isingParameters.numberOfSweepsPerTemperature - isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts));
	std::cout << '\n';

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

	std::cout << "The computation has started...\n";

	int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((isingParameters.startBeta - isingParameters.endBeta) / isingParameters.betaDecrement);
	std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
	std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

	// Set up the Ising grid on the CPU
	const uint32_t isingN = isingParameters.isingL * isingParameters.isingL;
	const uint32_t numberOfSpinBatches = (uint32_t) std::ceil(isingN / 32.0);
	const uint32_t numberOfElementsInTheSpinSumOutputArray = (isingParameters.numberOfSweepsPerTemperature - isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts - 1)
		/ isingParameters.sweepsPerSpinSumSample;																// Integer division
	uint32_t* pArraySpinBatches = new uint32_t[numberOfSpinBatches];
	int* pArraySpinSumOutputs = new int[numberOfElementsInTheSpinSumOutputArray];
	int TheSpinSum = isingN;
	for (uint32_t i = 0; i < numberOfSpinBatches; i++)
	{
		pArraySpinBatches[i] = ~0U;																					// All spins are +1
	}

	// Do the computation
	double beta = isingParameters.startBeta;
	for (int i = 0; i < numberOfDataPointsForTheBinderCumulantPlot; i++)
	{
		DoTheIsingGridSweepsCPU(pArraySpinBatches, pArraySpinSumOutputs, TheSpinSum, isingParameters.isingL, beta,
			isingParameters.numberOfSweepsPerTemperature, isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts, isingParameters.sweepsPerSpinSumSample);

		betaValues[i] = beta;
		binderCumulants[i] = CalculateBinderCumulantCPU(pArraySpinSumOutputs, isingParameters.isingL, numberOfElementsInTheSpinSumOutputArray);

		beta -= isingParameters.betaDecrement;
	}
	// ------------------


	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
	std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

	std::cout << "The computation has finished.\nCOMPUTATION TIME (seconds): " << (computationTime.count()) << '\n';

	// Ask the user to save the data
	char saveDataOrNotUserInput;
	std::cout << "\nSave data before displaying plot (Y/n)?\n";
	std::cin >> saveDataOrNotUserInput;

	if (saveDataOrNotUserInput == 89 || saveDataOrNotUserInput == 121)
	{
		std::string filename;
		std::cout << "Enter the filename: ";
		std::cin >> filename;
		SaveBinderCumulantData(filename.c_str(), isingParameters, computationTime.count(), betaValues, binderCumulants);
	}

	// Plot the Binder cumulant graph
	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta (CPU);#beta;Binder cumulant");
	TGraph* rootBinderCumulantGraph = new TGraph(numberOfDataPointsForTheBinderCumulantPlot, betaValues.data(), binderCumulants.data());
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.2);
	std::string legendEntryText = "L: ";
	legendEntryText.append(std::to_string(isingParameters.isingL));
	rootMultiGraphLegend->AddEntry(rootBinderCumulantGraph, legendEntryText.c_str(), "P");
	rootMultiGraph->Add(rootBinderCumulantGraph, "*");
	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();

	rootApp->Run();

	delete[] pArraySpinBatches;
	delete[] pArraySpinSumOutputs;
	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootBinderCumulantGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingCPUHardcodedRun()
{
	sIsingParameters isingParameters =
	{
		.isingL = 20,
		.startBeta = 0.50,
		.endBeta = 0.35,
		.betaDecrement = 0.01,
		.numberOfSweepsPerTemperature = 100000,
		.numberOfSweepsToWaitBeforeSpinSumSamplingStarts = 100,
		.sweepsPerSpinSumSample = 2,
		.GPUOrCPUIdentifierText = "CPU"
	};

	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

	std::cout << "The computation has started...\n";

	int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((isingParameters.startBeta - isingParameters.endBeta) / isingParameters.betaDecrement);
	std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
	std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

	// Set up the Ising grid on the CPU
	const uint32_t isingN = isingParameters.isingL * isingParameters.isingL;
	const uint32_t numberOfSpinBatches = (uint32_t)std::ceil(isingN / 32.0);
	const uint32_t numberOfElementsInTheSpinSumOutputArray = (isingParameters.numberOfSweepsPerTemperature - isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts - 1)
		/ isingParameters.sweepsPerSpinSumSample;																	// Integer division
	uint32_t* pArraySpinBatches = new uint32_t[numberOfSpinBatches];
	int* pArraySpinSumOutputs = new int[numberOfElementsInTheSpinSumOutputArray];
	int TheSpinSum = isingN;
	for (uint32_t i = 0; i < numberOfSpinBatches; i++)
	{
		pArraySpinBatches[i] = ~0U;																					// All spins are +1
	}

	// Do the computation
	double beta = isingParameters.startBeta;
	for (int i = 0; i < numberOfDataPointsForTheBinderCumulantPlot; i++)
	{
		DoTheIsingGridSweepsCPU(pArraySpinBatches, pArraySpinSumOutputs, TheSpinSum, isingParameters.isingL, beta,
			isingParameters.numberOfSweepsPerTemperature, isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts, isingParameters.sweepsPerSpinSumSample);

		betaValues[i] = beta;
		binderCumulants[i] = CalculateBinderCumulantCPU(pArraySpinSumOutputs, isingParameters.isingL, numberOfElementsInTheSpinSumOutputArray);

		beta -= isingParameters.betaDecrement;
	}
	// ------------------


	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
	std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

	std::cout << "The computation has finished.\nCOMPUTATION TIME (seconds): " << (computationTime.count()) << '\n';

	// Ask the user to save the data
	char saveDataOrNotUserInput;
	std::cout << "\nSave data before displaying plot (Y/n)?\n";
	std::cin >> saveDataOrNotUserInput;

	if (saveDataOrNotUserInput == 89 || saveDataOrNotUserInput == 121)
	{
		std::string filename;
		std::cout << "Enter the filename: ";
		std::cin >> filename;
		SaveBinderCumulantData(filename.c_str(), isingParameters, computationTime.count(), betaValues, binderCumulants);
	}

	// Plot the Binder cumulant graph
	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta (CPU);#beta;Binder cumulant");
	TGraph* rootBinderCumulantGraph = new TGraph(numberOfDataPointsForTheBinderCumulantPlot, betaValues.data(), binderCumulants.data());
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.2);
	std::string legendEntryText = "L: ";
	legendEntryText.append(std::to_string(isingParameters.isingL));
	rootMultiGraphLegend->AddEntry(rootBinderCumulantGraph, legendEntryText.c_str(), "P");
	rootMultiGraph->Add(rootBinderCumulantGraph, "*");
	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();

	rootApp->Run();

	delete[] pArraySpinBatches;
	delete[] pArraySpinSumOutputs;
	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootBinderCumulantGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingLoadAndPlotBinderCumulantDataUserInputRun()
{
	std::string filename;
	std::cout << "Enter the filename of the file to load: ";
	std::cin >> filename;

	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta;#beta;Binder cumulant");
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.2);

	LoadAndAddBinderCumulantDataToRootMultiGraph(filename.c_str(), rootMultiGraph, rootMultiGraphLegend, 0);

	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();
	rootApp->Run();
	
	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingLoadAndPlotBinderCumulantDataHardcodedRun()
{
	std::array<const char*, 5> filenames =
	{
		"L20GPU.txt",
		"L40GPU.txt",
		"L60GPU.txt",
		"L80GPU.txt",
		"L100GPU.txt"
	};

	TApplication* rootApp = new TApplication("app", nullptr, nullptr);
	TCanvas* rootCanvas = new TCanvas("canvas", "Ising GPU", 1280, 960);
	TMultiGraph* rootMultiGraph = new TMultiGraph();
	rootMultiGraph->SetName("multigraph");
	rootMultiGraph->SetTitle("Binder cumulant vs #beta;#beta;Binder cumulant");
	TLegend* rootMultiGraphLegend = new TLegend(0.1, 0.1, 0.2, 0.4);

	for (int i = 0; i < 5; i++)
	{
		LoadAndAddBinderCumulantDataToRootMultiGraph(filenames[i], rootMultiGraph, rootMultiGraphLegend, i);
	}

	rootMultiGraph->Draw("A");
	rootMultiGraphLegend->Draw();
	rootApp->Run();

	//delete rootApp, delete rootCanvas, delete rootMultiGraph, delete rootMultiGraphLegend
}

/**********************************************************************/

void IsingGPUHardcodedMultipleGridsAndAutoSaveRun()
{
	std::array<sIsingParameters, 1> aIsingParameters;
	std::array<const char*, 1> aOutputFilenames;
	aIsingParameters[0] =
	{
		.isingL = 20,
		.startBeta = 0.50,
		.endBeta = 0.35,
		.betaDecrement = 0.01,
		.numberOfSweepsPerTemperature = 10000,
		.numberOfSweepsToWaitBeforeSpinSumSamplingStarts = 100,
		.sweepsPerSpinSumSample = 2,
		.GPUOrCPUIdentifierText = "GPU"
	};
	aOutputFilenames[0] = "output0.txt";

	for (int i = 0; i < 1; i++)
	{
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

		int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((aIsingParameters[i].startBeta - aIsingParameters[i].endBeta) / aIsingParameters[i].betaDecrement);
		std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
		std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

		try
		{
			cSetup TheSetup(aIsingParameters[i].isingL, aIsingParameters[i].numberOfSweepsPerTemperature, aIsingParameters[i].numberOfSweepsToWaitBeforeSpinSumSamplingStarts,
				aIsingParameters[i].sweepsPerSpinSumSample, COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN);

			double beta = aIsingParameters[i].startBeta;
			for (uint32_t j = 0; j < numberOfDataPointsForTheBinderCumulantPlot; j++)
			{
				DoTheIsingGridSweepsGPU(&TheSetup, aIsingParameters[i].isingL, beta, aIsingParameters[i].numberOfSweepsPerTemperature,
					aIsingParameters[i].numberOfSweepsToWaitBeforeSpinSumSamplingStarts, aIsingParameters[i].sweepsPerSpinSumSample);

				betaValues[j] = beta;
				binderCumulants[j] = CalculateBinderCumulantGPU(&TheSetup, aIsingParameters[i].isingL);

				beta -= aIsingParameters[i].betaDecrement;
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what() << '\n';
		}

		std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
		std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

		SaveBinderCumulantData(aOutputFilenames[i], aIsingParameters[i], computationTime.count(), betaValues, binderCumulants);
	}
}

/**********************************************************************/

void IsingCPUHardcodedMultipleGridsAndAutoSaveRun()
{
	std::array<sIsingParameters, 1> aIsingParameters;
	std::array<const char*, 1> aOutputFilenames;
	aIsingParameters[0] =
	{
		.isingL = 20,
		.startBeta = 0.50,
		.endBeta = 0.35,
		.betaDecrement = 0.01,
		.numberOfSweepsPerTemperature = 10000,
		.numberOfSweepsToWaitBeforeSpinSumSamplingStarts = 100,
		.sweepsPerSpinSumSample = 2,
		.GPUOrCPUIdentifierText = "CPU"
	};
	aOutputFilenames[0] = "output0.txt";

	for (int i = 0; i < 1; i++)
	{
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint1 = std::chrono::steady_clock::now();

		int numberOfDataPointsForTheBinderCumulantPlot = (int)std::floor((aIsingParameters[i].startBeta - aIsingParameters[i].endBeta) / aIsingParameters[i].betaDecrement);
		std::vector<double> binderCumulants(numberOfDataPointsForTheBinderCumulantPlot);
		std::vector<double> betaValues(numberOfDataPointsForTheBinderCumulantPlot);

		// Set up the Ising grid on the CPU
		const uint32_t isingN = aIsingParameters[i].isingL * aIsingParameters[i].isingL;
		const uint32_t numberOfSpinBatches = (uint32_t)std::ceil(isingN / 32.0);
		const uint32_t numberOfElementsInTheSpinSumOutputArray = (aIsingParameters[i].numberOfSweepsPerTemperature - aIsingParameters[i].numberOfSweepsToWaitBeforeSpinSumSamplingStarts - 1)
			/ aIsingParameters[i].sweepsPerSpinSumSample;																// Integer division
		uint32_t* pArraySpinBatches = new uint32_t[numberOfSpinBatches];
		int* pArraySpinSumOutputs = new int[numberOfElementsInTheSpinSumOutputArray];
		int TheSpinSum = isingN;
		for (uint32_t j = 0; j < numberOfSpinBatches; j++)
		{
			pArraySpinBatches[j] = ~0U;																					// All spins are +1
		}

		// Do the computation
		double beta = aIsingParameters[i].startBeta;
		for (uint32_t j = 0; j < numberOfDataPointsForTheBinderCumulantPlot; j++)
		{
			DoTheIsingGridSweepsCPU(pArraySpinBatches, pArraySpinSumOutputs, TheSpinSum, aIsingParameters[i].isingL, beta,
				aIsingParameters[i].numberOfSweepsPerTemperature, aIsingParameters[i].numberOfSweepsToWaitBeforeSpinSumSamplingStarts, aIsingParameters[i].sweepsPerSpinSumSample);

			betaValues[j] = beta;
			binderCumulants[j] = CalculateBinderCumulantCPU(pArraySpinSumOutputs, aIsingParameters[i].isingL, numberOfElementsInTheSpinSumOutputArray);

			beta -= aIsingParameters[i].betaDecrement;
		}
		// ------------------
		std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> timePoint2 = std::chrono::steady_clock::now();
		std::chrono::duration<double> computationTime = timePoint2 - timePoint1;

		SaveBinderCumulantData(aOutputFilenames[i], aIsingParameters[i], computationTime.count(), betaValues, binderCumulants);

		delete[] pArraySpinBatches;
		delete[] pArraySpinSumOutputs;
	}
}

/**********************************************************************/

void SaveBinderCumulantData(const char* filename, sIsingParameters isingParameters, double computationTime, std::vector<double>& betaValues, std::vector<double>& binderCumulants)
{
	std::ofstream outputFileStream(filename, std::ios_base::out);
	if (!outputFileStream.is_open())
	{
		std::cout << "Failed to write to file.\n";
		return;
	}
	outputFileStream << "---Ising parameters---\n";
	outputFileStream << "Grid length: " << isingParameters.isingL << "\nStart beta: " << isingParameters.startBeta << "\nEnd beta: " << isingParameters.endBeta
		<< "\nBeta decrement: " << isingParameters.betaDecrement << "\nNumber of sweeps per temperature: " << isingParameters.numberOfSweepsPerTemperature
		<< "\nNumber of sweeps to wait for every temperature before spin sum sampling starts: " << isingParameters.numberOfSweepsToWaitBeforeSpinSumSamplingStarts
		<< "\nSweeps per spin sum sample after the wait: " << isingParameters.sweepsPerSpinSumSample << "\nRan on: " << isingParameters.GPUOrCPUIdentifierText
		<< "\nCOMPUTATION TIME (seconds): " << computationTime << "\n\n";
	outputFileStream << "Beta;Binder Cumulant\n";

	for (uint32_t i = 0; i < betaValues.size(); i++)
	{
		outputFileStream << betaValues[i] << ';' << binderCumulants[i] << '\n';
	}

	outputFileStream.close();
}

/**********************************************************************/

void LoadAndAddBinderCumulantDataToRootMultiGraph(const char* filename, TMultiGraph* rootMultiGraph, TLegend* rootMultiGraphLegend, int numberUsedToSetGraphMarkerStyleAndColor)
{
	assert(numberUsedToSetGraphMarkerStyleAndColor < 5);
	int colors[] = { kBlack, kRed, kGreen, kBlue, kOrange };
	char inputFileBuffer[100];
	std::string legendText = "L: ";
	std::vector<double> betaValues;
	std::vector<double> binderCumulants;

	std::ifstream inputFileStream(filename, std::ios_base::in);
	if (!inputFileStream.is_open())
	{
		std::cout << "Failed to open file.\n";
		return;
	}

	inputFileStream.ignore(1000, '\n');
	inputFileStream.ignore(1000, ' ');
	inputFileStream.ignore(1000, ' ');
	inputFileStream.getline(inputFileBuffer, 100, '\n');							// Get the grid length
	legendText.append(inputFileBuffer);

	// Skip to the data
	for (int i = 0; i < 10; i++)
	{
		inputFileStream.ignore(1000, '\n');
	}

	while (!inputFileStream.eof())
	{
		// Get the beta value
		inputFileStream.getline(inputFileBuffer, 100, ';');
		if (inputFileStream.eof()) break;
		betaValues.push_back(std::atof(inputFileBuffer));
		// Get the binder cumulant
		inputFileStream.getline(inputFileBuffer, 100, '\n');
		binderCumulants.push_back(std::atof(inputFileBuffer));
	}

	inputFileStream.close();
	kRed;
	TGraph* rootBinderCumulantGraph = new TGraph(betaValues.size(), betaValues.data(), binderCumulants.data());
	rootBinderCumulantGraph->SetMarkerStyle(21 + numberUsedToSetGraphMarkerStyleAndColor);
	rootBinderCumulantGraph->SetMarkerSize(2.0f);
	rootBinderCumulantGraph->SetMarkerColor(colors[numberUsedToSetGraphMarkerStyleAndColor]);
	rootBinderCumulantGraph->SetLineColor(colors[numberUsedToSetGraphMarkerStyleAndColor]);
	rootMultiGraph->Add(rootBinderCumulantGraph, "PL");
	rootMultiGraphLegend->AddEntry(rootBinderCumulantGraph, legendText.c_str());
}
