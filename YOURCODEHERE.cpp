#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>
#include "431project.h"

using namespace std;

/*
 * Enter your PSU IDs here to select the appropriate scanning order.
 */
#define PSU_ID_SUM (931701543+907056774)

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */
unsigned int currentlyExploringDim = 0;
bool currentDimDone = false;
bool isDSEComplete = false;
int exploreDimOrder[NUM_DIMS - NUM_DIMS_DEPENDENT] = {11, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1, 12, 13, 14};
int start = 0;

/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	
	std::stringstream latencySettings;
	
	int dlone_lat;
	int ilone_lat;
	int ultwo_lat;

	int dlone_asso = extractConfigPararm(halfBackedConfig, 4);
	int ilone_asso = extractConfigPararm(halfBackedConfig, 6);
	int ultwo_asso = extractConfigPararm(halfBackedConfig, 9);

	unsigned int dlone = getdl1size(halfBackedConfig);
	unsigned int ilone = getil1size(halfBackedConfig);
	unsigned int ultwo = getl2size(halfBackedConfig);

	dlone_lat = (int)log2((dlone/1024))-1+dlone_asso;
	ilone_lat = (int)log2((ilone/1024))-1+ilone_asso;
	ultwo_lat = (int)log2((ultwo/1024))-5+ultwo_asso;
	
	latencySettings << dlone_lat << " " <<ilone_lat << " " << ultwo_lat;
	
	return latencySettings.str();
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {
	
	// ifq size in bytes
	unsigned int ifq = 8 *pow(2,extractConfigPararm(configuration, 0));
	// il1 and ul2 BLOCK size in bytes
	unsigned int ilone_bsize = pow(2, 3+extractConfigPararm(configuration, 2));
	unsigned int ultwo_bsize = pow(2, 4+extractConfigPararm(configuration, 8));
	// il1 and ul2 size in bytes
	unsigned int dlone = getdl1size(configuration);
	unsigned int ilone = getil1size(configuration);
	unsigned int ultwo = getl2size(configuration);
	
	// Conditional 1 in section 8.3
	
	if (ilone_bsize < ifq){
		return 0;
	}
	
	// Conditional 2 in section 8.3
	if (ultwo_bsize < ilone_bsize * 2 || ultwo_bsize > 128){
		return 0;
	}
	
	if (ultwo < 2 *(dlone + ilone)){
		return 0;
	}
	// Conditional 3 in section 8.3
	if (dlone < 2048 || dlone > 65536){
		return 0;
	}
	if (ilone < 2048 || ilone > 65536){
		return 0;
	}
	// Conditional 4 in section 8.3
	if (ultwo < (32 * 1024) || ultwo > (1024 * 1024)){
		return 0;
	}
	
	return 1;
}

/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations
	
	std::string nextconfiguration = currentconfiguration;
	
	while (!validateConfiguration(nextconfiguration) ||
		GLOB_seen_configurations[nextconfiguration]) {
		
		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
			return currentconfiguration;
		}

		std::stringstream ss;

		string bestConfig;
		if (optimizeforEXEC == 1)
			bestConfig = bestEXECconfiguration;

		if (optimizeforEDP == 1)
			bestConfig = bestEDPconfiguration;
		
		// Fill in the dimensions already-scanned with the already-selected best
		// value.
		for (int dim = 0; dim < exploreDimOrder[currentlyExploringDim]; ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}

		// Handling for currently exploring dimension. This is a very dumb
		// implementation.
		
		int nextValue;
		if (start == 0){
			nextValue = 0;
		}
		else{
			nextValue = extractConfigPararm(nextconfiguration,
				exploreDimOrder[currentlyExploringDim]) + 1;
		}
		
		
		if (nextValue >= GLOB_dimensioncardinality[exploreDimOrder[currentlyExploringDim]]) {
			nextValue = GLOB_dimensioncardinality[exploreDimOrder[currentlyExploringDim]] - 1;
			currentDimDone = true;
		}
		
		ss << nextValue << " ";

		// Fill in remaining independent params with 0.
		
		for (int dim = (exploreDimOrder[currentlyExploringDim] + 1);
				dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}
		
		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();
		start++;

		// Make sure we start exploring next dimension in next iteration.
		
		if (currentDimDone) {
			start = 0;
			currentlyExploringDim++;
			currentDimDone = false;
		}

		// Signal that DSE is complete after this configuration.
		
		if (currentlyExploringDim == (NUM_DIMS - NUM_DIMS_DEPENDENT))
			isDSEComplete = true;
		
	}
	
	return nextconfiguration;
}

