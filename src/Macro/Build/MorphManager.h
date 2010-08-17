#pragma once
#include <Arbitrator.h>
#include <BWAPI.h>
#include "CSingleton.h"
class MorphManager : public Arbitrator::Controller<BWAPI::Unit*,double>, public CSingleton<MorphManager>
{
	friend class CSingleton<MorphManager>;
  public:
    class Unit
    {
      public:
        BWAPI::UnitType type;
        bool started;
    };
	void setDependencies(Arbitrator::Arbitrator<BWAPI::Unit*,double>* arb);
    virtual void onOffer(std::set<BWAPI::Unit*> units);
    virtual void onRevoke(BWAPI::Unit* unit, double bid);
    virtual void update();
    virtual std::string getName() const;
    virtual std::string getShortName() const;

    void onRemoveUnit(BWAPI::Unit* unit);
    bool morph(BWAPI::UnitType type);
    int getPlannedCount(BWAPI::UnitType type) const;
    int getStartedCount(BWAPI::UnitType type) const;
    BWAPI::UnitType getBuildType(BWAPI::Unit* unit) const;

  private:
	      MorphManager();
    bool canMake(BWAPI::Unit* builder, BWAPI::UnitType type);
    Arbitrator::Arbitrator<BWAPI::Unit*,double>* arbitrator;
    std::map<BWAPI::UnitType,std::list<BWAPI::UnitType> > morphQueues;
    std::map<BWAPI::Unit*,Unit> morphingUnits;
    std::map<BWAPI::UnitType, int> plannedCount;
    std::map<BWAPI::UnitType, int> startedCount;
};