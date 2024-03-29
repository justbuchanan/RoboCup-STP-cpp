
#include "Fullback.hpp"
#include <gameplay/GameplayModule.hpp>
#include "Goalie.hpp"

#include <Constants.hpp>
#include <gameplay/Window.hpp>
#include <Geometry2d/util.h>

#include <vector>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>


using namespace std;
using namespace Gameplay;
using namespace Tactics;



RobotRequirements Tactics::Fullback::robotRequirements = RobotRequirementNone;




vector<Fullback *> Tactics::Fullback::_allFullbacks;


namespace Tactics {
	REGISTER_TACTIC_CLASS(Fullback);
	REGISTER_CONFIGURABLE(Fullback);
}


ConfigDouble *Tactics::Fullback::_defend_goal_radius;
ConfigDouble *Tactics::Fullback::_opponent_avoid_threshold;





void Tactics::Fullback::createConfiguration(Configuration *cfg)
{
	_defend_goal_radius = new ConfigDouble(cfg, "Fullback/Defend Goal Radius", 0.9);
	_opponent_avoid_threshold = new ConfigDouble(cfg, "Fullback/Opponent Avoid Threshold", 2.0);
}



OpponentRobot* Tactics::Fullback::findRobotToBlock(const Geometry2d::Rect& area)
{
	OpponentRobot* target = 0;
	// Find by ball distance
	BOOST_FOREACH(OpponentRobot* r, systemState()->opp)
	{
		if(r && r->visible && area.contains(r->pos))
		{
			if(!target)
				target = r;
			else if(target->pos.distTo(ball().pos) > r->pos.distTo(ball().pos))
				target = r;
		}
	}

	return target;
}


//	FIXME: refactor the SHIT out of this - several "Skills" could be made from this single tactic
//			also note that this is a state machine - we should have one Skill/state and have a way to determine when what transitions should happen
void Tactics::Fullback::update() {
	if ( state() == ActionStateSettingUp ) {
		setState(ActionStateRunning);
	}


	if ( state() != ActionStateRunning ) return;



	if(blockRobot && !blockRobot->visible)
	{
		robot()->addText(QString("blockRobot Not visible!"));
		blockRobot = 0;
	}

	Geometry2d::Rect area(Geometry2d::Point(-Field_Width/2.f, Field_Length), Geometry2d::Point(Field_Width/2.f, 0));
	if(side == Right && (_objectives & AreaMarking))
		area.pt[0].x = 0;
	if(side == Left && (_objectives & AreaMarking))
		area.pt[1].x = 0;

	// State changes
	if(_subState == Marking)
	{
		if(area.contains(ball().pos))
			blockRobot = 0;
		else
			blockRobot = findRobotToBlock(area);
		// neither, defaults to blocking ball

		if(_objectives & AreaMarking)
		{
			if(!area.contains(ball().pos) && !blockRobot)
			{
				_subState = AreaMarking;
			}
		}
	}
	else if(_subState == AreaMarking)
	{
		if(_objectives & Marking)
		{
			if(area.contains(ball().pos))
			{
				_subState = Marking;
			}
			else
			{
				blockRobot = findRobotToBlock(area);
				if(blockRobot)
					_subState = Marking;
			}
		}
	}






	if(blockRobot)
		robot()->addText(QString("Blocking Robot %1").arg(blockRobot->shell()));

	if(_subState == AreaMarking)
		robot()->addText(QString("AreaMarking"));
	else if (_subState == Marking)
		robot()->addText(QString("Marking"));






	// Do not avoid opponents when planning while we are close to the goal
	if (robot()->pos.nearPoint(Geometry2d::Point(), *_opponent_avoid_threshold))
		robot()->avoidOpponents(false);
	else
		robot()->avoidOpponents(true);






	// Calculate windows
	_winEval.exclude.clear();
	_winEval.exclude.push_back(robot()->pos);

	Geometry2d::Point blockTargetFuture;

	if(_subState == Marking)
	{
		// we multiply by 0.3 here to look 0.3s into the future when considering the ball position
		// also, this will be used for where the robots will face
		if(blockRobot)
		{
			blockTargetFuture = blockRobot->pos + blockRobot->vel*0.3;
		}
		else if(!blockRobot)
		{
			blockTargetFuture = ball().pos + ball().vel*0.3;
		}

		//goal line, for intersection detection
		Geometry2d::Segment goalLine(Geometry2d::Point(-Field_GoalWidth / 2.0f, 0),
									 Geometry2d::Point(Field_GoalWidth / 2.0f, 0));

		//exclude robots that aren't the fullback
		BOOST_FOREACH(Fullback *f, _allFullbacks)	//	FIXME: what does the above comment mean?
		{
			if (f->robot() && f != this)
			{
				_winEval.exclude.push_back(f->robot()->pos);
			}
		}

		_winEval.run(blockTargetFuture, goalLine);
	}
	else if(_subState == AreaMarking)
	{
		Geometry2d::Point goalTarget(0, -Field_GoalDepth/2.f);

		//goal line, for intersection detection
		Geometry2d::Segment goalLine = Geometry2d::Segment(Geometry2d::Point(-Field_GoalWidth / 2.0f, 0),
														   Geometry2d::Point(Field_GoalWidth / 2.0f, 0));
		if(side == Left)
			goalLine.pt[1] = Geometry2d::Point(0,0);
		if(side == Right)
			goalLine.pt[0] = Geometry2d::Point(0,0);
		// Center defends entire area

		//exclude robots that are on the other team
		BOOST_FOREACH(OpponentRobot* opp, systemState()->opp)
		{
			if (opp)
				_winEval.exclude.push_back(opp->pos);
		}

		//exclude goalie
		if(gameplayModule()->goalie())
			_winEval.exclude.push_back(gameplayModule()->goalie()->robot()->pos);

		_winEval.run(goalLine, goalTarget, Field_Length/2.f);
	}


	// Pick best window
	Window* best = 0;
	
	if(_subState == Marking)
	{
		//pick biggest window on appropriate side
		Tactics::Goalie* goalie = gameplayModule()->goalie();
		if (goalie && goalie->robot() && side != Center)
		{
			BOOST_FOREACH(Window* window, _winEval.windows)
			{
				if (!best)
					best = window;
				else if (side == Left &&
						 window->segment.center().x < goalie->robot()->pos.x &&
						 window->segment.length() > best->segment.length()
						 )
					best = window;
				else if (side == Right &&
						 window->segment.center().x > goalie->robot()->pos.x &&
						 window->segment.length() > best->segment.length()
						 )
					best = window;
			}
		}
		else
		{
			//if no side parameter...stay in the middle
			float bestDist = 0;
			BOOST_FOREACH(Window* window, _winEval.windows)
			{
				Geometry2d::Segment seg(window->segment.center(), ball().pos);
				float newDist = seg.distTo(robot()->pos);

				if (!best || newDist < bestDist)
				{
					best = window;
					bestDist = newDist;
				}
			}
		}
	}
	else if(_subState == AreaMarking)
	{
		float angle;
		BOOST_FOREACH(Window* window, _winEval.windows)
		{
			if(!best){
				best = window;
				angle = window->a0 - window->a1;
			}
			else if (window->a0 - window->a1 > angle)
			{
				best = window;
				angle = window->a0 - window->a1;
			}
		}
	}
	

	// Find line of attack

	Geometry2d::Segment shootLine;
	if(best)
	{
		if(_subState == Marking)
		{
			if(blockRobot)
			{
				Geometry2d::Point dir = Geometry2d::Point::direction(blockRobot->angle * DegreesToRadians);
				shootLine = Geometry2d::Segment(blockRobot->pos, blockRobot->pos + dir* 7.0);
			}
			else if(!blockRobot)
			{
				shootLine = Geometry2d::Segment(ball().pos, ball().pos + ball().vel.normalized() * 7.0);
			}
		}
		else if(_subState == AreaMarking)
		{
			float angle = (best->a0 + best->a1) / 2.f;
			shootLine = Geometry2d::Segment(_winEval.origin(), Geometry2d::Point::direction(angle * DegreesToRadians));
			systemState()->drawLine(shootLine, QColor(255,0,0));
		}
	}

	// Driving

	bool needTask = false;
	if(best)
	{
		Geometry2d::Segment& winSeg = best->segment;
		
		if(_subState == Marking)
		{
			if (ball().vel.magsq() > 0.03 && winSeg.intersects(shootLine))
			{
				robot()->move(shootLine.nearestPoint(robot()->pos));
				robot()->faceNone();
			}
			else
			{
				const float winSize = winSeg.length();

				if (winSize < Ball_Radius)
				{
					needTask = true;
				}
				else
				{
					Geometry2d::Circle arc(Geometry2d::Point(), *_defend_goal_radius);
					Geometry2d::Line shot(winSeg.center(), blockTargetFuture);
					Geometry2d::Point dest[2];

					bool intersected = shot.intersects(arc, &dest[0], &dest[1]);

					if (intersected)
					{
						robot()->move(dest[0].y > 0 ? dest[0] : dest[1]);

						// Using regular pos rather than future because velocity approx on ball is not exact
						// enough and this leads to the robots turning backwards, towards the goal, when the
						// ball is shot at the goal.
						if(blockRobot)
							robot()->face(blockRobot->pos);
						else if(!blockRobot)
							robot()->face(ball().pos);
					}
					else
					{
						needTask = true;
					}
				}
			}
		}
		else if(_subState == AreaMarking)
		{
			Geometry2d::Circle arc(Geometry2d::Point(), *_defend_goal_radius);
			Geometry2d::Line shot(shootLine.pt[0],shootLine.pt[1]);
			Geometry2d::Point dest[2];

			bool intersected = shot.intersects(arc, &dest[0], &dest[1]);

			if (intersected)
			{
				robot()->move((dest[0].y > 0 ? dest[0] : dest[1]));
			}
			else
			{
				needTask = true;
			}
		}
	}
	else
	{
		needTask = true;
	}
	
	// if needTask, face the ball
	if(needTask)
	{
		robot()->face(ball().pos, true);
	}

	// Turn dribbler on when ball is near
	if(ball().pos.y < Field_Length / 2)
	{
		robot()->dribble(255);
	}

	// If ball sensor is tripped and we are not facing towards the goal, fire.
	// TODO: add chipping option for clearing the ball
	Geometry2d::Point backVec(1,0);
	Geometry2d::Point backPos(-Field_Width/2,0);
	Geometry2d::Point shotVec(ball().pos - robot()->pos);
	Geometry2d::Point shotPos(robot()->pos);
	Geometry2d::Point backVecRot(backVec.perpCCW());
	bool facingBackLine = (backVecRot.dot(shotVec) < 0);
	if(!facingBackLine)
	{
		if(robot()->chipper_available())
			robot()->chip(255);
		else
			robot()->kick(255);
	}
}












