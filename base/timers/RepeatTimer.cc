#include "RepeatTimer.h"

void RepeatTimer::init(BaseModule * parent)
{
	owner = parent;
	core = dynamic_cast < RepeatTimerCore * >
	    (findModuleType("RepeatTimerCore")->
	     createScheduleInit("generator", owner));
	core->init(this);
}
