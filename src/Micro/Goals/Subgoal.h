#pragma once
#include <boost/shared_ptr.hpp>
#include <BWAPI.h>
#include "Micro/UnitsGroup.h"

typedef enum
{
	SL_AND    = 0,
	SL_OR     = 1
} SubgoalLogic;
	
class Subgoal
{
public:
    Subgoal(SubgoalLogic l);
    Subgoal(const Subgoal& sg);
    ~Subgoal();
    //Accessors
	SubgoalLogic getLogic() const;
	//Check accomplishment
	virtual bool isRealized() = 0; //Return if the subgoal is accomplished
	//isRealized can return check(), but some subgoals will need to 
	//be accomplished only once to complete the goal, others must be valid
	//all along the goal accomplishment
	virtual void tryToRealize() = 0; //Give suggestions to the UnitsGroup
	virtual double distanceToRealize() = 0; //Return an estimated distance to accomplish the Subgoal
	//if the return value is negative, it means that the subgoal cannot give suggestions to the UnitsGroup
	void setUnitsGroup(UnitsGroup * ug);
protected:
	virtual bool check() = 0; //Function that defines the condition of the subgoal
	SubgoalLogic logic;
	UnitsGroup* unitsGroup;
};