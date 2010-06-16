#include "BayesianUnit.h"
#include "Rainbow.h"
#include <utility>
#ifdef PROBT
#include <pl.h>
#else
#include <boost/math/distributions/normal.hpp>
using boost::math::normal;
#endif

using namespace std;
using namespace BWAPI;

/** Attraction/Repulsion factors
- attraction fire range
- r�pulsion foe fire range
- follow lead in flocking mode
- group (friends) unit interaction:
  ��in_position: 1) 2)
  � flocking: 1) 2)
  � fight: 1) 2) 
- repulsion area of effect damage
*/

#define _PROB_NO_FLOCK_MOVE 0.1 // TODO change for const and set specific values depending on the unit type
#define _PROB_NO_EUNIT_MOVE 0.9
#define _PROB_NO_UNIT_MOVE 0.8
#define _PROB_NO_WALL_MOVE 0.9999999
#define _PROB_NO_BUILDING_MOVE 0.99999
#define _PROB_GO_OBJ 0.9

// TODO optimize by removing the dirv inside the unit (but one, the center, when it's better not to move at all)

BayesianUnit::BayesianUnit(Unit* u, vector<pBayesianUnit> const * ug)
: BattleUnit(u)
, dir(Vec(unit->getVelocityX(), unit->getVelocityY()))
, _mode(MODE_FLOCK)
, _ground_unit(!u->getType().isFlyer())
, _unitsGroup(ug)
, _sheight(unit->getType().dimensionUp() + unit->getType().dimensionDown())
, _slarge(unit->getType().dimensionRight() + unit->getType().dimensionLeft())
{
    updateDirV();
    mapManager = & MapManager::Instance();
    switchMode(_mode);
    //updateProx();
    updateAttractors();
    initDefaultProb();
    if (_mode == MODE_FLOCK)
    {
        _flockProb.push_back(_PROB_NO_FLOCK_MOVE);  //FLOCK_NO
        _flockProb.push_back(0.05);                  //FLOCK_CONTACT
        _flockProb.push_back(0.25);                 //FLOCK_CLOSE
        _flockProb.push_back(0.38);                  //FLOCK_MEDIUM
        _flockProb.push_back(0.22);                 //FLOCK_FAR
    }

    /// TEST test_sizes_tanks_marines 
    /// Broodwar->printf("I'm a %s and I do h: %d, w: %d \n", 
    ///     unit->getType().getName().c_str(), unit->getType().dimensionUp() - unit->getType().dimensionDown(), 
    ///     unit->getType().dimensionRight() - unit->getType().dimensionLeft());
    //unit->getType().size();
}

void BayesianUnit::initDefaultProb()
{
    _defaultProb.insert(make_pair(OCCUP_EUNIT, _PROB_NO_EUNIT_MOVE));         // P(enemy_unit_in_this_case=false | we_go_in_this_case=true)
    _defaultProb.insert(make_pair(OCCUP_UNIT, _PROB_NO_UNIT_MOVE));           // P(allied_unit_in_this_case=false | we_go_in_this_case=true)
    _defaultProb.insert(make_pair(OCCUP_BLOCKING, _PROB_NO_WALL_MOVE));       // P(this_case_is_blocking=false | we_go_in_this_case=true)
    _defaultProb.insert(make_pair(OCCUP_BUILDING, _PROB_NO_BUILDING_MOVE));   // P(there_is_a_building_in_this_case=false | we_go_in_this_case=true)
    //_defaultProb.insert(make_pair(OCCUP_FLOCK, _PROB_NO_FLOCK_MOVE));       // P(there_is_flocking_attraction=false | we_go_in_this_case=true)
}

void BayesianUnit::computeFlockValues()
{
    _flockValues.clear();
    for (unsigned int i = 0; i < _dirv.size(); ++i)
    {
        vector<flock_value> tmpv;
        for (vector<pBayesianUnit>::const_iterator it = _unitsGroup->begin(); 
            it != _unitsGroup->end(); ++it)
        {
            if ((*it)->unit == this->unit) continue; // we don't flock with ourselves!
            flock_value value = (flock_value)(1 + (int)((*it)->unit->getDistance(_dirv[i].translate(this->unit->getPosition()))/32));
            ///Position tmp = _dirv[i].translate(this->unit->getPosition());
            ///Vec tmpvit((*it)->unit->getVelocityX(), (*it)->unit->getVelocityY()); // we flock with the interpolated next position of other units
            ///tmpvit *= 8;
            //Broodwar->printf("X: %f, Y: %f \n", (*it)->unit->getVelocityX(), (*it)->unit->getVelocityY());
            ///flock_value value = (flock_value)(1 + (int)tmp.getDistance(tmpvit.translate((*it)->unit->getPosition())) / 32);
            // if (value == FLOCK_FAR + 1) --value; // some kind of hysteresis for FAR
            if (value <= FLOCK_FAR)
                tmpv.push_back(value);
            else
                tmpv.push_back(FLOCK_NO);
            //Broodwar->printf("distance int: %d, double %f\n", (int)(*it)->unit->getDistance(this->unit), (*it)->unit->getDistance(this->unit));
            //Broodwar->printf("Flock value %d\n", value);
        }
        _flockValues.push_back(tmpv);
    }
    // TODO: <OR> Heat dissipation model
}

void BayesianUnit::switchMode(unit_mode um)
{
    if (MODE_FLOCK)
    {
        _defaultProb[OCCUP_UNIT] = 0.6;
        //_defaultProb[OCCUP_FLOCK] = 0.1;
    }
}

void BayesianUnit::straightLine(vector<Position>& ppath, 
        const Position& p_start, const Position& p_end, bool quick)
{
    ppath.clear();
    //if (p_start == p_end) return;
    if (p_start.getDistance(p_end) < 8) return;
    // not anti-aliased :)
    vector<Vec> dirvnorm;
    for (unsigned int i = 0; i < _dirv.size(); ++i)
        dirvnorm.push_back(_dirv[i] / _dirv[i].norm());
    Position p_current = p_start;
    ppath.push_back(p_current);
    if (quick) 
    {
        for (int id = 0; p_current != p_end && id < 2; ++id)
        {
            //Vec line = Vec(end.x() - current.x(), end.y() - current.y());
            Vec line = Vec(p_end.x() - p_current.x(), 
                p_end.y() - p_current.y()); 
            Vec objnorm = line.normalize();
            double maxv = -10.;
            unsigned int imax = 0;
            for (unsigned int i = 0; i < _dirv.size(); ++i)
            {
                double tmp = dirvnorm[i].dot(objnorm);
                if (tmp > maxv || 
                        (tmp >= maxv && _dirv[i].norm() > _dirv[imax].norm()))
                {
                    maxv = tmp;
                    imax = i;
                }
            }
            p_current = _dirv[imax].translate(p_current);
            ppath.push_back(p_current);
        }
    } 
    else 
    {
        //while (current != end) 
        // TODO correct that, current and end rarely match
        //{
            //TODO
        //}
    }
}

void BayesianUnit::updateAttractors()
{
    _occupation.clear();
    if (_mode == MODE_FLOCK)
    {
        computeFlockValues();
    }
    const int width = Broodwar->mapWidth();
    Position up = unit->getPosition();
    for (unsigned int i = 0; i < _dirv.size(); ++i)
    {
        Position tmp = _dirv[i].translate(up);
        if (mapManager->buildings[tmp.x()/32 + (tmp.y()/32)*width])
            _occupation.push_back(OCCUP_BUILDING);
        else if (!mapManager->walkability[tmp.x()/8 + (tmp.y()/8)*4*width]) // tmp.x()/8 + (tmp.y()/2)*width
            _occupation.push_back(OCCUP_BLOCKING);
        else // TODO UNIT/EUNIT
            _occupation.push_back(OCCUP_NO);
    }
}

void BayesianUnit::drawAttractors()
{
    Position up = unit->getPosition();
    for (unsigned int i = 0; i < _dirv.size(); ++i)
    {
        Position p = _dirv[i].translate(up);
        if (!_occupation[i])
        {
            //Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() -2, p.x() + 2, p.y() + 2, Colors::White, true);
            continue;
        }
        if (_occupation[i] == OCCUP_BUILDING)
            Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() -2, p.x() + 2, p.y() + 2, Colors::Orange, true);
        else if (_occupation[i] == OCCUP_BLOCKING)
            Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() - 2, p.x() + 2, p.y() + 2, Colors::Red, true);
        if (_occupation[i] == OCCUP_UNIT)
            Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() - 2, p.x() + 2, p.y() + 2, Colors::Green, true);
        else if (_occupation[i] == OCCUP_EUNIT)
            Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() - 2, p.x() + 2, p.y() + 2, Colors::Purple, true);
        //if (_occupation[i].count(OCCUP_FLOCK))
        //    Broodwar->drawBox(CoordinateType::Map, p.x() - 2, p.y() - 2, p.x() + 2, p.y() + 2, Colors::Blue, true);
    }
}

void BayesianUnit::drawFlockValues()
{
    Rainbow colors = Rainbow(Color(12, 12, 255), 51);
    for (unsigned int i = 0; i < _flockValues.size(); ++i)
    {
        UnitType ut = this->unit->getType();
        string str = ut.getName();
        Position p = _dirv[i].translate(this->unit->getPosition());
        for (unsigned int j = 0; j < _flockValues[i].size(); ++j)
            Broodwar->drawBox(CoordinateType::Map, p.x() - 3 + j , p.y() - 3 + j, p.x() + 1 + j, p.y() + 1 + j, colors.rainbow[(_flockValues[i][j]+4) % 5], true);
    }
}

double BayesianUnit::computeProb(unsigned int i)
{
#ifdef PROBT
    // plSymbol A("A", PL_BINARY_TYPE);
#else
    double val = 1.;
    //for (multimap<Position, attractor_type>::const_iterator it = _prox.begin(); it != _prox.end(); ++it)

    if (_mode == MODE_FLOCK)
    {
        /// FLOCKING INFLUENCE
        double prob_obj = _PROB_GO_OBJ / _unitsGroup->size();
        //for (unsigned int j = 0; j < _flockValues[i].size(); ++j) // one j for each attractor
        //    val *= _flockProb[_flockValues[i][j]];

        /// OBJECTIVE (pathfinder) INFLUENCE
        if (_dirv[i] == obj)
            val *= prob_obj;
        else 
        {
            Vec dirvtmp = _dirv[i];
            dirvtmp.normalize();
            Vec objnorm = obj;
            objnorm.normalize();
            double tmp = dirvtmp.dot(objnorm);
            if (tmp > 0)
                val *= prob_obj*tmp;
            else
                val *= 1.0 - prob_obj;
        }
    }
    if (_occupation[i] == OCCUP_BUILDING) /// NON-WALKABLE (BUILDING) INFLUENCE
    {
        val *= 1.0-_PROB_NO_BUILDING_MOVE;
    }
    else if (_occupation[i] == OCCUP_BLOCKING) /// NON-WALKABLE INFLUENCE
    {
        val *= 1.0-_PROB_NO_WALL_MOVE;
    }
    //Broodwar->printf("val is %d \n", val);
#endif
    return val;
}

void BayesianUnit::drawProbs(multimap<double, Vec>& probs, int number)
{
    //Broodwar->printf("Size: %d\n", probs.size());
    Position up = unit->getPosition();
    Rainbow colors = Rainbow(Color(12, 12, 255), 51); // 5 colors
    for (multimap<double, Vec>::const_iterator it = probs.begin(); it != probs.end(); ++it)
    {
        //Broodwar->printf("proba: %f, Vec: %f, %f\n", it->first, it->second.x, it->second.y);
        if (number == 1)
        {
            const int middle = 64;
            if (it->first < 0.2)
                Broodwar->drawBoxScreen(middle - 4 + (int)it->second.x, middle - 4 + (int)it->second.y,
                middle + 4 + (int)it->second.x, middle + 4 + (int)it->second.y, colors.rainbow[0], true);
            else if (it->first >= 0.2 && it->first < 0.4)
                Broodwar->drawBoxScreen(middle - 4 + (int)it->second.x, middle - 4 + (int)it->second.y,
                middle + 4 + (int)it->second.x, middle + 4 + (int)it->second.y, colors.rainbow[1], true);
            else if (it->first >= 0.4 && it->first < 0.6)
                Broodwar->drawBoxScreen(middle - 4 + (int)it->second.x, middle - 4 + (int)it->second.y,
                middle + 4 + (int)it->second.x, middle + 4 + (int)it->second.y, colors.rainbow[2], true);
            else if (it->first >= 0.6 && it->first < 0.8)
                Broodwar->drawBoxScreen(middle - 4 + (int)it->second.x, middle - 4 + (int)it->second.y,
                middle + 4 + (int)it->second.x, middle + 4 + (int)it->second.y, colors.rainbow[3], true);
            else if (it->first >= 0.8)
                Broodwar->drawBoxScreen(middle - 4 + (int)it->second.x, middle - 4 + (int)it->second.y,
                middle + 4 + (int)it->second.x, middle + 4 + (int)it->second.y, colors.rainbow[4], true);
        } else
        {
            if (it->first < 0.2)
                Broodwar->drawBoxMap(up.x() - 4 + (int)it->second.x, up.y() - 4 + (int)it->second.y,
                up.x() + 4 + (int)it->second.x, up.y() + 4 + (int)it->second.y, colors.rainbow[0], true);
            else if (it->first >= 0.2 && it->first < 0.4)
                Broodwar->drawBoxMap(up.x() - 4 + (int)it->second.x, up.y() - 4 + (int)it->second.y,
                up.x() + 4 + (int)it->second.x, up.y() + 4 + (int)it->second.y, colors.rainbow[1], true);
            else if (it->first >= 0.4 && it->first < 0.6)
                Broodwar->drawBoxMap(up.x() - 4 + (int)it->second.x, up.y() - 4 + (int)it->second.y,
                up.x() + 4 + (int)it->second.x, up.y() + 4 + (int)it->second.y, colors.rainbow[2], true);
            else if (it->first >= 0.6 && it->first < 0.8)
                Broodwar->drawBoxMap(up.x() - 4 + (int)it->second.x, up.y() - 4 + (int)it->second.y,
                up.x() + 4 + (int)it->second.x, up.y() + 4 + (int)it->second.y, colors.rainbow[3], true);
            else if (it->first >= 0.8)
                Broodwar->drawBoxMap(up.x() - 4 + (int)it->second.x, up.y() - 4 + (int)it->second.y,
                up.x() + 4 + (int)it->second.x, up.y() + 4 + (int)it->second.y, colors.rainbow[4], true);
        }
    }
}

void BayesianUnit::updateObj()
{
    Position up = unit->getPosition();
    //pathFind(_path, unit->getPosition(), target); // TODO, too high cost for the moment
    straightLine(_ppath, up, target, true);
    if (_ppath.size() > 1)   // path[0] is the current unit position
    {
        Position p;
        if (_ppath.size() >= 3)
            p = _ppath[1]; // TODO
        else if (_path.size() >= 2) 
            p = _ppath[1];
        //Broodwar->printf("p.x: %d, p.y: %d, up.x: %d, up.y: %d\n", p.x(), p.y(), up.x(), up.y());
        obj = Vec(p.x() - up.x(), p.y() - up.y());
    }
}

void BayesianUnit::drawObj(int number)
{
    Position up = unit->getPosition();
    Broodwar->drawLine(CoordinateType::Map, up.x(), up.y(), up.x() + (int)obj.x, up.y() + (int)obj.y, Colors::Green);
    if (number == 0) return;
    if (number == 1)
    {
        const int middle = 64;
        for (unsigned int i = 0; i < this->_dirv.size(); ++i)
        {
            //Broodwar->printf("obj.x: %f, obj.y: %f, _dirv[i].x: %f, _dirv[i].y: %f\n", obj.x, obj.y, _dirv[i].x, _dirv[i].y);
            if (obj == _dirv[i])
                Broodwar->drawBoxScreen(middle - 4 + (int)_dirv[i].x, middle - 4 + (int)_dirv[i].y,
                middle + 4 + (int)_dirv[i].x, middle + 4 + (int)_dirv[i].y, Colors::Green, true);
            else
                Broodwar->drawBoxScreen(middle - 4 + (int)(int)_dirv[i].x, middle - 4 + (int)(int)_dirv[i].y,
                middle + 4 + (int)(int)_dirv[i].x, middle + 4 + (int)(int)_dirv[i].y, Colors::Orange);
        }
    } else
    {
        for (unsigned int i = 0; i < this->_dirv.size(); ++i)
        {
            //Broodwar->printf("obj.x: %f, obj.y: %f, _dirv[i].x: %f, _dirv[i].y: %f\n", obj.x, obj.y, _dirv[i].x, _dirv[i].y);
            if (obj == _dirv[i])
                Broodwar->drawBoxMap(up.x() - 4 + (int)_dirv[i].x, up.y() - 4 + (int)_dirv[i].y,
                up.x() + 4 + (int)_dirv[i].x, up.y() + 4 + (int)_dirv[i].y, Colors::Green, true);
            else
                Broodwar->drawBoxMap(up.x() - 4 + (int)(int)_dirv[i].x, up.y() - 4 + (int)(int)_dirv[i].y,
                up.x() + 4 + (int)(int)_dirv[i].x, up.y() + 4 + (int)(int)_dirv[i].y, Colors::Orange);
        }
    }
}

void BayesianUnit::drawOccupation(int number)
{
    Position up = unit->getPosition();
    if (number == 0) return;
    if (number == 1)
    {
        const int middleX = 64;
        const int middleY = 150;
        for (unsigned int i = 0; i < this->_dirv.size(); ++i)
        {
            //Broodwar->printf("obj.x: %f, obj.y: %f, _dirv[i].x: %f, _dirv[i].y: %f\n", obj.x, obj.y, _dirv[i].x, _dirv[i].y);
            if (_occupation[i] == OCCUP_BLOCKING)
                Broodwar->drawBoxScreen(middleX - 4 + (int)_dirv[i].x, middleY - 4 + (int)_dirv[i].y,
                middleX + 4 + (int)_dirv[i].x, middleY + 4 + (int)_dirv[i].y, Colors::Red, true);
            else if (_occupation[i] == OCCUP_BUILDING)
                Broodwar->drawBoxScreen(middleX - 4 + (int)(int)_dirv[i].x, middleY - 4 + (int)(int)_dirv[i].y,
                middleX + 4 + (int)(int)_dirv[i].x, middleY + 4 + (int)(int)_dirv[i].y, Colors::Orange, true);
        }
    } else
    {
        for (unsigned int i = 0; i < this->_dirv.size(); ++i)
        {
            //Broodwar->printf("obj.x: %f, obj.y: %f, _dirv[i].x: %f, _dirv[i].y: %f\n", obj.x, obj.y, _dirv[i].x, _dirv[i].y);
            if (_occupation[i] == OCCUP_BLOCKING)
                Broodwar->drawBoxMap(up.x() - 4 + (int)_dirv[i].x, up.y() - 4 + (int)_dirv[i].y,
                up.x() + 4 + (int)_dirv[i].x, up.y() + 4 + (int)_dirv[i].y, Colors::Red, true);
            else if (_occupation[i] == OCCUP_BUILDING)
                Broodwar->drawBoxMap(up.x() - 4 + (int)(int)_dirv[i].x, up.y() - 4 + (int)(int)_dirv[i].y,
                up.x() + 4 + (int)(int)_dirv[i].x, up.y() + 4 + (int)(int)_dirv[i].y, Colors::Orange, true);
        }
    }
}

void BayesianUnit::updateDirV()
{
    _dirv.clear();
    Position p = unit->getPosition();
    WalkTilePosition wtp(p);
    const int minx = max(p.x() - _slarge, 0);
    const int maxx = min(p.x() + _slarge, 32*Broodwar->mapWidth());
    const int miny = max(p.y() - _sheight, 0);
    const int maxy = min(p.y() + _sheight, 32*Broodwar->mapHeight());
    for (int x = -4; x <= 4; ++x)
        for (int y = -4; y <= 4; ++y)
        {
            Vec v(x*_slarge/2, y*_sheight/2);
            Position tmp = v.translate(p);
            if (tmp.x() <= maxx && tmp.y() <= maxy 
                    && tmp.x() >= minx && tmp.y() >= miny)
            {
                _dirv.push_back(v);
            }
        }  
    /*
    const int sh = 1 + _sheight/8;
    const int sl = 1 + _slarge/8;
    //Broodwar->printf("unit size : %d, %d\n", _sheight, _slarge);
    //Broodwar->printf("sh : %d, sl : %d\n", sh, sl);
    const int width = Broodwar->mapWidth();
    Position p = unit->getPosition();
    const int minx = max(p.x() - sl*8, 0);
    const int maxx = min(p.x() + sl*8, 32*width);
    const int miny = max(p.y() - sh*8, 0);
    const int maxy = min(p.y() + sh*8, 32*Broodwar->mapHeight());
    _dirv.clear();
    for (int x = - sl; x <= sl; ++x)
        for (int y = -sh; y <= sh; ++y)
        {
            Vec v(x, y);
            v *= 8;
            Position tmp = v.translate(p);
            if (tmp.x() <= maxx && tmp.y() <= maxy && tmp.x() >= minx && tmp.y() >= miny)
                _dirv.push_back(v);
        }
    */
}

void BayesianUnit::drawDirV()
{
    Position up = unit->getPosition();
    for (unsigned int i = 0; i < _dirv.size(); ++i)
    {
        //Broodwar->printf("_dirv[i].x: %f, _dirv[i].y: %f \n", _dirv[i].x, _dirv[i].y);
        Broodwar->drawLine(CoordinateType::Map, up.x(), up.y(), _dirv[i].translate(up).x(), _dirv[i].translate(up).y(), Colors::Grey);
    }
    for (unsigned int i = 0; i < _dirv.size(); ++i)
        if (_dirv[i].norm() < 15.5)
            Broodwar->drawLine(CoordinateType::Map, up.x(), up.y(), _dirv[i].translate(up).x(), _dirv[i].translate(up).y(), Colors::Black);
}

void BayesianUnit::updateDir()
{
    Position p = this->unit->getPosition();
    updateDirV();
    updateAttractors();
    updateObj();
    //drawObj(_unitsGroup->size());
    //drawObj(0);
    //drawOccupation(_unitsGroup->size());
    //drawPath();
    multimap<double, Vec> dirvProb;
    for (unsigned int i = 0; i < _dirv.size(); ++i)
        dirvProb.insert(make_pair(computeProb(i), _dirv[i]));
    multimap<double, Vec>::const_iterator last = dirvProb.end(); // I want the right probas and not 1-prob in the multimap
    --last;
    if (dirvProb.count(dirvProb.begin()->first) == 1)
        dir = last->second;
    else
    {
        pair<multimap<double, Vec>::const_iterator, multimap<double, Vec>::const_iterator> 
            possible_dirs = dirvProb.equal_range(last->first);
        double max = -100000.0;
        for (multimap<double, Vec>::const_iterator it = possible_dirs.first; it != possible_dirs.second; ++it)
        {
            double tmp = obj.dot(it->second);
            if (tmp > max)
            {
                max = tmp;
                dir = it->second;
            }
        }
    }
    drawProbs(dirvProb, _unitsGroup->size());
}

void BayesianUnit::drawDir()
{
    Position up = unit->getPosition();
    Broodwar->drawLineMap(up.x(), up.y(), dir.translate(up).x(), dir.translate(up).y(), Colors::Red);
}

void BayesianUnit::clickDir()
{
    dir += unit->getPosition();
    //if (unit->getPosition() != dir.toPosition()) TODO
    unit->rightClick(dir.toPosition());
}

void BayesianUnit::drawArrow(Vec& v)
{
    int xfrom = unit->getPosition().x();
    int yfrom = unit->getPosition().y();
    double xto = xfrom + 20*v.x; // 20, magic number
    double yto = yfrom + 20*v.y;
    double v_x = xto-xfrom; 
    double v_y = yto-yfrom;
    Broodwar->drawLine(CoordinateType::Map, xfrom, yfrom, (int)xto, (int)yto, Colors::Orange);
    Broodwar->drawTriangle(CoordinateType::Map, (int)(xto - 0.1*v_y), (int)(yto + 0.1*v_x), (int)(xto + 0.1*v_y), (int)(yto - 0.1*v_x), (int)(xto + 0.1*v_x), (int)(yto + 0.1*v_y), Colors::Orange); // 0.1, magic number
}

void BayesianUnit::deleteRangeEnemiesElem(Unit* u)
{
    // TODO change with Boost's Multi-Index or BiMap to avoid the O(n) search in the map
    // http://www.boost.org/doc/libs/1_42_0/libs/multi_index/doc/examples.html#example4
    // http://www.boost.org/doc/libs/1_42_0/libs/bimap/doc/html/boost_bimap/one_minute_tutorial.html
    for (std::multimap<double, Unit*>::const_iterator it = _rangeEnemies.begin(); it != _rangeEnemies.end(); ++it)
        if (it->second == u)
        {
            _rangeEnemies.erase(it);
            return;
        }
}

void BayesianUnit::updateRangeEnemiesWith(Unit* u)
{
    // TODO change with Boost's Multi-Index or BiMap to avoid the O(n) search in the map
    // http://www.boost.org/doc/libs/1_42_0/libs/multi_index/doc/examples.html#example4
    // http://www.boost.org/doc/libs/1_42_0/libs/bimap/doc/html/boost_bimap/one_minute_tutorial.html
    for (std::multimap<double, Unit*>::const_iterator it = _rangeEnemies.begin(); it != _rangeEnemies.end(); ++it)
        if (it->second == u)
        {
            std::pair<double, Unit*> temp(u->getDistance(unit->getPosition()), u);
            _rangeEnemies.erase(it);
            _rangeEnemies.insert(temp);
            return;
        }
    std::pair<double, Unit*> temp(u->getDistance(unit->getPosition()), u);
    _rangeEnemies.insert(temp);
}

void BayesianUnit::onUnitDestroy(Unit* u)
{
    deleteRangeEnemiesElem(u);
}

void BayesianUnit::onUnitShow(Unit* u)
{
    updateRangeEnemiesWith(u);
}

void BayesianUnit::onUnitHide(Unit* u)
{
    updateRangeEnemiesWith(u);
}

void BayesianUnit::update()
{
    if (!unit->exists()) return;
    if (_mode == MODE_FIGHT_G) {
        // TODO not every update()s, perhaps even asynchronously
        // TODO inline function!
        if (!unit->getGroundWeaponCooldown()) {
            std::multimap<double, Unit*>::const_iterator rangeEnemyUnit;
            rangeEnemyUnit = _rangeEnemies.begin();
            unsigned int i = 0;
            unsigned int end = _rangeEnemies.size();
            while (i < end) 
            {
                double enemyDistance = rangeEnemyUnit->second->getDistance(unit->getPosition());
                if (enemyDistance < unit->getType().groundWeapon().maxRange()) { // attack former closer if in range
                    unit->rightClick(rangeEnemyUnit->second->getPosition());
                    break;
                } else { // replace former close that is now out of range in the right position
                    if (enemyDistance > unit->getType().groundWeapon().maxRange() + rangeEnemyUnit->second->getType().groundWeapon().maxRange()) {
                        _rangeEnemies.erase(rangeEnemyUnit);
                    } else {
                        std::pair<double, Unit*> temp = *rangeEnemyUnit;
                        _rangeEnemies.erase(rangeEnemyUnit);
                        _rangeEnemies.insert(temp);
                    }
                    ++rangeEnemyUnit;
                    ++i;
                }
            }
            if (++i == end) {
                // NOT IMPL TODO
                // perhaps fill _rangeEnemies in the UnitsGroup (higher level)
                Broodwar->printf("me think I have no enemy unit in range, me perhaps stoodpid!\n");
            }
        }
    } else if (_mode == MODE_FLOCK) {
        //if (tick())
        {
            //drawAttractors();
            //drawTarget();
            updateDir();
            //drawDir();
            clickDir();
            //drawFlockValues();
        }
        //Broodwar->drawLine(CoordinateType::Map, unit->getPosition().x(), unit->getPosition().y(), target.x(), target.y(), BWAPI::Color(92, 92, 92));
    }
    /*if (tick())
    {
        _path = BWTA::getShortestPath(unit->getTilePosition(), target);
        // pathFind(_path, unit->getPosition(), target);
        updateDir();
    }*/
    // _path = BWTA::getShortestPath(unit->getTilePosition(), target);
    //if (tick()) 
    //    pathFind(_path, unit->getPosition(), target);
    ///// drawWalkability();
    //drawPath();
    // double velocity = sqrt(unit->getVelocityX()*unit->getVelocityX() + unit->getVelocityY()*unit->getVelocityY());
    // if (velocity > 0)
    //    Broodwar->printf("Velocity: %d\n", velocity);
    
    //drawVelocityArrow();
    //drawArrow(obj);
    //drawArrow(dir);
    //drawEnclosingBox();
}