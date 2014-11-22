
#include "Bump.hpp"
#include <stdio.h>

using namespace Geometry2d;

namespace Skills
{
	REGISTER_CONFIGURABLE(Bump)
}

ConfigBool   *Skills::Bump::_face_ball;
ConfigDouble *Skills::Bump::_drive_around_dist;
ConfigDouble *Skills::Bump::_setup_to_charge_thresh;
ConfigDouble *Skills::Bump::_escape_charge_thresh;
ConfigDouble *Skills::Bump::_setup_ball_avoid;
ConfigDouble *Skills::Bump::_bump_complete_dist;
ConfigDouble *Skills::Bump::_accel_bias;
ConfigDouble *Skills::Bump::_facing_thresh;

void Skills::Bump::createConfiguration(Configuration *cfg)
{
	_drive_around_dist = new ConfigDouble(cfg, "Bump/Drive Around Dist", 0.45);
	_face_ball = new ConfigBool(cfg, "Bump/Face Ball otherwise target", true);
	_setup_to_charge_thresh = new ConfigDouble(cfg, "Bump/Charge Thresh", 0.1);
	_escape_charge_thresh = new ConfigDouble(cfg, "Bump/Escape Charge Thresh", 0.1);
	_setup_ball_avoid = new ConfigDouble(cfg, "Bump/Setup Ball Avoid", 1.0);
	_bump_complete_dist = new ConfigDouble(cfg, "Bump/Bump Complete Distance", 0.5);
	_accel_bias = new ConfigDouble(cfg, "Bump/Accel Bias", 0.1);
	_facing_thresh = new ConfigDouble(cfg, "Bump/Facing Thresh - Deg", 10);
}

Skills::Bump::Bump(Gameplay::GameplayModule *gameplay) :
    Skill(gameplay)
{
	_subState = State_Setup;
	target = Point(0.0, Field_Length);
}

void Skills::Bump::restart()
{
	_subState = State_Setup;
}

void Skills::Bump::update()
{
	if ( state() == ActionStateSettingUp ) {
		setState(ActionStateRunning);
	}
	

	if ( state() == ActionStateRunning ) {

		Line targetLine(ball().pos, target);
		const Point dir = Point::direction(robot()->angle * DegreesToRadians);
		double facing_thresh = cos(*_facing_thresh * DegreesToRadians);
		double facing_err = dir.dot((target - ball().pos).normalized());
	//	robot->addText(QString("Err:%1,T:%2").arg(facing_err).arg(facing_thresh));

		// State changes
		if (_subState == State_Setup)
		{
			if (targetLine.distTo(robot()->pos) <= *_setup_to_charge_thresh &&
					targetLine.delta().dot(robot()->pos - ball().pos) <= -Robot_Radius &&
					facing_err >= facing_thresh)
			{
				_subState = State_Charge;
			}
		} else if (_subState == State_Charge)
		{
			if (Line(robot()->pos, target).distTo(ball().pos) > *_escape_charge_thresh)
			{
				// Ball is in a bad place
				_subState = State_Setup;
			}

			// FIXME: should finish at some point, but this condition is bad
	//		if (!robot->pos.nearPoint(ball().pos, *_bump_complete_dist))
	//		{
	//			_subState = State_Done;
	//		}
		}
		
		// Driving
		if (_subState == State_Setup)
		{
			// Move onto the line containing the ball and the_setup_ball_avoid target
			robot()->addText(QString("%1").arg(targetLine.delta().dot(robot()->pos - ball().pos)));
			Segment behind_line(ball().pos - targetLine.delta().normalized() * (*_drive_around_dist + Robot_Radius),
					ball().pos - targetLine.delta().normalized() * 5.0);
			if (targetLine.delta().dot(robot()->pos - ball().pos) > -Robot_Radius)
			{
				// We're very close to or in front of the ball
				robot()->addText("In front");
				robot()->avoidBall(*_setup_ball_avoid);
				robot()->move(ball().pos - targetLine.delta().normalized() * (*_drive_around_dist + Robot_Radius));
			} else {
				// We're behind the ball
				robot()->addText("Behind");
				robot()->avoidBall(*_setup_ball_avoid);
				robot()->move(behind_line.nearestPoint(robot()->pos));
				systemState()->drawLine(behind_line);
			}

			// face in a direction so that on impact, we aim at goal
			Point delta_facing = target - ball().pos;
			robot()->face(robot()->pos + delta_facing);

		} else if (_subState == State_Charge)
		{
			robot()->addText("Charge!");
			systemState()->drawLine(robot()->pos, target, Qt::white);
			systemState()->drawLine(ball().pos, target, Qt::white);

			Point ballToTarget = (target - ball().pos).normalized();
	//		Point robotToBall = (ball().pos - robot->pos).normalized();
			Point driveDirection = (ball().pos - ballToTarget * Robot_Radius) - robot()->pos;
			
			//We want to move in the direction of the target without path planning
			double speed =  robot()->vel.mag() + *_accel_bias; // enough of a bias to force it to accelerate
			robot()->worldVelocity(driveDirection.normalized() * speed);
			robot()->angularVelocity(0.0);
		} else {
			robot()->addText("Done");
			setState(ActionStateCompleted);
		}


	}
}



