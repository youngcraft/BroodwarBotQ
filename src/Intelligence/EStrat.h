#pragma once
#include "EEcoEstimator.h"

class EStrat : public CSingleton<EStrat>
{
	friend class CSingleton<EEcoEstimator>;

public :
	int probability_of_rush();
	int probability_of_proxy();
	int probability_of_canonrush();
};