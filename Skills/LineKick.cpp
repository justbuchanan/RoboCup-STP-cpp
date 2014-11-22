
#include "LineKick.hpp"

#include <stdio.h>

using namespace std;
using namespace Geometry2d;

namespace Skills
{
	REGISTER_CONFIGURABLE(LineKick)
}

ConfigDouble *Skills::LineKick::_drive_around_dist;
ConfigDouble *Skills::LineKick::_setup_to_charge_thresh;
ConfigDouble *Skills::LineKick::_escape_charge_thresh;
ConfigDouble *Skills::LineKick::_setup_ball_avoid;
ConfigDouble *Skills::LineKick::_accel_bias;
ConfigDouble *Skills::LineKick::_facing_thresh;
ConfigDouble *Skills::LineKick::_max_speed;
ConfigDouble *Skills::LineKick::_proj_time;
ConfigDouble *Skills::LineKick::_dampening;
ConfigDouble *Skills::LineKick::_done_thresh;



void Skills::LineKick::createConfiguration(Configuration *cfg)
{
	_drive_around_dist = new ConfigDouble(cfg, "LineKick/Drive Around Dist", 0.25);
	_setup_to_charge_thresh = new ConfigDouble(cfg, "LineKick/Charge Thresh", 0.1);
	_escape_charge_thresh = new ConfigDouble(cfg, "LineKick/Escape Charge Thresh", 0.1);
	_setup_ball_avoid = new ConfigDouble(cfg, "LineKick/Setup Ball Avoid", Ball_Radius * 2.0);
	_accel_bias = new ConfigDouble(cfg, "LineKick/Accel Bias", 0.1);
	_facing_thresh = new ConfigDouble(cfg, "LineKick/Facing Thresh - Deg", 10);
	_max_speed = new ConfigDouble(cfg, "LineKick/Max Charge Speed", 1.5);
	_proj_time = new ConfigDouble(cfg, "LineKick/Ball Project Time", 0.4);
	_dampening = new ConfigDouble(cfg, "LineKick/Ball Project Dampening", 0.8);
	_done_thresh = new ConfigDouble(cfg, "LineKick/Done State Thresh", 0.11);
}

Skills::LineKick::LineKick(Gameplay::GameplayModule *gpModule) :
    Skill(gpModule, false, false),	//	FIXME: should it evaluate success?
    ballClose(false)
{
	restart();
	target = Geometry2d::Point(0.0, Field_Length);
	enable_kick = true;
}




void Skills::LineKick::restart()
{
	_subState = State_Setup;
	use_chipper = false;
	kick_power = 255;
	scaleAcc = 1.0;
	scaleSpeed = 1.0;
	scaleW = 1.0;
	ballClose = false;
	kick_ready = false;
}



void Skills::LineKick::update() {
	//	TODO: state management

	if ( state() == ActionStateSettingUp ) {
		setState(ActionStateRunning);
	}


	if ( state() == ActionStateRunning ) {
		OurRobot *theRobot = robot();


		// project the ball ahead to handle movement
		double dt = *_proj_time;
		Point ballPos = ball().pos + ball().vel * dt * _dampening->value();  // projecting
		//	Point ballPos = ball().pos; // no projecting
		Line targetLine(ballPos, target);
		const Point dir = Point::direction(theRobot->angle * DegreesToRadians);
		double facing_thresh = cos(*_facing_thresh * DegreesToRadians);
		double facing_err = dir.dot((target - ballPos).normalized());
		

		if(ballPos.distTo(theRobot->pos) <= *_done_thresh)
		{
			ballClose = true;
		}

		// State changes
		if (_subState == State_Setup)
		{
			if (targetLine.distTo(theRobot->pos) <= *_setup_to_charge_thresh &&
					targetLine.delta().dot(theRobot->pos - ballPos) <= -Robot_Radius &&
					facing_err >= facing_thresh &&
					theRobot->vel.mag() < 0.05)
			{
				if(enable_kick)
				{
					_subState = State_Charge;
				}
				kick_ready = true;

			}
			else
			{
				kick_ready = false;
			}

			//if the ball if further away than the back off distance for the setup stage
			if(ballClose && ballPos.distTo(theRobot->pos) > *_drive_around_dist + Robot_Radius)
			{
				_subState = State_Done;
			}
		} else if (_subState == State_Charge)
		{
			if (Line(theRobot->pos, target).distTo(ballPos) > *_escape_charge_thresh)
			{
				// Ball is in a bad place
				_subState = State_Setup;
			}

			//if the ball if further away than the back off distance for the setup stage
			if(ballClose && ballPos.distTo(theRobot->pos) > *_drive_around_dist + Robot_Radius)
			{
				_subState = State_Done;
			}
		}
		
		// Driving
		if (_subState == State_Setup)
		{
			// Move onto the line containing the ball and the_setup_ball_avoid target
			theRobot->addText(QString("%1").arg(targetLine.delta().dot(theRobot->pos - ballPos)));
			Point moveGoal = ballPos - targetLine.delta().normalized() * (*_drive_around_dist + Robot_Radius);

			static const Segment left_field_edge(Point(-Field_Width / 2.0, 0.0), Point(-Field_Width / 2.0, Field_Length));
			static const Segment right_field_edge(Point(Field_Width / 2.0, 0.0), Point(Field_Width / 2.0, Field_Length));

			// Handle edge of field case
			float field_edge_thresh = 0.3;
			Segment behind_line(ballPos - targetLine.delta().normalized() * (*_drive_around_dist),
					ballPos - targetLine.delta().normalized() * 1.0);
			systemState()->drawLine(behind_line);
			Point intersection;
			if (left_field_edge.nearPoint(ballPos, field_edge_thresh) && behind_line.intersects(left_field_edge, &intersection))   /// kick off left edge of far half fieldlPos, field_edge_thresh) && behind_line.intersects(left_field_edge, &intersection))
			{
				moveGoal = intersection;
			} else if (right_field_edge.nearPoint(ballPos, field_edge_thresh) && behind_line.intersects(right_field_edge, &intersection))
			{
				moveGoal = intersection;
			}

			theRobot->addText("Setup");
			theRobot->avoidBall(*_setup_ball_avoid);
			theRobot->move(moveGoal);

			// face in a direction so that on impact, we aim at goal
			Point delta_facing = target - ballPos;
			theRobot->face(theRobot->pos + delta_facing);

			theRobot->kick(0);

		} else if (_subState == State_Charge)
		{
			theRobot->addText("Charge!");
			if (use_chipper)
			{
				theRobot->chip(kick_power);
			} else
			{
				theRobot->kick(kick_power);
			}


			systemState()->drawLine(theRobot->pos, target, Qt::white);
			systemState()->drawLine(ballPos, target, Qt::white);
			Point ballToTarget = (target - ballPos).normalized();
			Point theRobotToBall = (ballPos - theRobot->pos).normalized();
			Point driveDirection = theRobotToBall;

			// Drive directly into the ball
			double speed = min(theRobot->vel.mag() + (*_accel_bias * scaleAcc), _max_speed->value()); // enough of a bias to force it to accelerate
			theRobot->worldVelocity(driveDirection.normalized() * speed);

			// scale everything to adjust precision
			theRobot->setWScale(scaleW);
			theRobot->setVScale(scaleSpeed);

			theRobot->face(ballPos);

		} else {
			setState(ActionStateCompleted);
		}

	}


}

