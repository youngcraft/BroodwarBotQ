#include <PrecompiledHeader.h>
#include "Micro/Goals/GoalManager.h"
#include "Micro/Goals/RegroupGoal.h"
#include "Macro/BWSAL.h"
#include "Defines.h"

using namespace BWAPI;
using namespace std;

GoalManager::GoalManager()
: _firstPoke(false) // hack, remove TODO
, attackGoals(0)
{
}

GoalManager::~GoalManager()
{
	_goals.clear();
}

void GoalManager::update()
{
	/// Check if units in training are completed and create BayesianUnits if so
	for (list<Unit*>::const_iterator it = _inTrainingUnits.begin();
		it != _inTrainingUnits.end(); )
	{
		if ((*it)->isCompleted())
		{
			pBayesianUnit tmp = BayesianUnit::newBayesianUnit(*it);
			if (tmp.get() != NULL)
				_completedUnits.insert(make_pair<Unit*, pBayesianUnit>(*it, tmp));
			_inTrainingUnits.erase(it++);
		}
		else
			++it;
	}

	/// Do a regroup goal garbage collect units if nothing else to do, lowest bid
	/*if (_goals.empty() || _assignedUnits.size() < _completedUnits.size())
	{
        garbageCollector = pGoal(new RegroupGoal(BWTA::getStartLocation(Broodwar->self())->getPosition(), 1));
		_goal.push_back(garbageCollector);
	}*/

#ifdef __MICRO_DEBUG__
	int y = 3;
#endif
	/// Update all goals
	for each (pGoal g in _goals)
	{
		g->update();
#ifdef __MICRO_DEBUG__
		if (g->getStatus() == GS_WAIT_PRECONDITION)
			Broodwar->drawTextScreen(5, y, "\x17%s: %d", g->getName().c_str(), g);
		else if (g->getStatus() == GS_IN_PROGRESS)
			Broodwar->drawTextScreen(5, y, "\x07%s: %d", g->getName().c_str(), g);
		else
			Broodwar->drawTextScreen(5, y, "%s: %d", g->getName().c_str(), g);
		y += 8;
#endif
	}
	/// Removed finished goals
	for (list<pGoal>::const_iterator it = _goals.begin();
		it != _goals.end();)
	{
		if ((*it)->getStatus() == GS_ACHIEVED || (*it)->getStatus() == GS_CANCELED)
		{
#ifdef __MICRO_DEBUG__
			Broodwar->printf("\x08 !!! DELETED A %s GOAL !!!", (*it)->getName().c_str());
#endif
			_goals.erase(it++);
		}
		else
			++it;
	}
}

/***
 * Add the goal is not already present in _goals (O(#goals)), defensive thing
 */
void GoalManager::addGoal(pGoal g)
{
	for each (pGoal pg in _goals)
	{
		if (pg == g)
			return;
	}
	_goals.push_back(g);
}

void GoalManager::addGoals(const list<pGoal>& l)
{
	for each (pGoal g in l)
		addGoal(g);
}

/// Add all units not building
void GoalManager::onUnitCreate(Unit* u)
{
	if (u->getPlayer() != Broodwar->self())
		return;
	if (!u->getType().isBuilding()) // yup, even workers, we want to create BayesianUnits in case they have to fight
	{
		if (u->getType() == UnitTypes::Protoss_Scarab || u->getType() == UnitTypes::Protoss_Interceptor)
			return;
		_inTrainingUnits.push_back(u);
	}
}

/// Remove from _completedUnits or from _inTrainingUnits 
void GoalManager::onUnitDestroy(Unit* u)
{
	if (u->getPlayer() == Broodwar->self())
	{
		map<Unit*, pBayesianUnit>::iterator itUnit = _completedUnits.find(u);
		if (itUnit != _completedUnits.end())
			_completedUnits.erase(itUnit);
		_inTrainingUnits.remove(u);
	}
	for each (pGoal g in _goals)
		g->onUnitDestroy(u); // will delete the pBayesianUnit OR will remove enemy units from the bimap
}

const map<Unit*, pBayesianUnit>& GoalManager::getCompletedUnits() const
{
	return _completedUnits;
}

pBayesianUnit GoalManager::getCompletedUnit(Unit* u)
{
#ifdef __DEBUG__
	assert(_completedUnits.find(u) != _completedUnits.end());
#endif
	return _completedUnits[u];
}