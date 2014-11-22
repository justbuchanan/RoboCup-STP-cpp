
#pragma once

#include "../../STP.hpp"
#include "../../Configuration.hpp"


namespace Skills {

	class Bump : public Skill {
	public:
		static void createConfiguration(Configuration *cfg);

		Bump(Gameplay::GameplayModule *gameplay);
		
		
		void restart();


		virtual void update();

		
		Geometry2d::Point target;
		
	private:
		enum
		{
			State_Setup,
			State_Charge,
			State_Done
		} _subState;

		static ConfigBool *_face_ball;
		static ConfigDouble *_drive_around_dist;
		static ConfigDouble *_setup_to_charge_thresh;
		static ConfigDouble *_escape_charge_thresh;
		static ConfigDouble *_setup_ball_avoid;
		static ConfigDouble *_bump_complete_dist;
		static ConfigDouble *_facing_thresh;
		static ConfigDouble *_accel_bias;

	};

}