#include <PrecompiledHeader.h>
#include "Formation.h"

using namespace BWAPI;

Formation::Formation(const Formation& f)
: center(f.center)
, direction(f.direction)
, mean(f.center)
, end_positions(f.end_positions)
, space(24)
{
	log("created a formation\n");
}

Formation::Formation(const Vec& center, const Vec& direction)
: center(center)
, direction(direction)
, mean(center)
, space(24)
{
    end_positions.clear();
	log("created a formation\n");
}

Formation::Formation(const Position& p, const Vec& direction)
: center(p.x(), p.y())
, direction( direction)
, mean(p.x(), p.y())
, space(24)
{
    end_positions.clear();
	log("created a formation\n");
}

Formation::~Formation()
{
	log("deleted a formation\n");
}

void Formation::computeMean()
{
    if (!end_positions.size())
        return;
    mean = Vec(0, 0);
    for (std::vector<BWAPI::Position>::const_iterator it = end_positions.begin();
        it != end_positions.end(); ++it)
    {
        mean.x += it->x();
        mean.y += it->y();
    }
    mean /= end_positions.size();
}

// No formations => on one point
void Formation::computeToPositions(const std::vector<pBayesianUnit>& vUnit)
{
	end_positions.clear();
	for (unsigned int i = 0; i < vUnit.size(); i++)
		end_positions.push_back(center.toPosition());
}
