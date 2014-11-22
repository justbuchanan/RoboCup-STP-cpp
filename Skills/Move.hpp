
#pragma once


#include "../../STP.hpp"



namespace Skills {

	class Move : public Skill {
	public:

		Move(Gameplay::GameplayModule *gameplayModule):
			Skill(gameplayModule, false, false)
		{
			stopAtEnd = true;
		}


		bool isTargetReached() {
			const float threshold = .1;	//	5cm
			float dist = (robot()->pos - target).mag();

			return dist < threshold;
		}


		virtual void update() {
			if ( state() == ActionStateSettingUp ) {
				setState(ActionStateRunning);
			} 

			if ( state() == ActionStateRunning ) {
				OurRobot *r = robot();

				r->move(target - (target - r->pos).normalized() * backoff, stopAtEnd);
				r->face(face);

				if ( isTargetReached() ) {
					setState(ActionStateCompleted);
				}
			}
		}


		Geometry2d::Point target;
		Geometry2d::Point face;
		float backoff;
		bool stopAtEnd;
	};
}
