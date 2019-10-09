#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <math.h>
#include <time.h>
#include "Population.hpp"
#include "Config.hpp"

Population::Population(char* t_inputFilePath, char* t_outputFilePath) {
	m_inputFilePath = t_inputFilePath;
	m_outputFilePath = t_outputFilePath;
	m_speciesCount = 1;
	m_individuals = new Individual[PARAMS__POPULATION_SIZE];

#if LOG_INFO
	std::cout << "Population initiated." << std::endl;
#endif
}

Population::~Population() {
	delete[] m_individuals;
}

double* readVectorFromFile(int t_count, int t_size, const char* t_filePath) {
	double* result = new double[t_count * t_size];

	std::ifstream file;
	file.open(t_filePath, std::ios::in);
	if (file.is_open()) {
#if LOG_DEBUG
		std::cout << "Input file opened" << std::endl;
#endif
	}
	else {
#if LOG_ERROR
		std::cout << "ERROR: Couldn't open file" << std::endl;
#endif
		return nullptr;
	}
	std::string line;
	for (int i = 0; i < t_count; i++)
	{
		std::getline(file, line);
		std::istringstream stringStream(line);
		std::string item;
		for (int j = 0; j < t_size; j++)
		{
			std::getline(stringStream, item, ',');
			result[i * t_size + j] = std::stod(item);
		}
	}
	file.close();

#if LOG_DEBUG
	std::cout << "Input read from file successfuly" << std::endl;
#endif

	return result;
}

double* Population::trainGeneration(double* t_input, double* t_expectedOutput) {
	double* error = new double[PARAMS__POPULATION_SIZE];
	for (int i = 0; i < PARAMS__POPULATION_SIZE; i++)
	{
		double* output = m_individuals[i].getOutput(PARAMS__TRAINING_SIZE, t_input);

		// TODO: Do this in the GPU kernel along other tasks
		error[i] = 0;
		for (int j = 0; j < PARAMS__TRAINING_SIZE; j++)
		{
			for (int k = 0; k < SUBSTRATE__OUTPUT_SIZE; k++)
			{
				double diff = output[j * SUBSTRATE__OUTPUT_SIZE + k] -
					t_expectedOutput[j * SUBSTRATE__OUTPUT_SIZE + k];
				error[i] += diff * diff;
			}
		}

#if LOG_VERBOSE
		std::cout << "Result for individual " << i << ": ";
		for (int j = 0; j < fminl(PARAMS__TRAINING_SIZE, 10); j++)
		{
			for (int k = 0; k < fminl(SUBSTRATE__OUTPUT_SIZE, 3); k++)
			{

			}
			std::cout << "(";
			for (int k = 0; k < fminl(SUBSTRATE__OUTPUT_SIZE, 3); k++)
				std::cout << output[j * SUBSTRATE__OUTPUT_SIZE + k] << ", ";
			std::cout << (SUBSTRATE__OUTPUT_SIZE > 3 ? "..." : "") << ") ";
		}
		std::cout << (PARAMS__TRAINING_SIZE > 10 ? "..." : "") << std::endl;
#endif

		delete[] output;
	}

#if LOG_DEBUG
	for (int i = 0; i < PARAMS__POPULATION_SIZE; i++)
	{
		std::cout << "Error for individual " << i << " = " << error[i] << std::endl;
	}
#endif

	return error;
}

void Population::createNextGeneration(double* error) {
	//Load species and distribute eviction size in each species
	int* specieSizes = new int[m_speciesCount];
	for (int i = 0; i < m_speciesCount; i++)
		specieSizes[i] = 0;
	for (int i = 0; i < PARAMS__POPULATION_SIZE; i++)
		specieSizes[m_individuals[i].speciesId]++;

	int* evictionSizes = new int[m_speciesCount];
	for (int i = 0; i < m_speciesCount; i++)
		evictionSizes[i] = specieSizes[i] * PARAMS__EVICTION_SIZE / PARAMS__POPULATION_SIZE;

	//TODO: Sort individuals small to large
	//std::sort(m_individuals, &m_individuals[PARAMS__POPULATION_SIZE],
	//	[](Individual const& first, Individual const& second)->bool {
	//	return first
	//});

	//Evict from each species
	int evictionList[PARAMS__EVICTION_SIZE];
	int currentEvictionIndex = 0;
	for (int i = PARAMS__POPULATION_SIZE - 1; i >= 0; i--)
	{
		if (evictionSizes[m_individuals[i].speciesId] > 0) {
			evictionList[currentEvictionIndex] = i;
			currentEvictionIndex++;
			evictionSizes[m_individuals[i].speciesId]--;
			m_individuals[i].isAlive = false;
		}
	}

	//Create new generation from remaining indivs
	srand(time(0));
	for (int i = 0; i < PARAMS__EVICTION_SIZE; i++)
	{
		int p1Ind;
		do {
			p1Ind = rand() % PARAMS__POPULATION_SIZE;
		} while (!m_individuals[p1Ind].isAlive);

		int p2Ind;
		do {
			p2Ind = rand() % PARAMS__POPULATION_SIZE;
		} while (!m_individuals[p2Ind].isAlive);

		m_individuals[evictionList[i]].recreateAsChild(
			&m_individuals[p1Ind], &m_individuals[p2Ind]);

		//TODO:Assign offsprings to species
	}

	delete[] specieSizes;
	delete[] evictionSizes;
}

double* Population::getBestTestResult(double* t_errors, double* t_input) {
	double minError = INFINITY;
	int minErrorIndex = -1;
	for (int i = 0; i < PARAMS__POPULATION_SIZE; i++)
	{
		if (t_errors[i] < minError) {
			minError = t_errors[i];
			minErrorIndex = i;
		}
	}

	double* result = new double[PARAMS__TEST_SIZE * SUBSTRATE__OUTPUT_SIZE];
	for (int i = 0; i < PARAMS__TEST_SIZE; i++)
	{
		double* output = m_individuals[minErrorIndex].
			getOutput(PARAMS__TEST_SIZE, &t_input[PARAMS__TRAINING_SIZE]);
		for (int j = 0; j < SUBSTRATE__OUTPUT_SIZE; j++)
			result[i * SUBSTRATE__OUTPUT_SIZE + j] = output[j];

		delete[] output;
	}
	return result;
}

void Population::run() {
#if LOG_DEBUG
	if (SYSTEM__USE_GPU)
		std::cout << "Running. (GPU support ENABLED)" << std::endl;
	else
		std::cout << "Running. (GPU support DISABLED)" << std::endl;
#endif
	double* input = readVectorFromFile(
		PARAMS__TRAINING_SIZE + PARAMS__TEST_SIZE,
		SUBSTRATE__INPUT_SIZE,
		m_inputFilePath);
	double* expOutput = readVectorFromFile(
		PARAMS__TRAINING_SIZE + PARAMS__TEST_SIZE,
		SUBSTRATE__OUTPUT_SIZE,
		m_outputFilePath);

	for (int generation = 0; generation < PARAMS__TRAINING_GENERATIONS - 1; generation++)
	{
		double* error = trainGeneration(input, expOutput);
		createNextGeneration(error);
#if LOG_INFO
		std::cout << "Training generation " << generation << " done." << std::endl;
#if LOG_VERBOSE
		double minError = INFINITY;
		for (int i = 0; i < PARAMS__POPULATION_SIZE; i++)
			if (error[i] < minError)
				minError = error[i];
		std::cout << "Minimum error so far = " << minError << std::endl;
#endif
#endif
		delete[] error;
	}
	double* error = trainGeneration(input, expOutput);

	/*double* result = getBestTestResult(error, input);
	std::cout << "Results:" << std::endl;
	for (int i = 0; i < PARAMS__TEST_SIZE; i++) {
		for (int j = 0; j < SUBSTRATE__OUTPUT_SIZE; j++)
			std::cout << result[i * SUBSTRATE__OUTPUT_SIZE + j] << ", ";
		std::cout << std::endl;
	}

	delete[] result;*/
	delete[] error;
	delete[] input;
}