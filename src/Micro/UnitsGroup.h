#pragma once
#include <BWAPI.h>
#include <BWTA.h>
#include <windows.h>
#include <math.h>
#include "BayesianUnit.h"
#include "Goal.h"
#include "Formations.h"
#include <Vec.h>
#include <set>

#define _UNITS_DEBUG 1

//class Goal;
class Formation;

struct i_dist {
	unsigned int ind;
	double dist;
	i_dist(unsigned int i, double d): ind(i), dist(d) {}
	//bool operator<(i_dist& ext) { return (ext.dist < dist); }
};


class UnitsGroup
{
private:
	Goal* lastGoal; // the last goal is copied from the last executed goal in 'goals' and is keep to give order to new units added to a group (to avoid empty 'goals' for new units).
	int totalHP;
	int totalPower;
    BWAPI::Position center;
    std::vector<pBayesianUnit> units;

public:
	std::list<Goal*> goals; // list of goals to execute.

	UnitsGroup();
	~UnitsGroup();

	std::map<BWAPI::UnitSizeType, int> sizes;

	virtual void update();
	virtual void display();

	// Goals interface
	virtual void attackMove(int x, int y);
	virtual void attackMove(BWAPI::Position& p);
	virtual void formation(Formation* f);
	virtual void setGoals(std::list<Goal*>& goals);
	virtual void addGoal(Goal* goal);
	virtual const Goal* getLastGoal() const;
	//virtual bool checkInFormation();
	//virtual bool checkAtDestination();
	virtual void updateCenter();
    virtual BWAPI::Position getCenter() const;

	// Micro tech (to be placed in other classes. For instance DistantUnits...)
	void keepDistance();

	// Units interface
    virtual void onUnitDestroy(BWAPI::Unit* u);
    virtual void onUnitShow(BWAPI::Unit* u);
    virtual void onUnitHide(BWAPI::Unit* u);
    virtual void takeControl(BWAPI::Unit* u);
    virtual void giveUpControl(BWAPI::Unit* u);
	bool empty();
	unsigned int getNbUnits() const;
#ifdef _UNITS_DEBUG
    void selectedUnits(std::set<pBayesianUnit>& u);
#endif
	const BayesianUnit& operator[](int i);
};