#pragma once
#include <BWAPI.h>
#include "CSingleton.h"
#include "EUnitsFilter.h"
#include <vector>
#include "Vec.h"

/** From BWAPI's doc:
* Positions are measured in pixels and are the highest resolution
* Walk Tiles - each walk tile is an 8x8 square of pixels. These are called walk tiles because walkability data is available at this resolution.
* Build Tiles - each build tile is a 4x4 square of walk tiles, or a 32x32 square of pixels. These are called build tiles because buildability data is available at this resolution. 
*/

// TODO problem when we lift a terran building (we should call onUnitDestroy)
class MapManager: public CSingleton<MapManager>
{
    friend class CSingleton<MapManager>;

private:
    EUnitsFilter* _eUnitsFilter;
    std::map<BWAPI::Unit*, BWAPI::Position> _trackedUnits;
    std::map<BWAPI::Bullet*, BWAPI::Position> _trackedStorms;
    MapManager();
    ~MapManager();
    int _width;
    int _height;
    int _pix_width;
    int _pix_height;
    inline void modifyBuildings(BWAPI::Unit* u, bool b);
    inline void addBuilding(BWAPI::Unit* u);
    inline void removeBuilding(BWAPI::Unit* u);
    inline void modifyDamages(int* tab, Position p, int minRadius, int maxRadius, int damages);
    inline void updateDamagesGrad(Vec* grad, int* tab, Position p, int minRadius, int maxRadius);
    inline void removeDmg(BWAPI::UnitType ut, BWAPI::Position p);
    inline void removeDmgStorm(BWAPI::Position p);
    inline void addDmg(BWAPI::UnitType ut, BWAPI::Position p);
    inline void addDmgStorm(BWAPI::Position p);
    inline int additionalRangeGround(BWAPI::UnitType ut);
    inline int additionalRangeAir(BWAPI::UnitType ut);
public:
    bool* walkability;          // walk tiles
    bool* lowResWalkability;    // low res => building tiles
    bool* buildings_wt;         // walk tiles
    bool* buildings_wt_strict;
    bool* buildings;            // low res => building tiless
    int* groundDamages;         // build tiles
    int* airDamages;            // build tiles
    Vec* groundDamagesGrad;
    Vec* airDamagesGrad;
    void setDependencies();
    void onUnitCreate(BWAPI::Unit* u);
    void onUnitDestroy(BWAPI::Unit* u);
    void onUnitShow(BWAPI::Unit* u);
    void onUnitHide(BWAPI::Unit* u);
    void onFrame();
    std::map<BWAPI::Bullet*, BWAPI::Position> getTrackedStorms();
    void drawBuildings();   // debug
    void drawBuildingsStrict();   // debug
    void drawWalkability(); // debug
    void drawLowResWalkability(); // debug
    void drawLowResBuildings(); // debug
    void drawGroundDamages(); // debug
    void drawAirDamages(); // debug
    void drawGroundDamagesGrad(); // debug
    void drawAirDamagesGrad(); // debug
};
