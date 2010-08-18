#pragma once
#include "MapManager.h"
#include "Vec.h"
#include "UnitGroupManager.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

using namespace BWAPI;

MapManager::MapManager()
: _pix_width(Broodwar->mapWidth() * 32)
, _pix_height(Broodwar->mapHeight() * 32)
, _width(Broodwar->mapWidth() * 4)
, _height(Broodwar->mapHeight() * 4)
{
    walkability = new bool[_width * _height];             // Walk Tiles resolution
    buildings_wt = new bool[_width * _height];
    buildings_wt_strict = new bool[_width * _height];
    lowResWalkability = new bool[Broodwar->mapWidth() * Broodwar->mapHeight()]; // Build Tiles resolution
    buildings = new bool[Broodwar->mapWidth() * Broodwar->mapHeight()];         // [_width * _height / 16];
    groundDamages = new int[Broodwar->mapWidth() * Broodwar->mapHeight()];
    airDamages = new int[Broodwar->mapWidth() * Broodwar->mapHeight()];

    // initialization
    for (int x = 0; x < _width; ++x) 
        for (int y = 0; y < _height; ++y) 
        {
            walkability[x + y*_width] = Broodwar->isWalkable(x, y);
            buildings_wt[x + y*_width] = false;
            buildings_wt_strict[x + y*_width] = false;
        }
    for (int x = 0; x < _width/4; ++x) 
    {
        for (int y = 0; y < _height/4; ++y) 
        {
            int xx = 4*x;
            int yy = 4*y;
            bool walkable = true;
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j) 
                {
                    if (!walkability[xx+i + (yy+j)*_width])
                        walkable = false;
                }
            }
            lowResWalkability[x + y*_width/4] = walkable;
            buildings[x + y*_width/4] = false; // initialized with manual call to onUnitCreate() in onStart()
            groundDamages[x + y*_width/4] = 0;
            airDamages[x + y*_width/4] = 0;
        }
    }
    _eUnitsFilter = & EUnitsFilter::Instance();
}

MapManager::~MapManager()
{
#ifdef __DEBUG_GABRIEL__
    Broodwar->printf("MapManager destructor");
#endif
    delete [] walkability;
    delete [] lowResWalkability;
    delete [] buildings_wt;
    delete [] buildings;
    delete [] groundDamages;
    delete [] airDamages;
}

void MapManager::modifyBuildings(Unit* u, bool b)
{
    // TODO Optimize (3 loops are unecessary)
    if (!u->getType().isBuilding() 
        || (u->isLifted() && b)) // lifted building won't be added (b==true)
        return;
    TilePosition tpBd = u->getTilePosition(); // top left corner of the building
    for (int x = tpBd.x(); x < tpBd.x() + u->getType().tileWidth(); ++x)
        for (int y = tpBd.y(); y < tpBd.y() + u->getType().tileHeight(); ++y)
        {
            buildings[x + y*Broodwar->mapWidth()] = b;
        }
    for (int x = tpBd.x()*4 - 1; x < tpBd.x()*4 + u->getType().tileWidth()*4 + 1; ++x)
        for (int y = tpBd.y()*4 - 1; y < tpBd.y()*4 + u->getType().tileHeight()*4 + 1; ++y)
            if (x >= 0 && x < _width && y >= 0 && y < _height)
                buildings_wt[x + y*_width] = b;
    for (int x = (u->getPosition().x() - u->getType().dimensionLeft() - 5) / 8; 
        x <= (u->getPosition().x() + u->getType().dimensionRight() + 5) / 8; ++x) // x += 8
    {
        for (int y = (u->getPosition().y() - u->getType().dimensionUp() - 5) / 8;
            y <= (u->getPosition().y() + u->getType().dimensionDown() + 5) / 8; ++y) // y += 8
        {
            buildings_wt_strict[x + y*_width] = b;
            //buildings_wt[x + y*_width] = b;
            //if (y > 0) buildings_wt[x + (y-1)*_width] = b;
        }
    }
}

void MapManager::addBuilding(Unit* u)
{
    //Broodwar->printf("%x %s \n", u, u->getType().getName().c_str());
    modifyBuildings(u, true);
}

void MapManager::removeBuilding(Unit* u)
{
    modifyBuildings(u, false);
}

void MapManager::modifyDamages(int* tab, Position p, int minRadius, int maxRadius, int damages)
{
    // TODO optimize
    int tmpMaxRadius = maxRadius + 32;
    //Broodwar->printf("modify minRadius: %d, maxRadius %d, Position(%d, %d)", minRadius, maxRadius, p.x(), p.y());
    int lowerX = (p.x() - tmpMaxRadius) > 0 ? p.x() - tmpMaxRadius : 0;
    int higherX = (p.x() + tmpMaxRadius) < _width ? p.x() + tmpMaxRadius : _pix_width;
    int lowerY = (p.y() - tmpMaxRadius) > 0 ? p.y() - tmpMaxRadius : 0;
    int higherY = (p.y() + tmpMaxRadius) < _height ? p.y() + tmpMaxRadius : _pix_height;
    assert(higherX > lowerX);
    assert(higherY > lowerY);
    for (int x = lowerX; x <= higherX; x += 32)
        for (int y = lowerY; y <= higherY; y += 32)
        {
            double dist = p.getDistance(Position(x, y));
            if (dist <= tmpMaxRadius && dist > minRadius)
            {
                tab[x/32 + y/32*Broodwar->mapWidth()] += damages;
            }                
        }
}

void MapManager::removeDmg(UnitType ut, Position p)
{
    if (p.x() == 0 && p.y() == 0)
        Broodwar->printf("0,0 : %s", ut.getName().c_str());
    if (ut.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        modifyDamages(this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange(), 
            - ut.groundWeapon().damageAmount() * ut.maxGroundHits());
    }
    if (ut.airWeapon() != BWAPI::WeaponTypes::None)
    {
        modifyDamages(this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange(), 
            - ut.airWeapon().damageAmount() * ut.maxAirHits());
    }
}

void MapManager::addDmg(UnitType ut, Position p)
{
    if (p.x() == 0 && p.y() == 0)
        Broodwar->printf("0,0 : %s", ut.getName().c_str());
    if (ut.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        modifyDamages(this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange(), 
            ut.groundWeapon().damageAmount() * ut.maxGroundHits());
    }
    if (ut.airWeapon() != BWAPI::WeaponTypes::None)
    {
        modifyDamages(this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange(), 
            ut.airWeapon().damageAmount() * ut.maxAirHits());
    }
}

void MapManager::onUnitCreate(Unit* u)
{
    addBuilding(u);
}

void MapManager::onUnitDestroy(Unit* u)
{
    removeBuilding(u);
    removeDmg(u->getType(), u->getPosition());
}

void MapManager::onUnitShow(Unit* u)
{
    addBuilding(u);
}

void MapManager::onUnitHide(Unit* u)
{
    addBuilding(u);
}

void MapManager::onFrame()
{
    // check/update the damage maps. BEWARE: hidden units are not removed in presence of doubt!
    std::map<BWAPI::Unit*, EViewedUnit> tmp = _eUnitsFilter->getViewedUnits();
    //std::map<BWAPI::Unit*, EViewedUnit> tmp = _eUnitsFilter->_eViewedUnits;
    for (std::map<BWAPI::Unit*, EViewedUnit>::const_iterator it = tmp.begin();
        it != tmp.end(); ++it)
    {
        if (it->first->isVisible() 
            && it->first->exists()
            && _trackedUnits[it->first] != it->first->getPosition())
        {
            // update EUnitsFilter
            _eUnitsFilter->update(it->first);
            // update the map
            removeDmg(it->first->getType(), _trackedUnits[it->first]);
            addDmg(it->first->getType(), it->first->getPosition());
            _trackedUnits[it->first] = it->first->getPosition();
        }
    }

    this->drawGroundDamages();
}

void MapManager::drawBuildings()
{
    for (int x = 0; x < _width; ++x)
        for (int y = 0; y < _height; ++y)
        {
            if (buildings_wt[x + y*_width])
                Broodwar->drawBoxMap(8*x+1, 8*y+1, 8*x+7, 8*y+7, Colors::Orange);
        }
}

void MapManager::drawBuildingsStrict()
{
    for (int x = 0; x < _width; ++x)
        for (int y = 0; y < _height; ++y)
        {
            if (buildings_wt_strict[x + y*_width])
                Broodwar->drawBoxMap(8*x+1, 8*y+1, 8*x+7, 8*y+7, Colors::Orange);
        }
}

void MapManager::drawWalkability()
{
    for (int x = 0; x < _width; ++x)
        for (int y = 0; y < _height; ++y)
        {
            if (!walkability[x + y*_width])
                Broodwar->drawBox(CoordinateType::Map, 8*x + 1, 8*y + 1, 8*x + 7, 8*y + 7, Colors::Red);
        }
}

void MapManager::drawLowResWalkability()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            if (!lowResWalkability[x + y*Broodwar->mapWidth()])
                Broodwar->drawBox(CoordinateType::Map, 32*x + 2, 32*y + 2, 32*x + 30, 32*y + 30, Colors::Red);
        }
}

void MapManager::drawLowResBuildings()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            if (buildings[x + y*Broodwar->mapWidth()])
                Broodwar->drawBox(CoordinateType::Map, 32*x + 2, 32*y + 2, 32*x + 30, 32*y + 30, Colors::Orange);
            //Broodwar->drawBox(CoordinateType::Map, 32*x - 15, 32*y - 15, 32*x + 15, 32*y + 15, Colors::Orange);
        }
}

void MapManager::drawGroundDamages()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            if (groundDamages[x + y*Broodwar->mapWidth()] > 0 && groundDamages[x + y*Broodwar->mapWidth()] <= 40)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::White);
            }
            else if (groundDamages[x + y*Broodwar->mapWidth()] > 40 && groundDamages[x + y*Broodwar->mapWidth()] <= 80)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Yellow);
            }
            else if (groundDamages[x + y*Broodwar->mapWidth()] > 80 && groundDamages[x + y*Broodwar->mapWidth()] <= 160)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Orange);
            }
            else if (groundDamages[x + y*Broodwar->mapWidth()] > 160)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Red);
            }
        }
}

void MapManager::drawAirDamages()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            if (airDamages[x + y*Broodwar->mapWidth()] > 0 && airDamages[x + y*Broodwar->mapWidth()] <= 40)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::White);
            }
            else if (airDamages[x + y*Broodwar->mapWidth()] > 40 && airDamages[x + y*Broodwar->mapWidth()] <= 80)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Yellow);
            }
            else if (airDamages[x + y*Broodwar->mapWidth()] > 80 && airDamages[x + y*Broodwar->mapWidth()] <= 160)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Orange);
            }
            else if (airDamages[x + y*Broodwar->mapWidth()] > 160)
            {
                Broodwar->drawBox(CoordinateType::Map, 32*x - 14, 32*y - 14, 32*x +14, 32*y +14, Colors::Red);
            }
        }
}