
#pragma once

#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>

#include "Role.hpp"
#include "ValueTree.hpp"

#include <framework/SystemState.hpp>


#define STP_DEBUG 1



namespace Gameplay {
	class GameplayModule;
}



typedef enum {

	ActionStateSettingUp			= -3,	//	TODO: remove this? is it good for anything?

	ActionStateReady				= -2,	//	TODO: if we remove setting up, we should remove this too

	ActionStateRunning				= -1,

	///	The Action finished running, but it's waiting to see if its task succeeded or not
	///	note: this state only applies to Actions that have evaluatesSuccess set to true
	///	note: any Roles that the Action allocated should get released when it transitions to Evaluating
	ActionStateEvaluatingSuccess	= 0,

	///	The Action ended execution, but didn't accomplish its goal.  Maybe it
	///	was a pass that didn't work or maybe a goal shot that didn't make it in, etc.
	ActionStateFailed				= 1,

	///	The Action was told to end before its execution could complete
	///	note: this state only applies to Actions that have evaluatesSuccess set to true
	///	note: Actions that don't evaluateSuccess are put in the ActionStateCompleted state when terminated
	ActionStateCancelled			= 2,

	///	Either the Action evaluatesSuccess and was successful or 
	///	it doesn't evaluate success and has ended
	ActionStateCompleted			= 3
} ActionState;

#define ACTION_STATE_IS_DONE(state) ((state) > 0)
#define ACTION_STATE_IS_DONE_RUNNING(state) ((state) >= 0)



///	There are three main "levels of abstraction" of Actions
///	corresponding to our three "semi-concrete" subclasses of Action
typedef enum {
	ActionAbstractionLevelSkill		= 0,	///	single-robot action
	ActionAbstractionLevelTactic	= 1,	///	one or two -robot action
	ActionAbstractionLevelPlay		= 2		///	team-wide action
} ActionAbstractionLevel;



/**
 *	Provides an abstract superclass for Skills, Tactics, and Plays.
 *	These have a lot in common, so the common code goes here.
 *
 *	note: the GameplayModule will deallocate any Roles allocated by an Action once the Action is removed from gameplay
 */
class Action {
public:
	Action(Gameplay::GameplayModule *gameplayModule, bool evaluatesSuccess = false, bool continuous = false) {
		if ( !gameplayModule ) throw std::string("ERROR: attempt to construct Action with NULL gameplay module.");

		_gameplayModule = gameplayModule;


		_evaluatesSuccess = evaluatesSuccess;
		_continuous = continuous;
		_state = ActionStateSettingUp;
	}
	
	
	///	the destructor is virtual to ensure that subclasses' destructors are called
	///	the destructor should not do anything dealing with the GameplayModule
	virtual ~Action() {};
	
	
	///	call this method to see if the Action is a Skill, Tactic, or Play
	virtual ActionAbstractionLevel abstractionLevel() const = 0;


	///	main meat of an action
	///	gets get called repeatedly while the Action is live
	virtual void update() {};

	
	///	cancels the Action and stops it from running
	///	note: terminating an Action that doesn't evaluate success transitions it to the Completed state
	///	note: discrete Actions taht are terminated go to the Cancelled state if they weren't completed yet
	void terminate();

	
	ActionState state() const {
		return _state;
	};
	

	///	If true, this Action has a notion of success or failure.
	///	Some Actions just execute and that's it, others have a specific
	///	mission to accomplish.
	bool evaluatesSuccess() const {
		return _evaluatesSuccess;
	}
	

	bool continuous() const {
		return _continuous;
	}


	///	override this if you want to
	virtual void transition(ActionState from, ActionState to) {};


	///	note: throws an exception if the transition is invalid
	void setState(ActionState newState);
	

	bool stateTransitionIsValid(ActionState from, ActionState to);


	Gameplay::GameplayModule *gameplayModule() {
		return _gameplayModule;
	}

	// virtual std::string &name() {};




/////////	Convenience methods copied from old Behavior class
public:
	SystemState *systemState() const;

	
protected:

	// Convenience functions
	const Ball &ball() const;

	const GameState &gameState() const;

	OurRobot *self(int i) const;

	const OpponentRobot *opp(int i) const;

///////////////////////////////////
	
	
private:
	Gameplay::GameplayModule *_gameplayModule;

	ActionState _state;
	bool _evaluatesSuccess;
	bool _continuous;
};



//================================================================================



class PlayFactory;


///	http://en.wikipedia.org/wiki/Abstract_factory_pattern
class ActionFactory {
public:
	ActionFactory(std::string name, ActionAbstractionLevel abstractionLevel) : _name(name) {
		_abstractionLevel = abstractionLevel;

		registerFactory(this, abstractionLevel);
	}

	virtual ~ActionFactory() {}
	
	
	///	returns an instance of the particular Action subclass
	///	that this ActionFactory is responsible for vending
	virtual Action *create(Gameplay::GameplayModule *gameplayModule) const = 0;
	
	
	///	The name of the Action that this is a factory for
	std::string &name();
	
	
	///	Adds the given factory to the global registry of ActionFactorys
	static void registerFactory(ActionFactory *factory, ActionAbstractionLevel abstractionLevel);

	static ActionFactory *getRegisteredFactory(std::string & name, ActionAbstractionLevel abstractionLevel);

	static std::map<std::string, ActionFactory *> &factoriesForAbstractionLevel(ActionAbstractionLevel absLevel);

	static std::map<std::string, PlayFactory *> &playFactories() {
		return (std::map<std::string, PlayFactory *> &)factoriesForAbstractionLevel(ActionAbstractionLevelPlay);
	}

	
protected:
	static void _setupFactoryRegistryIfNecessary();

	
private:
	std::string _name;

	ActionAbstractionLevel _abstractionLevel;
	
	///	global set of ActionFactory registries.
	///	There are multiple registries (maps of {name : factory} pairs) - one for each ActionType
	///	[0] Skills?
	///	[1] Tactics
	///	[2]	Plays
	static std::vector< std::map<std::string, ActionFactory *> > _factoriesByLevel;
	static bool _factoryRegistryIsSetup;
};


///	The global ActionFactory registry must hold pointers to non-templated classes, so we
///	make a templated concrete subclass to do the actual work.
template<class T>
class ActionFactoryImpl : public ActionFactory {
public:
	ActionFactoryImpl(const std::string &name, ActionAbstractionLevel abstractionLevel) :
		ActionFactory(name, abstractionLevel) {}
	
	virtual Action *create(Gameplay::GameplayModule *gameplayModule) const {
		Action *a = new T(gameplayModule);
		return a;
	}
	
};



class TacticFactory : public ActionFactory {
public:
	TacticFactory(const std::string &name) : ActionFactory(name, ActionAbstractionLevelTactic) {}


	virtual RobotRequirements robotRequirements() const = 0;
};



template<class T>
class TacticFactoryImpl : public TacticFactory {
public:
	TacticFactoryImpl(const std::string &name) : TacticFactory(name) {}


	virtual Action *create(Gameplay::GameplayModule *gameplayModule) const {
		Action *t = new T(gameplayModule);
		return t;
	}


	///	Tactic subclasses MUST declare a static robotRequirements variable to make this work
	virtual RobotRequirements robotRequirements() const {
		return T::robotRequirements;
	}

};


///	use this macro to create an ActionFactory corresponding to the Action subclass specified by
///	klass and register it in the global registry for the specified ActionType
#define REGISTER_ACTION_CLASS(klass, abstractionLevel) \
static ActionFactoryImpl<klass> _factory_for_##klass(#klass, abstractionLevel);

//#define REGISTER_SKILL_CLASS(klass) REGISTER_ACTION_CLASS(klass, ActionTypeSkill);
#define REGISTER_TACTIC_CLASS(klass) \
static TacticFactoryImpl<klass> _factory_for_##klass(#klass);



//==============================================================================



///	Contains the code shared between Skill and Tactic
class SingleRobotAction : public Action {
public:
	SingleRobotAction(Gameplay::GameplayModule *gameplayModule, bool evaluatesSuccess = false, bool continuous = false)
	: Action(gameplayModule, evaluatesSuccess, continuous) {}

	boost::shared_ptr<Role> role() {
		return _role;
	}

	void setRole(boost::shared_ptr<Role> role) {
		_role = role;
	}

	///	looks up the OurRobot corresponding to _role in the GameplayModule
	OurRobot *robot();

private:
	boost::shared_ptr<Role> _role;
	// RobotRequirements _robotRequirements;
};



//==============================================================================



///	Low-level action that controls a single robot
class Skill : public SingleRobotAction {
public:
	Skill(Gameplay::GameplayModule *gameplayModule, bool evaluatesSuccess = false, bool continuous = false)
	: SingleRobotAction(gameplayModule, evaluatesSuccess, continuous) {}
	
	ActionAbstractionLevel abstractionLevel() const {
		return ActionAbstractionLevelSkill;
	}
	
};



//==============================================================================



///	Higher-level Action that controls a single robot
class Tactic : public SingleRobotAction {
public:
	Tactic(Gameplay::GameplayModule *gameplayModule, bool evaluatesSuccess = false, bool continuous = false)
	: SingleRobotAction(gameplayModule, evaluatesSuccess, continuous) {}
	
	ActionAbstractionLevel abstractionLevel() const {
		return ActionAbstractionLevelTactic;
	}

	virtual ~Tactic() {
		delete _parameters;
	}
	
	
	///	note: may throw an exception if the parameters set here are invalid
	///	note: this 
	void setParameters(ValueTree *vtree);


	ValueTree &parameters() {
		return *_parameters;
	}

	
	///	if the Tactic has a preferred initial location or something, it should set it on the role here.
	virtual void setPreferencesForRole(boost::shared_ptr<Role> role) {};


protected:

	///	note: no need to call this directly, it's called by setParameters()
	///	if there are any issues with the parameters, THROW AN EXCEPTION
	virtual void parseParameters() {}

	
private:
	ValueTree *_parameters;
};



//==============================================================================



//	FIXME: make a Tactic abstract subclass that's a state machine
class StateMachineTactic : public Tactic {
public:
private:
	// std::map< int, std::vector<Action *> > _actionsByState;

	int _tacticState;

	// bool **_tacticStateTransitionTable;
};



//==============================================================================



class TacticStub {
public:
	TacticStub(const std::string &name, ValueTree *invocationParameters = NULL) : _name(name) {
		_invocationParameters = invocationParameters;
	}

	Tactic *instantiate(Gameplay::GameplayModule *gameplayModule);


	std::string &name() {
		return _name;
	}


	ValueTree *invocationParameters() {
		return _invocationParameters;
	}

	TacticFactory *factory() {
		return (TacticFactory *)ActionFactory::getRegisteredFactory(name(), ActionAbstractionLevelTactic);
	}

private:
	std::string _name;
	ValueTree *_invocationParameters;
};



//==============================================================================



typedef std::vector<TacticStub *> TacticSequence;

class PlayFactory;

/**
 *	High-level Action that coordinates the execution of Tactics amongst
 *	multiple robots.
 *
 *	A Play object holds the internal state of the Play as it runs, but the logic
 *	for the Play is actually handled by the PlayFactory.
 *	This is why the Play holds a pointer back to the PlayFactory it came from.
 */
class Play : public Action {
public:

	Play(PlayFactory *playFactory, Gameplay::GameplayModule *gameplayModule);
	~Play();


	virtual ActionAbstractionLevel abstractionLevel() const {
		return ActionAbstractionLevelPlay;
	}


	virtual void update();


	std::string name();


	//	checks each of the action sequences that feed into this sync point to see if ALL of
	//	them can be considered to be completed.  if so, the sync point is "reachable" now
	bool syncPointAtIndexIsReachableNow(unsigned int syncPtIndex);

	PlayFactory *factory() {
		return _playFactory;
	}


protected:

	//	returns true if the tactic is completed OR it's running and continuous
	bool tacticCanBeConsideredCompleted(Tactic *t);


	//	advances the sequence to the next state
	//	TODO: describe special case behaviors
	void transitionSequenceAtIndex(int seqIndex);


	//	note: only call this if it is reachable
	void transitionToSyncPointAtIndex(int syncPtIndex);

	
	//	sequence indices of -1 indicate that the Role is coming from or going to purgatory
	bool transitionRole(boost::shared_ptr<Role> role, int currSeqIdx, int newSeqIdx);


	//	TODO: play that is assigned to defense and lets people score should return a different success code
	//		on defense AND offense, measure the amount of time the tactic/play ran before failing
	//	TODO: add begin/end timestamps to tactics and plays
	//			record the difference and record it as a key output parameter of the tactic and play classes


	///	returns false if one of the tactics awaiting results failed
	bool checkPendingTacticResults();


	boost::shared_ptr<Role> roleForTacticSequenceAtIndex(int sequenceIndex);


	int stateOfTacticSequenceAtIndex(int tacticSeqIdx) {
		return _sequenceStateByIndex[tacticSeqIdx];
	}

	TacticStub *tacticStubForStateForTacticSequenceAtIndex(int tacticSeqIdx, int state);


	void initializeIvars();



private:
	PlayFactory *_playFactory;

	std::vector<int> _unreachedSyncPoints;	//	contains the indices of the sync points we haven't reached yet


	//	the "state" of a sequence is just the index into the sequence that we're currently executing
	//	a state of -1 means it hasn't started yet, while a state >= sequence.length means the sequence has finished
	//	note: when a sequence has finished, but the sync point hasn't been reached yet,
	//		a "placeholder" tactic may be assigned to the role
	std::vector<int> _sequenceStateByIndex;	//	StateByIndex[tacticSequenceIndex]
	std::vector<Tactic *> _tacticsBySequenceIndex;

	std::vector<Tactic *> _tacticsAwaitingResults;


	bool _debugLogging;	//	if true, prints a bunch of garbage to stdout
};




//================================================================================


/**
 *	The PlayFactory has two important jobs: it creates Play objects on the fly and it houses the logic for each Play.
 *
 *	Plays hold their own internal state, but the PlayFactory is where the actual logic for the Play is kept.
 */
class PlayFactory : public ActionFactory {
public:
	PlayFactory(std::string &name, std::string category = std::string("")) : ActionFactory(name, ActionAbstractionLevelPlay) {
		_finalized = false;
		_enabled = true;
		_category = category;
	}

	virtual Action *create(Gameplay::GameplayModule *gameplayModule);
	
	
	//	FIXME: termination conditions
	//	FIXME: how do enemy Roles fit into this?????


	///	bigger scores are better
	///	return -1 to indicate that the play isn't applicable
	virtual float score(Gameplay::GameplayModule *gpModule) {
		return 5;
	}


	///	returns true if score() == -1
	bool applicable(Gameplay::GameplayModule *gpModule) {
		float s = score(gpModule);
		return s > -1.1 && s < -.9;
	}


	///	returns a "Halt" tactic
	//	TODO: explain what this is used for
	TacticStub *placeholderTacticStub();


	///	note: the PlayFactory assumes "ownership" of the sequence memory-wise
	void addTacticSequence(TacticSequence *sequence, std::string &roleName, std::string &startSyncPoint, std::string &endSyncPoint);


	TacticSequence *tacticSequenceAtIndex(int tacticSequenceIndex) {
		return _tacticSequences[tacticSequenceIndex];
	}

	//	"freezes" the PlayFactory and makes it immutable
	//	any attempts to modify the PlayFactory after finalize() will throw exceptions
	void finalize() {
		_finalized = true;
		updateRoleRequirements();
	}


	boost::shared_ptr<Role> roleForTacticSequenceAtIndex(int tacticSeqIdx) {
		return _rolesByTacticSequenceIndex[tacticSeqIdx];
	}

	boost::shared_ptr<Role> roleNamed(std::string &name);


	///	throws an exception if the Tactic Sequence isn't valid
	///	note: continuous tactics are only allowed to appear at the end of a sequence.
	void ensureTacticSequenceValidity(TacticSequence *ts);


	///	how many robots are used simultaneously during the play?
	int maxSimultaneousRobots() const {
		//	FIXME: implement

		return 5;
	}


	bool enabled() const {
		return _enabled;
	}

	void setEnabled(bool enabled) {
		_enabled = enabled;
	}


	std::string &category() {
		return _category;
	}

	void setCategory(std::string &category) {
		_category = category;
	}



protected:
	friend class Play;

	//	for each role, set the requirements so that the robot filling the role is
	//	physically able to execute the tactics assigned to it
	void updateRoleRequirements();


	//	note: if there's no sync point with this name, it will create it
	int indexForSyncPointNamed(std::string &name);


	void addSyncPointNamed(std::string &name);


	boost::shared_ptr<Role> addRoleNamed(std::string &name);


private:
	std::map<std::string, boost::shared_ptr<Role> > _rolesByName;
	std::vector<boost::shared_ptr<Role> > _rolesByTacticSequenceIndex;		//	_roles[tacticSequenceIndex] = Role *

	std::vector<std::string> _syncPointNames;
	std::vector<TacticSequence *> _tacticSequences;

	std::vector<std::vector<int> > _syncPointInputs;		//	_syncPointInputs[syncPointIndex] = [array containing the indexes of the action sequences]
	std::vector<std::vector<int> > _syncPointOutputs;	//	_syncPointOutputs[syncPointIndex] = [array containing the indexes of the action sequences]

	bool _finalized;

	bool _enabled;


	std::string _category;


	static TacticStub *_globalPlaceholderTacticStub;
};
