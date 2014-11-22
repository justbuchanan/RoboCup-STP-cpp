
#pragma once

#include "../../STP.hpp"
#include "Configuration.hpp"
#include "../../gameplay/Window.hpp"


namespace Tactics {

	class Fullback : public Tactic {
	public:


		static void createConfiguration(Configuration *cfg);


		typedef enum {
			Left,
			Center,
			Right
		} Side;


		enum Objective {
			//defense states
			Marking = 1,
			AreaMarking = 2,
			MultiMark = 4,
			Intercept = 8,
			//offensive states
			Support = 16,
			Receiving = 32,
			Passing = 64
		};



		Fullback(Gameplay::GameplayModule *gpModule) :
			Tactic(gpModule, false, false),
			_winEval(systemState())
		{
			_subState = Marking;
			_winEval.debug = false;
			_objectives = Marking;
			side = Center;

			//	add to global Fullback registry
			_allFullbacks.push_back(this);
		}

		~Fullback() {
			//	remove from global Fullback registry
			_allFullbacks.erase( std::find(_allFullbacks.begin(), _allFullbacks.end(), this) );
		}

		virtual void update();



		virtual void parseParameters() {
			
		}

	
		//	we'd prefer to have a robot that's already close to the target point
		virtual void setPreferencesForRole(Role *role) {
			// role->setPreferredInitialPosition(target);
		}





		Side side;

		///	note: if blockRobot is set to NULL, the fullback will block the ball
		OpponentRobot *blockRobot;



		//	FIXME: global registry/set of other fullbacks



		static RobotRequirements robotRequirements;




	private:
		Gameplay::WindowEvaluator _winEval;
		int _objectives;
		Objective _subState;

		static ConfigDouble *_defend_goal_radius;
		static ConfigDouble *_opponent_avoid_threshold;

		OpponentRobot* findRobotToBlock(const Geometry2d::Rect& area);


		static std::vector<Fullback *> _allFullbacks;
	};
}
