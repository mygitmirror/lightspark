/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2013  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "scripting/abc.h"
#include "asobject.h"
#include "scripting/class.h"
#include "compat.h"
#include "parsing/amf3_generator.h"
#include "scripting/argconv.h"
#include "scripting/flash/errors/flasherrors.h"
#include "scripting/flash/utils/Timer.h"

using namespace std;
using namespace lightspark;

void Timer::tick()
{
	//This will be executed once if repeatCount was originally 1
	//Otherwise it's executed until stopMe is set to true
	this->incRef();
	getVm(getSystemState())->addEvent(_MR(this),_MR(Class<TimerEvent>::getInstanceS(getSystemState(),"timer")));

	currentCount++;
	if(repeatCount!=0)
	{
		if(currentCount==repeatCount)
		{
			this->incRef();
			getVm(getSystemState())->addEvent(_MR(this),_MR(Class<TimerEvent>::getInstanceS(getSystemState(),"timerComplete")));
			stopMe=true;
			running=false;
		}
	}
}

void Timer::tickFence()
{
	tickJobInstance = NullRef;
}


void Timer::sinit(Class_base* c)
{
	CLASS_SETUP(c, EventDispatcher, _constructor, CLASS_SEALED);
	c->setDeclaredMethodByQName("currentCount","",Class<IFunction>::getFunction(c->getSystemState(),_getCurrentCount),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("repeatCount","",Class<IFunction>::getFunction(c->getSystemState(),_getRepeatCount),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("repeatCount","",Class<IFunction>::getFunction(c->getSystemState(),_setRepeatCount),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("running","",Class<IFunction>::getFunction(c->getSystemState(),_getRunning),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("delay","",Class<IFunction>::getFunction(c->getSystemState(),_getDelay),GETTER_METHOD,true);
	c->setDeclaredMethodByQName("delay","",Class<IFunction>::getFunction(c->getSystemState(),_setDelay),SETTER_METHOD,true);
	c->setDeclaredMethodByQName("start","",Class<IFunction>::getFunction(c->getSystemState(),start),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("reset","",Class<IFunction>::getFunction(c->getSystemState(),reset),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("stop","",Class<IFunction>::getFunction(c->getSystemState(),stop),NORMAL_METHOD,true);
}

ASFUNCTIONBODY_ATOM(Timer,_constructor)
{
	EventDispatcher::_constructor(sys,obj,NULL,0);
	Timer* th=obj.as<Timer>();

	th->delay=args[0].toInt();
	if(argslen>=2)
		th->repeatCount=args[1].toInt();

	return asAtom::invalidAtom;
}

ASFUNCTIONBODY_ATOM(Timer,_getCurrentCount)
{
	Timer* th=obj.as<Timer>();
	return asAtom(th->currentCount);
}

ASFUNCTIONBODY_ATOM(Timer,_getRepeatCount)
{
	Timer* th=obj.as<Timer>();
	return asAtom(th->repeatCount);
}

ASFUNCTIONBODY_ATOM(Timer,_setRepeatCount)
{
	assert_and_throw(argslen==1);
	int32_t count=args[0].toInt();
	Timer* th=obj.as<Timer>();
	th->repeatCount=count;
	if(th->repeatCount>0 && th->repeatCount<=th->currentCount)
	{
		getSys()->removeJob(th);
		th->running=false;
		th->tickJobInstance = NullRef;
	}
	return asAtom::invalidAtom;
}

ASFUNCTIONBODY_ATOM(Timer,_getRunning)
{
	Timer* th=obj.as<Timer>();
	return asAtom(th->running);
}

ASFUNCTIONBODY_ATOM(Timer,_getDelay)
{
	Timer* th=obj.as<Timer>();
	return asAtom(th->delay);
}

ASFUNCTIONBODY_ATOM(Timer,_setDelay)
{
	assert_and_throw(argslen==1);
	int32_t newdelay = args[0].toInt();
	if (newdelay<=0)
		throw Class<RangeError>::getInstanceS(sys,"delay must be positive", 2066);

	Timer* th=obj.as<Timer>();
	th->delay=newdelay;

	return asAtom::invalidAtom;
}

ASFUNCTIONBODY_ATOM(Timer,start)
{
	Timer* th=obj.as<Timer>();
	if(th->running)
		return asAtom::invalidAtom;
	th->running=true;
	th->stopMe=false;
	th->incRef();
	th->tickJobInstance = _MNR(th);
	// according to spec Adobe handles timers 60 times per second, so minimum delay is 17 ms
	if(th->repeatCount==1)
		sys->addWait(th->delay < 17 ? 17 : th->delay,th);
	else
		sys->addTick(th->delay < 17 ? 17 : th->delay,th);
	return asAtom::invalidAtom;
}

ASFUNCTIONBODY_ATOM(Timer,reset)
{
	Timer* th=obj.as<Timer>();
	if(th->running)
	{
		//This spin waits if the timer is running right now
		sys->removeJob(th);
		//NOTE: although no new events will be sent now there might be old events in the queue.
		//Is this behaviour right?
		//This is not anymore used by the timer, so it can die
		th->tickJobInstance = NullRef;
		th->running=false;
	}
	th->currentCount=0;
	return asAtom::invalidAtom;
}

ASFUNCTIONBODY_ATOM(Timer,stop)
{
	Timer* th=obj.as<Timer>();
	if(th->running)
	{
		//This spin waits if the timer is running right now
		sys->removeJob(th);
		//NOTE: although no new events will be sent now there might be old events in the queue.
		//Is this behaviour right?

		//This is not anymore used by the timer, so it can die
		th->tickJobInstance = NullRef;
		th->running=false;
	}
	return asAtom::invalidAtom;
}

