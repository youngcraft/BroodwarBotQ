#pragma once
#include "CSingleton.h"
#include <BWTA.h>
#include <BWAPI.h>
#include "BaseManager.h"
#include "TimeManager.h"
#include "MapManager.h"
#include <map>
#include <set>
#include <list>
#include <vector>


class UnitData
{
public:
    Unit* unit;
    UnitType unitType;
    Position position;
    int lastSeen;
    UnitData(Unit* unit);
    bool operator == (const UnitData& ud) const;
};

class RegionData
{
public:
    // TODO perhaps change vectors by sets (we need a quick find and don't care about the order)
    std::map<Player*, std::vector<UnitData> > buildings;	// list of enemy building seen in this region for each player.
    std::map<Player*, std::vector<UnitData> > units;      // list of enemy units seen in this region for each player.
    int lastSeen; // Last seen frame.

	RegionData();
	bool isOccupied() const;
	bool contain(Unit* unit) const;
    inline void add(Unit* unit);
};

class Regions : public CSingleton<Regions> 
{
	friend class CSingleton<Regions>;

private:
	Regions();
	~Regions();
    MapManager* mapManager;
    inline BWTA::Region* findRegion(BWAPI::Position p);

public:
    inline void addUnit(BWAPI::Unit* unit); // Add to the corresponding map (building/unit) in regionData. Refresh it if already present.
    void removeUnits();
    void addUnits();
    inline void removeUnit( Unit* unit);
	virtual void update();
	virtual std::string getName() const;
    void onUnitCreate(BWAPI::Unit* unit);
    void onUnitDestroy(BWAPI::Unit* unit);
    void onUnitShow(BWAPI::Unit* unit);
    void onUnitHide(BWAPI::Unit* unit);
	void display() const;

    std::map<BWTA::Region*, RegionData> regionsData;
	TimeManager* timeManager;
};