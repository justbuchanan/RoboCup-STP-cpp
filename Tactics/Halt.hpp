
#pragma once


#include "../../STP.hpp"


namespace Tactics {

	class Halt : public Tactic {
	public:
		Halt(Gameplay::GameplayModule *gpModule) : Tactic(gpModule, false, true) {}

		void update() {
			if ( state() == ActionStateSettingUp ) {
				setState(ActionStateRunning);
			}

			if ( state() == ActionStateRunning ) {
				robot()->stop();
			}
		}

		static RobotRequirements robotRequirements;
	};
}