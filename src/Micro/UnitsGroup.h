#pragma once
#include <BWAPI.h>
#include <BWTA.h>
#include <windows.h>
#include <math.h>
#include "Micro/Units/BayesianUnit.h"
#include "Micro/Formations.h"
#include "Utils/Vec.h"
#include <set>
#include <boost/bimap/bimap.hpp>
#include "Intelligence/EUnitsFilter.h"

#define _UNITS_DEBUG 1

struct Dmg
{
    Dmg(int d, BWAPI::Unit* u, int h) 
        : dmg(d)
        , unit(u)
        , hp(h)
    { }
    Dmg(const Dmg& d)
        : dmg(d.dmg)
        , unit(d.unit)
        , hp(d.hp)
    { }
    int dmg;
    int hp;
    BWAPI::Unit* unit;
    bool operator<(const Dmg& d) const { return ((this->hp - this->dmg) < (d.hp - d.dmg) || (this->hp - this->dmg == d.hp - d.dmg) && this->unit < d.unit); }
    Dmg operator-=(int i) { this->dmg -= i; return *this; }
    Dmg operator+=(int i) { this->dmg += i; return *this; }
};

typedef boost::bimaps::bimap<BWAPI::Unit*, Dmg> UnitDmgBimap;
typedef UnitDmgBimap::relation UnitDmg;

class Formation;

struct i_dist 
{
	unsigned int ind;
	double dist;
	i_dist(unsigned int i, double d): ind(i), dist(d) {}
	//bool operator<(i_dist& ext) { return (ext.dist < dist); }
};

struct BasicUnitsGroup // should derive from vector<pBayesianUnit>
{
	std::vector<pBayesianUnit> units;
	virtual void update();
    bool removeUnit(BWAPI::Unit* u);
	bool emptyUnits();
    int size() const;
};

class UnitsGroup : public BasicUnitsGroup
{
private:
	int _totalHP;
    int _totalMinPrice;
    int _totalGazPrice;
    int _totalSupply;
	unit_mode _groupMode;
    EUnitsFilter* _eUnitsFilter;
    std::set<BWAPI::Unit*> _mergersHT;
    bool _hasDetection;
    inline void updateNearbyEnemyUnitsFromFilter(BWAPI::Position p, double radius = 400.0);
    double evaluateForces();
	inline void activeUnit(pBayesianUnit bu);
    bool removeArrivingUnit(BWAPI::Unit* u);
#ifndef __RELEASE_OPTIM__
    void displayTargets();  // debug purpose
#endif
public:
	/// Units tools / units intercommunication
    std::list<pBayesianUnit> arrivingUnits;
    std::vector<BWAPI::Position> ppath;
    UnitDmgBimap unitDamages;
    pBayesianUnit leadingUnit;
    BWAPI::Unit* defaultTargetEnemy;
    BWTA::Chokepoint* nearestChoke;
    double distToNearestChoke;
    Vec centerSpeed;
	std::map<BWAPI::UnitSizeType, int> sizes;
	/// Group recap variables
    BWAPI::Position center;
    int groupAltitude;
    double stdDevRadius, maxRadius;
	/// Enemies info
    BWAPI::Position enemiesCenter;
    int enemiesAltitude;
    std::map<BWAPI::Unit*, BWAPI::Position> enemies;
	
	/// Basic
	UnitsGroup();
	virtual ~UnitsGroup();
	void update();
#ifndef __RELEASE_OPTIM__
	void display();
#endif
	/// Group interaction/orders
	void attack(int x, int y);
	void attack(BWAPI::Position& p);
	void move(BWAPI::Position& p);
	virtual void formation(pFormation f);
	void switchMode(unit_mode um);
	void idle();
	/// Units pool management
	void dispatchCompleteUnit(pBayesianUnit bu);
    void giveUpControl(BWAPI::Unit* u);
    /// AIModule interface
    virtual void onUnitDestroy(BWAPI::Unit* u);
    virtual void onUnitShow(BWAPI::Unit* u);
    virtual void onUnitHide(BWAPI::Unit* u);
	/// Updaters of info
	inline void updateCenter();
	inline void updateGroupStrengh(BWAPI::Unit* u);
	/// Getters
    virtual BWAPI::Position getCenter() const;
    inline double getDistance(BWAPI::Unit* u) const;
	double getDistance(BWAPI::Position p) const;
    int getTotalHP() const;
    std::vector<pBayesianUnit>* getUnits(); // a baaad getter, keep an eye
	const BayesianUnit& operator[](ptrdiff_t i);
	/// Templas merging
    inline void templarMergingStuff();
    void signalMerge(BWAPI::Unit* u);
#ifndef __RELEASE_OPTIM__
    void selectedUnits(std::set<pBayesianUnit>& u);
#endif
};

