
#pragma once

#include "../../STP.hpp"


namespace Tactics {

	class Goalie : public Tactic {
	public:
		Goalie(Gameplay::GameplayModule *gpModule) : Tactic(gpModule, false, true) {
			//	FIXME
		}


		~Goalie() {
			//	FIXME
		}


		void update() {
			//	FIXME:
		}


		virtual void parseParameters() {

		}


		virtual void setPreferencesForRole(Role *role) {
			Geometry2d::Point start;

			//	FIXME: calculate preferred start point

			role->setPreferredInitialPosition(start);
		}



		static RobotRequirements robotRequirements;

	private:


	};

}