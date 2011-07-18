#include <PrecompiledHeader.h>
#include "Macro/BasesManager.h"
#include "Macro/BorderManager.h"

using namespace BWAPI;

BasesManager* TheBasesManager = NULL;

BasesManager* BasesManager::create()
{
	if (TheBasesManager)
		return TheBasesManager;
	TheBasesManager = new BasesManager();
	return TheBasesManager;
}

void BasesManager::destroy()
{
	if (TheBasesManager)
		delete TheBasesManager;
}

BasesManager::BasesManager()
{
	TheBasesManager = this;
}

BasesManager::~BasesManager()
{
	TheBasesManager = NULL;
}

void BasesManager::update()
{
	for each(Base mb in allBases)
	{
		mb.update();
		if (mb.isActive())
			activeBases.insert(&mb);
		else
			activeBases.erase(&mb);
		if (mb.isReady())
			readyBases.insert(&mb);
		else
			readyBases.erase(&mb);
	}
}

Base* BasesManager::getBase(BWTA::BaseLocation* location)
{
	std::map<BWTA::BaseLocation*, Base*>::iterator i = location2base.find(location);
	if (i == location2base.end())
		return NULL;
	return i->second;
}

void BasesManager::expand(BWTA::BaseLocation* location)
{
	if (location == NULL)
	{
		// Find closer expand location not taken
		BWTA::BaseLocation* home = BWTA::getStartLocation(BWAPI::Broodwar->self());
		double minDist = -1;
		for(std::set<BWTA::BaseLocation*>::const_iterator i = BWTA::getBaseLocations().begin();
			i != BWTA::getBaseLocations().end(); i++)
		{
			double dist = home->getGroundDistance(*i);
			if (dist > 0 && getBase(*i) == NULL)
			{
				if (minDist < 0 || dist < minDist)
				{
					minDist = dist;
					location = *i;
				}
			}
		}
	}
#ifdef __DEBUG__
	if (location == NULL)
		Broodwar->printf("CANNOT EXPAND");
#endif

	allBases.push_back(Base(location));
	location2base[location] = & allBases.back();
	TheBorderManager->addMyBase(location);
}

const std::set<Base*>& BasesManager::getActiveBases() const
{
	return activeBases;
}

const std::set<Base*>& BasesManager::getReadyBases() const
{
	return readyBases;
}

const std::list<Base>& BasesManager::getAllBases() const
{
	return allBases;
}

const std::set<Base*>& BasesManager::getDestroyedBases() const
{
	return destroyedBases;
}

std::string BasesManager::getName()
{
	return "BasesManager";
}

void BasesManager::onUnitDestroy(BWAPI::Unit* unit)
{
	for each(Base b in allBases)
	{
		b.onUnitDestroy(unit);
	}
}

