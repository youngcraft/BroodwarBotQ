#include <PrecompiledHeader.h>
#include "Defines.h"
#include "Regions/MapManager.h"
#include "Micro/Units/ProtossSpecial/HighTemplarUnit.h"

#define __MESH_SIZE__ 32 // 16 // 24 // 32 // 48
#define __STORM_SIZE__ 96

using namespace BWAPI;

std::map<BWAPI::Unit*, BWAPI::Position> HighTemplarUnit::stormableUnits;

MapManager::MapManager()
: _pix_width(Broodwar->mapWidth() * 32)
, _pix_height(Broodwar->mapHeight() * 32)
, _width(Broodwar->mapWidth() * 4)
, _height(Broodwar->mapHeight() * 4)
, _stormPosMutex(CreateMutex( 
        NULL,                  // default security attributes
        FALSE,                 // initially not owned
        NULL))                  // unnamed mutex
, _lastStormUpdateFrame(0)
, _eUnitsFilter(& EUnitsFilter::Instance())
{
#ifdef __DEBUG__
    if (_stormPosMutex == NULL) 
    {
        Broodwar->printf("CreateMutex error: %d\n", GetLastError());
        return;
    }
#endif
    walkability = new bool[_width * _height];             // Walk Tiles resolution
    buildings_wt = new bool[_width * _height];
    buildings_wt_strict = new bool[_width * _height];
    lowResWalkability = new bool[Broodwar->mapWidth() * Broodwar->mapHeight()]; // Build Tiles resolution
    buildings = new bool[Broodwar->mapWidth() * Broodwar->mapHeight()];         // [_width * _height / 16];
    groundDamages = new int[Broodwar->mapWidth() * Broodwar->mapHeight()];
    airDamages = new int[Broodwar->mapWidth() * Broodwar->mapHeight()];
    groundDamagesGrad = new Vec[Broodwar->mapWidth() * Broodwar->mapHeight()];
    airDamagesGrad = new Vec[Broodwar->mapWidth() * Broodwar->mapHeight()];

	/// Fill regionsPFCenters (regions pathfinding aware centers, 
	/// min of the sum of the distance to chokes on paths between/to chokes)
	const std::set<BWTA::Region*> allRegions = BWTA::getRegions();
	for (std::set<BWTA::Region*>::const_iterator it = allRegions.begin();
		it != allRegions.end(); ++it)
	{
		std::list<Position> chokesCenters;
		for (std::set<BWTA::Chokepoint*>::const_iterator it2 = (*it)->getChokepoints().begin();
			it2 != (*it)->getChokepoints().end(); ++it2)
			chokesCenters.push_back((*it2)->getCenter());
		if (chokesCenters.empty())
			regionsPFCenters.insert(std::make_pair<BWTA::Region*, Position>(*it, (*it)->getCenter()));
		else
		{
			std::list<TilePosition> validTilePositions;
			for (std::list<Position>::const_iterator c1 = chokesCenters.begin();
				c1 != chokesCenters.end(); ++c1)
			{
				for (std::list<Position>::const_iterator c2 = chokesCenters.begin();
					c2 != chokesCenters.end(); ++c2)
				{
					if (*c1 != *c2)
					{
						std::vector<TilePosition> buffer = BWTA::getShortestPath(TilePosition(*c1), TilePosition(*c2));
						for (std::vector<TilePosition>::const_iterator vp = buffer.begin();
							vp != buffer.end(); ++vp)
							validTilePositions.push_back(*vp);
					}
				}
			}
			double minDist = 1000000000000.0;
			TilePosition centerCandidate = TilePosition((*it)->getCenter());
			for (std::list<TilePosition>::const_iterator vp = validTilePositions.begin();
				vp != validTilePositions.end(); ++vp)
			{
				double tmp = 0.0;
				for (std::list<Position>::const_iterator c = chokesCenters.begin();
					c != chokesCenters.end(); ++c)
				{
					tmp += BWTA::getGroundDistance(TilePosition(*c), *vp);
				}
				if (tmp < minDist)
				{
					minDist = tmp;
					centerCandidate = *vp;
				}
			}
			regionsPFCenters.insert(std::make_pair<BWTA::Region*, Position>(*it, Position(centerCandidate)));
		}
	}

	/// Fill distRegions with the mean distance between each Regions
	for (std::set<BWTA::Region*>::const_iterator it = allRegions.begin();
		it != allRegions.end(); ++it)
	{
		distRegions.insert(std::pair<BWTA::Region*, std::map<BWTA::Region*, double> >(*it,
			std::map<BWTA::Region*, double>()));
		for (std::set<BWTA::Region*>::const_iterator it2 = allRegions.begin();
			it2 != allRegions.end(); ++it2)
		{
			distRegions[*it].insert(std::pair<BWTA::Region*, double>(*it2, 
				BWTA::getGroundDistance(TilePosition(regionsPFCenters[*it]),
				TilePosition(regionsPFCenters[*it2]))));
		}
	}

	/// search the centers of all regions
	for (std::set<BWTA::Region*>::const_iterator it = allRegions.begin();
		it != allRegions.end(); ++it)
	{
		TilePosition tmpRegionCenter = TilePosition((*it)->getCenter()); // region->getCenter() is bad (can be out of the Region)
		BWTA::Polygon polygon = (*it)->getPolygon();
		std::set<TilePosition> out;
		std::list<TilePosition>in;
		for (std::vector<Position>::const_iterator it2 = polygon.begin();
			it2 != polygon.end(); ++it2)
		{
			in.push_back(TilePosition(*it2));
		}
		while (!out.empty())
		{
			if (out.size() == 1)
			{
				tmpRegionCenter = *(out.begin());
				break;
			}
			else
			{
				std::list<TilePosition> newIn;
				for (std::list<TilePosition>::const_iterator it2 = in.begin();
					it2 != in.end(); ++it2)
				{
					out.erase(*it2);
					newIn.push_back(TilePosition(it2->x()+1, it2->y()));
					newIn.push_back(TilePosition(it2->x(), it2->y()+1));
					newIn.push_back(TilePosition(it2->x()-1, it2->y()));
					newIn.push_back(TilePosition(it2->x(), it2->y()-1));
				}
				in.swap(newIn);
			}
		}
		regionsInsideCenter.insert(std::make_pair<BWTA::Region*, TilePosition>(*it, tmpRegionCenter));
	}

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
            groundDamagesGrad[x + y*_width/4] = Vec(0, 0);
            airDamages[x + y*_width/4] = 0;
            airDamagesGrad[x + y*_width/4] = Vec(0, 0);
        }
    }
}

MapManager::~MapManager()
{
#ifdef __DEBUG__
    Broodwar->printf("MapManager destructor");
#endif
    delete [] walkability;
    delete [] lowResWalkability;
    delete [] buildings_wt;
    delete [] buildings_wt_strict;
    delete [] buildings;
    delete [] groundDamages;
    delete [] airDamages;
    delete [] groundDamagesGrad;
    delete [] airDamagesGrad;
    CloseHandle(_stormPosMutex);
}

void MapManager::modifyBuildings(Unit* u, bool b)
{
    // TODO Optimize (3 loops are not necessary)
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

void MapManager::addAlliedUnit(Unit* u)
{
    if (!(u->getType().isBuilding()) && !_ourUnits.count(u))
        _ourUnits.insert(std::make_pair<Unit*, Position>(u, u->getPosition()));
}

void MapManager::removeBuilding(Unit* u)
{
    modifyBuildings(u, false);
}

void MapManager::removeAlliedUnit(Unit* u)
{
    if (!(u->getType().isBuilding()))
        _ourUnits.erase(u);
}

void MapManager::modifyDamages(int* tab, Position p, int minRadius, int maxRadius, int damages)
{
    // TODO optimize
    int tmpMaxRadius = maxRadius; /// + 32; // TOCHANGE 32
    //Broodwar->printf("modify minRadius: %d, maxRadius %d, Position(%d, %d)", minRadius, maxRadius, p.x(), p.y());
    int lowerX = (p.x() - tmpMaxRadius) > 0 ? p.x() - tmpMaxRadius : 0;
    int higherX = (p.x() + tmpMaxRadius) < _pix_width ? p.x() + tmpMaxRadius : _pix_width;
    int lowerY = (p.y() - tmpMaxRadius) > 0 ? p.y() - tmpMaxRadius : 0;
    int higherY = (p.y() + tmpMaxRadius) < _pix_height ? p.y() + tmpMaxRadius : _pix_height;
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

void MapManager::updateDamagesGrad(Vec* grad, int* tab, Position p, int minRadius, int maxRadius)
{
    int tmpMaxRadius = maxRadius + 32 + 46; // 32 b/c it is a gradient, 46 for enemy unit movement
    int tmpMinRadius = max(0, minRadius - 32);
    int lowerX = (p.x() - tmpMaxRadius) > 0 ? p.x() - tmpMaxRadius : 0;
    int higherX = (p.x() + tmpMaxRadius) < _pix_width ? p.x() + tmpMaxRadius : _pix_width;
    int lowerY = (p.y() - tmpMaxRadius) > 0 ? p.y() - tmpMaxRadius : 0;
    int higherY = (p.y() + tmpMaxRadius) < _pix_height ? p.y() + tmpMaxRadius : _pix_height;
    assert(higherX > lowerX);
    assert(higherY > lowerY);
    for (int x = lowerX; x <= higherX; x += 32)
        for (int y = lowerY; y <= higherY; y += 32)
        {
            double dist = p.getDistance(Position(x, y));
            if (dist <= tmpMaxRadius && dist > tmpMinRadius)
            {
                int xx = x/32;
                int yy = y/32;
                grad[xx + yy*Broodwar->mapWidth()] = Vec(0, 0);
                for (int tmpx = xx - 1; tmpx <= xx + 1; ++tmpx)
                    for (int tmpy = yy - 1; tmpy <= yy + 1; ++tmpy)
                    {
                        int deltax = tmpx - xx;
                        int deltay = tmpy - yy;
                        if (deltax == 0 && deltay == 0)
                            continue;
                        if (tmpx < 0 || tmpy < 0)
                            continue;
                        if (tmpx >= Broodwar->mapWidth() || tmpy >= Broodwar->mapHeight())
                            continue;
                        grad[xx + yy*Broodwar->mapWidth()] += 
                            Vec(deltax, deltay)*tab[tmpx + (tmpy)*Broodwar->mapWidth()];
                    }
            }                
        }
}

void MapManager::removeDmg(UnitType ut, Position p)
{
    if (ut.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeGround(ut);
        modifyDamages(this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            - ut.groundWeapon().damageAmount() * ut.maxGroundHits());
        updateDamagesGrad(this->groundDamagesGrad, this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft());
    }
    if (ut.airWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeAir(ut);
        modifyDamages(this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            - ut.airWeapon().damageAmount() * ut.maxAirHits());
        updateDamagesGrad(this->airDamagesGrad, this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft());
    }
}

void MapManager::removeDmgStorm(Position p)
{
    modifyDamages(this->groundDamages, p, 0, 63, -50);
    modifyDamages(this->airDamages, p, 0, 63, -50);
    updateDamagesGrad(this->groundDamagesGrad, this->groundDamages, p, 0, 63);
    updateDamagesGrad(this->airDamagesGrad, this->airDamages, p, 0, 63);
}

void MapManager::addDmg(UnitType ut, Position p)
{ 
    if (ut.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeGround(ut);
        modifyDamages(this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            ut.groundWeapon().damageAmount() * ut.maxGroundHits());
        updateDamagesGrad(this->groundDamagesGrad, this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange() + addRange);
    }
    if (ut.airWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeAir(ut);
        modifyDamages(this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            ut.airWeapon().damageAmount() * ut.maxAirHits());
        updateDamagesGrad(this->airDamagesGrad, this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange() + addRange);
    }
}

void MapManager::addDmgWithoutGrad(UnitType ut, Position p)
{ 
    if (ut.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeGround(ut);
        modifyDamages(this->groundDamages, p, ut.groundWeapon().minRange(), ut.groundWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            ut.groundWeapon().damageAmount() * ut.maxGroundHits());
    }
    if (ut.airWeapon() != BWAPI::WeaponTypes::None)
    {
        int addRange = additionalRangeAir(ut);
        modifyDamages(this->airDamages, p, ut.airWeapon().minRange(), ut.airWeapon().maxRange() + addRange + ut.dimensionRight() + ut.dimensionLeft(), 
            ut.airWeapon().damageAmount() * ut.maxAirHits());
    }
}

void MapManager::addDmgStorm(Position p)
{
    // storm don't stack, but we make them stack
    modifyDamages(this->groundDamages, p, 0, 63, 50); 
    modifyDamages(this->airDamages, p, 0, 63, 50);
}

int MapManager::additionalRangeGround(UnitType ut)
{
    // we consider that the upgrades are always researched/up
    if (ut == UnitTypes::Protoss_Dragoon)
        return 64;
    else if (ut == UnitTypes::Terran_Marine)
        return 32;
    else if (ut == UnitTypes::Zerg_Hydralisk)
        return 32;
    else if (ut == UnitTypes::Terran_Vulture_Spider_Mine)
        return 64;
    else 
        return 0;
}

int MapManager::additionalRangeAir(UnitType ut)
{
    // we consider that the upgrades are always researched/up
    if (ut == UnitTypes::Protoss_Dragoon)
        return 64;
    else if (ut == UnitTypes::Terran_Marine)
        return 32;
    else if (ut == UnitTypes::Terran_Goliath)
        return 96;
    else if (ut == UnitTypes::Zerg_Hydralisk)
        return 32;
    else if (ut == UnitTypes::Terran_Missile_Turret 
        || ut == UnitTypes::Zerg_Spore_Colony
        || ut == UnitTypes::Protoss_Photon_Cannon)
        return 32;
    else 
        return 0;
}

void MapManager::onUnitCreate(Unit* u)
{
    addBuilding(u);
    if (u->getPlayer() == Broodwar->self())
        addAlliedUnit(u);
}

void MapManager::onUnitDestroy(Unit* u)
{
    removeBuilding(u);
    if (u->getPlayer() == Broodwar->enemy())
    {
        removeDmg(u->getType(), _trackedUnits[u]);
        _trackedUnits.erase(u);
    }
    else
    {
        removeAlliedUnit(u);
    }
}

void MapManager::onUnitShow(Unit* u)
{
    addBuilding(u);
    if (u->getPlayer() == Broodwar->self())
        addAlliedUnit(u);
}

void MapManager::onUnitHide(Unit* u)
{
    addBuilding(u);
}

DWORD WINAPI MapManager::StaticLaunchUpdateStormPos(void* obj)
{
    MapManager* This = (MapManager*) obj;
    int buf = This->LaunchUpdateStormPos();
    ExitThread(buf);
    return buf;
}

DWORD MapManager::LaunchUpdateStormPos()
{
    DWORD waitResult = WaitForSingleObject(
        _stormPosMutex,
        100);

    switch (waitResult) 
    {
    case WAIT_OBJECT_0: 
        updateStormPos();
        ReleaseMutex(_stormPosMutex);
        break; 

    case WAIT_ABANDONED:
        ReleaseMutex(_stormPosMutex);
        return -1;
    }
    return 0;
}

void MapManager::updateStormPos()
{
    _stormPosBuf.clear();
    // Construct different possible positions for the storms, shifting by __MESH_SIZE__
    std::set<Position> possiblePos;
    for (std::map<BWAPI::Unit*, BWAPI::Position>::const_iterator eit = _enemyUnitsPosBuf.begin();
        eit != _enemyUnitsPosBuf.end(); ++eit)
    {
        Position tmpPos = eit->second;
        if (possiblePos.size() > 32) // TOCHANGE 32 units
            possiblePos.insert(tmpPos);
        else
        {
            possiblePos.insert(tmpPos);
            if (tmpPos.x() >= __MESH_SIZE__)
            {
                possiblePos.insert(Position(tmpPos.x() - __MESH_SIZE__, tmpPos.y()));
                if (tmpPos.y() + __MESH_SIZE__ < _pix_height)
                    possiblePos.insert(Position(tmpPos.x() - __MESH_SIZE__, tmpPos.y() - __MESH_SIZE__));
                if (tmpPos.y() >= __MESH_SIZE__)
                    possiblePos.insert(Position(tmpPos.x() - __MESH_SIZE__, tmpPos.y() + __MESH_SIZE__));
            }
            if (tmpPos.x() + __MESH_SIZE__ < _pix_width)
            {
                possiblePos.insert(Position(tmpPos.x() + __MESH_SIZE__, tmpPos.y()));
                if (tmpPos.y() >= __MESH_SIZE__)
                    possiblePos.insert(Position(tmpPos.x() + __MESH_SIZE__, tmpPos.y() + __MESH_SIZE__));
                if (tmpPos.y() + __MESH_SIZE__ < _pix_height)
                    possiblePos.insert(Position(tmpPos.x() + __MESH_SIZE__, tmpPos.y() - __MESH_SIZE__));
            }
            if (tmpPos.y() >= __MESH_SIZE__) 
                possiblePos.insert(Position(tmpPos.x(), tmpPos.y() - __MESH_SIZE__));
            if (tmpPos.y() + __MESH_SIZE__ < _pix_height)
                possiblePos.insert(Position(tmpPos.x(), tmpPos.y() + __MESH_SIZE__));
        }
    }
    // Score the possible possitions for the storms
    std::set<std::pair<int, Position> > storms;
    for (std::set<Position>::const_iterator it = possiblePos.begin();
        it != possiblePos.end(); ++it)
    {
        int tmp = 0;
        for (std::map<BWAPI::Unit*, BWAPI::Position>::const_iterator uit = _alliedUnitsPosBuf.begin();
            uit != _alliedUnitsPosBuf.end(); ++uit)
        {
            if (uit->second.x() > it->x() - (__STORM_SIZE__ / 2 + 1) && uit->second.x() < it->x() + (__STORM_SIZE__ / 2 + 1)
                && uit->second.y() > it->y() - (__STORM_SIZE__ / 2 + 1) && uit->second.y() < it->y() + (__STORM_SIZE__ / 2 + 1))
                tmp -= 4;
        }
        for (std::map<BWAPI::Unit*, BWAPI::Position>::const_iterator eit = _enemyUnitsPosBuf.begin();
            eit != _enemyUnitsPosBuf.end(); ++eit)
        {
            // TODO TOCHANGE 40 here, it could be 3 tiles x 32  / 2 = 48 or less (to account for their movement)
            if (eit->second.x() > it->x() - 40 && eit->second.x() < it->x() + 40
                && eit->second.y() > it->y() - 40 && eit->second.y() < it->y() + 40)
                tmp += 2;
            // TODO TOCHANGE 8 here, to center, it could be 1 tiles x 32 / 2 = 16 or less
            if (eit->second.x() > it->x() - 8 && eit->second.x() < it->x() + 8
                && eit->second.y() > it->y() - 8 && eit->second.y() < it->y() + 8)
                ++tmp;
        }
        for (std::map<BWAPI::Unit*, std::pair<BWAPI::UnitType, BWAPI::Position> >::const_iterator iit = _invisibleUnitsBuf.begin();
            iit != _invisibleUnitsBuf.end(); ++ iit)
        {
            if (iit->second.first != UnitTypes::Protoss_Observer && iit->second.first != UnitTypes::Zerg_Zergling
                && iit->second.first != UnitTypes::Terran_Vulture_Spider_Mine
                && iit->second.second.x() > it->x() - (__STORM_SIZE__ / 2 + 1) && iit->second.second.x() < it->x() + (__STORM_SIZE__ / 2 + 1)
                && iit->second.second.y() > it->y() - (__STORM_SIZE__ / 2 + 1) && iit->second.second.y() < it->y() + (__STORM_SIZE__ / 2 + 1))
                ++tmp;            
        }
        if (tmp > 0)
        {
            storms.insert(std::make_pair<int, Position>(tmp, *it));
        }
    }
    // Filter the positions for the storms by descending order + eliminate some overlapings
    std::set<Position> covered;
    for (std::set<std::pair<int, Position> >::const_reverse_iterator it = storms.rbegin();
        it != storms.rend(); ++it)
    {
        bool foundCoverage = false;
        for (std::set<Position>::const_iterator i = covered.begin();
            i != covered.end(); ++i)
        {
            if (it->second.x() > i->x() - (__STORM_SIZE__ / 2 + 1) && it->second.x() < i->x() + (__STORM_SIZE__ / 2 + 1)
                && it->second.y() > i->y() - (__STORM_SIZE__ / 2 + 1) && it->second.y() < i->y() + (__STORM_SIZE__ / 2 + 1))
            {
                foundCoverage = true;
                break;
            }
        }
        for (std::map<Position, int>::const_iterator i = _dontReStormBuf.begin();
            i != _dontReStormBuf.end(); ++i)
        {
            if (it->second.x() > i->first.x() - (__STORM_SIZE__ / 2 + 1) && it->second.x() < i->first.x() + (__STORM_SIZE__ / 2 + 1)
                && it->second.y() > i->first.y() - (__STORM_SIZE__ / 2 + 1) && it->second.y() < i->first.y() + (__STORM_SIZE__ / 2 + 1))
            {
                foundCoverage = true;
                break;
            }
        }
        if (!foundCoverage)
        {
            _stormPosBuf.insert(std::make_pair<Position, int>(it->second, it->first));
            covered.insert(it->second);
        }
    }
    return;
}

void MapManager::justStormed(Position p)
{
    stormPos.erase(p);
    // remove the >= 4/9 (buildtiles) overlaping storms yet present in stormPos
    for (std::map<Position, int>::iterator it = stormPos.begin();
        it != stormPos.end(); )
    {
        if (it->first.getDistance(p) < 46.0)
        {
            stormPos.erase(it++);
        }
        else
            ++it;
    }
    if (_dontReStorm.count(p))
        _dontReStorm[p] = 0;
    else
        _dontReStorm.insert(std::make_pair<Position, int>(p, 0));
}

void MapManager::update()
{
#ifdef __DEBUG__
    clock_t start = clock();
#endif
    // update our units' positions
    for (std::map<BWAPI::Unit*, BWAPI::Position>::iterator it = _ourUnits.begin();
        it != _ourUnits.end(); ++it)
    {
        _ourUnits[it->first] = it->first->getPosition();
    }

    // check/update the damage maps. BEWARE: hidden units are not removed in presence of doubt!
    std::list<Unit*> toFilter;
    for (std::map<BWAPI::Unit*, EViewedUnit>::const_iterator it = _eUnitsFilter->getViewedUnits().begin();
        it != _eUnitsFilter->getViewedUnits().end(); ++it)
    {
        if (_trackedUnits.count(it->first))
        {
            if (it->first->exists()
                && it->first->isVisible()
                && (_trackedUnits[it->first] != it->first->getPosition())) // it moved
            {
                // update EUnitsFilter
                _eUnitsFilter->update(it->first);
                // update MapManager (ourselves)
                if (it->first->getType().isWorker()
                    && (it->first->isRepairing() || it->first->isConstructing()
                    || it->first->isGatheringGas() || it->first->isGatheringMinerals()))
                {
                    removeDmg(it->first->getType(), _trackedUnits[it->first]);
                    _trackedUnits.erase(it->first);
                }
                else
                {
                    addDmgWithoutGrad(it->first->getType(), it->first->getPosition());
                    removeDmg(it->first->getType(), _trackedUnits[it->first]);
                    _trackedUnits[it->first] = it->first->getPosition();
                }
            }
        }
        else
        {
            if (it->first->exists()
                && it->first->isVisible() // SEGFAULT
                && !(it->first->getType().isWorker()
                && (it->first->isRepairing() || it->first->isConstructing()
                || it->first->isGatheringGas() || it->first->isGatheringMinerals())))
            {
                // add it to MapManager (ourselves)
                addDmg(it->first->getType(), it->first->getPosition());
                _trackedUnits.insert(std::make_pair<Unit*, Position>(it->first, it->first->getPosition()));
            }
        }
        if (!(it->first->isVisible()))
        {
            toFilter.push_back(it->first);
        }
    }
    for (std::map<Unit*, Position>::iterator it = _trackedUnits.begin();
       it != _trackedUnits.end(); )
    {
        if (!it->first->isVisible() && !(_eUnitsFilter->getInvisibleUnits().count(it->first)))
        {
            //Broodwar->printf("removing a %s", _eUnitsFilter->getViewedUnit(it->first).type.getName().c_str());
            removeDmg(_eUnitsFilter->getViewedUnit(it->first).type, _trackedUnits[it->first]);
            _trackedUnits.erase(it++);
        }
        else
            ++it;
    }
    for (std::list<Unit*>::const_iterator it = toFilter.begin();
        it != toFilter.end(); ++it)
    {
        _eUnitsFilter->filter(*it);
    }

    // Iterate of all the Bullets to extract the interesting ones
    for (std::set<Bullet*>::const_iterator it = Broodwar->getBullets().begin();
        it != Broodwar->getBullets().end(); ++it)
    {
        if ((*it)->getType() == BWAPI::BulletTypes::Psionic_Storm)
        {
            if ((*it)->exists() && !_trackedStorms.count(*it))
            {
                _trackedStorms.insert(std::make_pair<Bullet*, Position>(*it, (*it)->getPosition()));
                addDmgStorm((*it)->getPosition());                
            }
        }
    }
    // Updating the damages maps with storms 
    // (overlapping => more damage, that's false but easy AND handy b/c of durations)
    for (std::map<Bullet*, Position>::iterator it = _trackedStorms.begin();
        it != _trackedStorms.end(); )
    {
        if (!it->first->exists())
        {
            removeDmgStorm(it->second);
            _trackedStorms.erase(it++);
        }
        else
            ++it;
    }

    if (Broodwar->self()->hasResearched(BWAPI::TechTypes::Psionic_Storm))
    {
        // update the possible storms positions
        if (WaitForSingleObject(_stormPosMutex, 0) == WAIT_OBJECT_0) // cannot enter when the thread is running
        {
            /// Update stormPos
            stormPos = _stormPosBuf;
            int lastUpdateDiff = Broodwar->getFrameCount() - _lastStormUpdateFrame;
            // decay the "dont re storm" positions + erase
            for (std::map<Position, int>::iterator it = _dontReStorm.begin();
                it != _dontReStorm.end(); )
            {
                if (stormPos.count(it->first))
                    stormPos.erase(it->first);
                _dontReStorm[it->first] = _dontReStorm[it->first] + lastUpdateDiff;
                if (it->second > 72)
                {
                    _dontReStorm.erase(it++);
                }
                else
                    ++it;
            }
            _lastStormUpdateFrame = Broodwar->getFrameCount();

            /// Prepare for the next update of _stormPosBuf thread
            _enemyUnitsPosBuf = HighTemplarUnit::stormableUnits;
            if (!_enemyUnitsPosBuf.empty())
            {
                _alliedUnitsPosBuf = _ourUnits;
                _invisibleUnitsBuf = _eUnitsFilter->getInvisibleUnits();
#ifdef __DEBUG__
                for (std::map<Unit*, std::pair<UnitType, Position> >::const_iterator ii = _invisibleUnitsBuf.begin();
                    ii != _invisibleUnitsBuf.end(); ++ii)
                    Broodwar->drawCircleMap(ii->second.second.x(), ii->second.second.y(), 8, Colors::Red, true);
#endif
                // Don't restorm where there are already existing storms, lasting more than 48 frames
                for (std::map<Bullet*, Position>::const_iterator it = _trackedStorms.begin();
                    it != _trackedStorms.end(); ++it)
                {
                    if (it->first->exists() && it->first->getRemoveTimer() > 0) // TODO TOCHANGE 47
                    {
                        if (_dontReStorm.count(it->second))
                            _dontReStorm[it->second] = 72 - it->first->getRemoveTimer();
                        else
                            _dontReStorm.insert(std::make_pair<Position, int>(it->second, 72 - it->first->getRemoveTimer()));
                    }
                }
                _dontReStormBuf = _dontReStorm;
                // this thread is doing updateStormPos();
                DWORD threadId;
                HANDLE thread = CreateThread( 
                    NULL,                   // default security attributes
                    0,                      // use default stack size  
                    &MapManager::StaticLaunchUpdateStormPos,      // thread function name
                    (void*) this,                   // argument to thread function 
                    0,                      // use default creation flags 
                    &threadId);             // returns the thread identifier 
                if (thread == NULL)
                {
                    Broodwar->printf("(mapmanager) error creating thread");
                }
                CloseHandle(thread);
            }
            else
            {
                _stormPosBuf.clear();
            }
        }
        ReleaseMutex(_stormPosMutex);
    }

#ifdef __DEBUG__
    clock_t end = clock();
    double duration = (double)(end - start) / CLOCKS_PER_SEC;
    if (duration > 0.040) 
        Broodwar->printf("MapManager::update() took: %2.5f seconds\n", duration);
    //this->drawGroundDamagesGrad(); // DRAW
    //this->drawGroundDamages();
    //this->drawAirDamagesGrad();
    //this->drawAirDamages();
    this->drawBestStorms();
#endif
}

const std::map<BWAPI::Unit*, BWAPI::Position>& MapManager::getOurUnits()
{
    return _ourUnits;
}

const std::map<BWAPI::Unit*, BWAPI::Position>& MapManager::getTrackedUnits()
{
    return _trackedUnits;
}

const std::map<BWAPI::Bullet*, BWAPI::Position>& MapManager::getTrackedStorms()
{
    return _trackedStorms;
}

Position MapManager::closestWalkabableSameRegionOrConnected(Position p)
{
    if (!p.isValid())
        p.makeValid();
    WalkTilePosition wtp(p);
    TilePosition tp(p);
    BWTA::Region* r = BWTA::getRegion(tp);
    int lowerX = (wtp.x() - 1) > 0 ? wtp.x() - 1 : 0;
    int higherX = (wtp.x() + 1) < _width ? wtp.x() + 1 : _width;
    int lowerY = (wtp.y() - 1) > 0 ? wtp.y() - 1 : 0;
    int higherY = (wtp.y() + 1) < _height ? wtp.y() + 1 : _height;
    Position saved = Positions::None;
    for (int x = lowerX; x <= higherX; ++x)
    {
        for (int y = lowerY; y <= higherY; ++y)
        {
            if (walkability[x + y*_width])
            {
                if (BWTA::getRegion(x/4, y/4) == r)
                    return WalkTilePosition(x, y).getPosition();
                else if (BWTA::isConnected(TilePosition(x/4, y/4), tp))
                    saved = WalkTilePosition(x, y).getPosition();
            }
        }
    }
    if (saved != Positions::None)
        return saved;
    // else, quickly (so, approximately, not as exact as in the method name :))
    lowerX = (wtp.x() - 4) > 0 ? wtp.x() - 4 : 0;
    higherX = (wtp.x() + 4) < _width ? wtp.x() + 4 : _width;
    lowerY = (wtp.y() - 4) > 0 ? wtp.y() - 4 : 0;
    higherY = (wtp.y() + 4) < _height ? wtp.y() + 4 : _height;
    for (int x = lowerX; x <= higherX; ++x)
    {
        for (int y = lowerY; y <= higherY; ++y)
        {
            if (walkability[x + y*_width])
            {
                if (BWTA::getRegion(x/4, y/4) == r)
                    return WalkTilePosition(x, y).getPosition();
                else if (BWTA::isConnected(TilePosition(x/4, y/4), tp))
                    saved = WalkTilePosition(x, y).getPosition();
            }
        }
    }
    if (saved != Positions::None)
        return saved;
    return Positions::None;
}

TilePosition MapManager::closestWalkabableSameRegionOrConnected(TilePosition tp)
{
    if (!tp.isValid())
        tp.makeValid();
    BWTA::Region* r = BWTA::getRegion(tp);
    int lowerX = (tp.x() - 1) > 0 ? tp.x() - 1 : 0;
    int higherX = (tp.x() + 1) < Broodwar->mapWidth() ? tp.x() + 1 : Broodwar->mapWidth();
    int lowerY = (tp.y() - 1) > 0 ? tp.y() - 1 : 0;
    int higherY = (tp.y() + 1) < Broodwar->mapHeight() ? tp.y() + 1 : Broodwar->mapHeight();
    TilePosition saved = TilePositions::None;
    for (int x = lowerX; x <= higherX; ++x)
    {
        for (int y = lowerY; y <= higherY; ++y)
        {
#ifdef __DEBUG__
            //Broodwar->drawBoxMap(x*32 + 2, y*32 + 2, x*32+29, y*32+29, Colors::Red);
#endif
            if (lowResWalkability[x + y*Broodwar->mapWidth()])
            {
                if (BWTA::getRegion(x, y) == r)
                    return TilePosition(x, y);
                else if (BWTA::isConnected(TilePosition(x, y), tp))
                    saved = TilePosition(x, y);
            }
        }
    }
    if (saved != TilePositions::None)
        return saved;
    // else, quickly (so, approximately, not as exact as in the method name :))
    lowerX = (tp.x() - 2) > 0 ? tp.x() - 2 : 0;
    higherX = (tp.x() + 2) < Broodwar->mapWidth() ? tp.x() + 2 : Broodwar->mapWidth();
    lowerY = (tp.y() - 2) > 0 ? tp.y() - 2 : 0;
    higherY = (tp.y() + 2) < Broodwar->mapHeight() ? tp.y() + 2 : Broodwar->mapHeight();
    for (int x = lowerX; x <= higherX; ++x)
    {
        for (int y = lowerY; y <= higherY; ++y)
        {
#ifdef __DEBUG__
            //Broodwar->drawBoxMap(x*32 + 2, y*32 + 2, x*32+29, y*32+29, Colors::Red);
#endif
            if (lowResWalkability[x + y*Broodwar->mapWidth()])
            {
                if (BWTA::getRegion(x, y) == r)
                    return TilePosition(x, y);
                else if (BWTA::isConnected(TilePosition(x, y), tp))
                    saved = TilePosition(x, y);
            }
        }
    }
    if (saved != TilePositions::None)
        return saved;
    return TilePositions::None;
}

bool MapManager::isBTWalkable(int x, int y)
{
	return lowResWalkability[x + y*Broodwar->mapWidth()] 
		   && !buildings[x + y*Broodwar->mapWidth()];
}

bool MapManager::isBTWalkable(TilePosition tp)
{
	return lowResWalkability[tp.x() + tp.y()*Broodwar->mapWidth()] 
		   && !buildings[tp.x() + tp.y()*Broodwar->mapWidth()];
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

void MapManager::drawGroundDamagesGrad()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            Vec tmp = groundDamagesGrad[x + y*Broodwar->mapWidth()];
            tmp.normalize();
            tmp *= 14;
            Position tmpPos = Position(x*32, y*32);
            if (groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 0 && groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 40)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::White);
            }
            else if (groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 40 && groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 80)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Yellow);
            }
            else if (groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 80 && groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 160)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Orange);
            }
            else if (groundDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 160)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Red);
            }
        }
}

void MapManager::drawAirDamagesGrad()
{
    for (int x = 0; x < Broodwar->mapWidth(); ++x)
        for (int y = 0; y < Broodwar->mapHeight(); ++y)
        {
            Vec tmp = airDamagesGrad[x + y*Broodwar->mapWidth()];
            tmp.normalize();
            tmp *= 14;
            Position tmpPos = Position(x*32, y*32);
            if (airDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 0 && airDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 40)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::White);
            }
            else if (airDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 40 && airDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 80)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Yellow);
            }
            else if (airDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 80 && airDamagesGrad[x + y*Broodwar->mapWidth()].norm() <= 160)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Orange);
            }
            else if (airDamagesGrad[x + y*Broodwar->mapWidth()].norm() > 160)
            {
                Broodwar->drawLineMap(tmpPos.x(), tmpPos.y(), tmp.translate(tmpPos).x(), tmp.translate(tmpPos).y(), Colors::Red);
            }
        }
}

void MapManager::drawBestStorms()
{
    for (std::map<Position, int>::const_iterator it = stormPos.begin();
        it != stormPos.end(); ++it)
    {
        Broodwar->drawBoxMap(it->first.x() - 48, it->first.y() - 48, it->first.x() + 48, it->first.y() + 48, BWAPI::Colors::Purple);
        char score[5];
        sprintf_s(score, "%d", it->second);
        Broodwar->drawTextMap(it->first.x() + 46, it->first.y() + 46, score);
    }
}