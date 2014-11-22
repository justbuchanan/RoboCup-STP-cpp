
#pragma once

#include "../../STP.hpp"
#include "../Skills/Move.hpp"


namespace Tactics {

	class Move : public Tactic {
	public:
		Move(Gameplay::GameplayModule *gpModule) : Tactic(gpModule, false, false) {
			_move = NULL;
		}

		~Move() {
			if ( _move ) delete _move;
		}

		void update() {
			if ( state() == ActionStateSettingUp ) {
				_move = new Skills::Move(gameplayModule());
				_move->setRole(role());

				_move->target.x = target.x;
				_move->target.y = target.y;

				setState(ActionStateRunning);
			}


			_move->update();


			if ( ACTION_STATE_IS_DONE(_move->state()) ) {
				setState( _move->state() );
			}
		}



		virtual void parseParameters() {
			target.x = parameters().get<float>("target.x");
			target.y = parameters().get<float>("target.y");
		}

	
		//	we'd prefer to have a robot that's already close to the target point
		virtual void setPreferencesForRole(Role *role) {
			role->setPreferredInitialPosition(target);
		}



		static RobotRequirements robotRequirements;

	private:
		Geometry2d::Point target;

		Skills::Move *_move;
	};

}
