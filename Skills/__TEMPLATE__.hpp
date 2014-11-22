
#pragma once

#include "../../STP.hpp"


namespace Skills {

	class __TEMPLATE__ : public Skill {
	public:

		__TEMPLATE__(Gameplay::GameplayModule *gameplayModule):
			Skill(gameplayModule, bool evaluatesSuccess, bool continuous)
		{
				//	TODO: init
		}

		virtual void update() {
			//	TODO: state management


			if ( state() == ActionStateRunning ) {
				OurRobot *r = robot();

				//	TODO: make the robot do something
			}


		}


	};

}
