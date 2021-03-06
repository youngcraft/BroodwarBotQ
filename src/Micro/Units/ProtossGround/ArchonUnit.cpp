#include <PrecompiledHeader.h>
#include "Micro/Units/ProtossGround/ArchonUnit.h"
#include "Micro/UnitsGroup.h"

using namespace BWAPI;

std::set<UnitType> ArchonUnit::setPrio;

ArchonUnit::ArchonUnit(Unit* u)
: GroundUnit(u)
{
    if (setPrio.empty())
    {
        setPrio.insert(UnitTypes::Zerg_Mutalisk);
        setPrio.insert(UnitTypes::Zerg_Zergling);
        setPrio.insert(UnitTypes::Protoss_High_Templar);
        setPrio.insert(UnitTypes::Protoss_Zealot);
        setPrio.insert(UnitTypes::Terran_Firebat);
        setPrio.insert(UnitTypes::Terran_Marine);
        setPrio.insert(UnitTypes::Terran_Medic);
        setPrio.insert(UnitTypes::Terran_Ghost);
        setPrio.insert(UnitTypes::Terran_Wraith);
    }
}

ArchonUnit::~ArchonUnit()
{
}

bool ArchonUnit::decideToFlee()
{
    if (targetEnemy && targetEnemy->exists() && targetEnemy->isVisible() 
        && Broodwar->getGroundHeight(TilePosition(targetEnemy->getPosition())) > Broodwar->getGroundHeight(TilePosition(_unitPos)))
    {
        if (_unitsGroup && _unitsGroup->nearestChoke && _unitsGroup->nearestChoke->getCenter().getDistance(_unitPos) < 128)
        {
            _fleeing = false;
            return false;
        }
    }
    if (unit->getShields() < 10)
        _fleeingDmg = 12;
    // TODO complete conditions
    int diff = _lastTotalHP - (unit->getShields() + unit->getHitPoints());
    _HPLosts.push_back(diff);
    _sumLostHP += diff;
    if (_HPLosts.size() > 24)
    {
        _sumLostHP -= _HPLosts.front();
        _HPLosts.pop_front();
    }
    if (_sumLostHP > _fleeingDmg)
        _fleeing = true;
    else
        _fleeing = false;
    if (!_fleeing)
    {
        int incDmg = 0;
        for (std::set<Unit*>::const_iterator it = _targetingMe.begin();
            it != _targetingMe.end(); ++it)
        {
            if ((*it)->getDistance(_unitPos) <= (*it)->getType().groundWeapon().maxRange() + 32)
                incDmg += (*it)->getType().groundWeapon().damageAmount() * (*it)->getType().maxGroundHits();
        }
        if (incDmg + _sumLostHP > _fleeingDmg)
            _fleeing = true;
    }
    return _fleeing;
}

void ArchonUnit::micro()
{
    int currentFrame = Broodwar->getFrameCount();
    updateTargetingMe();
    decideToFlee();
    if (currentFrame - _lastAttackFrame <= getAttackDuration()) // not interrupting attacks
        return;
    if (currentFrame - _lastAttackFrame == getAttackDuration() + 1)
        clearDamages();
    /// Dodge storm, drag mine, drag scarab
    if (dodgeStorm() || dragMine() || dragScarab()) 
        return;
    updateRangeEnemies();
    updateTargetEnemy();
    if (unit->getGroundWeaponCooldown() <= Broodwar->getLatencyFrames() + 1)
    {
        if (!inRange(targetEnemy))
        {
            clearDamages();
        }
        attackEnemyUnit(targetEnemy);
    }
    else if (unit->getGroundWeaponCooldown() > Broodwar->getLatencyFrames() + 2
		|| unit->getGroundWeaponCooldown() == unit->getType().groundWeapon().damageCooldown()) // against really laggy games TODO in other units 
    {
        if (_fleeing)
        {
#ifdef __SIMPLE_FLEE__
            simpleFlee();
#else
			if (_targetingMe.size() > 3) /// HACK TODO remove/change (unit->isStuck()?)
				simpleFlee();
			else
				flee();
#endif
        }
        else
        {
            fightMove();
        }
    }
}

void ArchonUnit::check()
{
}

int ArchonUnit::getAttackDuration()
{
    return 3;
}

std::set<UnitType> ArchonUnit::getSetPrio()
{
    return ArchonUnit::setPrio;
}