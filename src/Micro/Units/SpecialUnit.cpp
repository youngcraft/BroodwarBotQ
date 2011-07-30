#include <PrecompiledHeader.h>
#include "SpecialUnit.h"

ProbTables SpecialUnit::_sProbTables = ProbTables(-3); // -3 for special

SpecialUnit::SpecialUnit(BWAPI::Unit* u)
: BayesianUnit(u, &_sProbTables)
{
}

SpecialUnit::~SpecialUnit()
{
}

int SpecialUnit::getAttackDuration()
{
    return 1; // TOCHANGE TODO
}