
#include "STP.hpp"
#include "RoleManager.hpp"
#include "gameplay/GameplayModule.hpp"

#include <boost/make_shared.hpp>

#include <boost/foreach.hpp>

using namespace std;
using namespace Gameplay;
using namespace boost;



void Action::terminate() {
	if ( _evaluatesSuccess ) {
		if ( state() != ActionStateCompleted ) {
			setState(ActionStateCancelled);
		}
	} else {
		setState(ActionStateCompleted);
	}
}



void Action::setState(ActionState newState) {
	if ( _state != newState ) {

		//	ensure the transition is valid
		if ( !stateTransitionIsValid(_state, newState) ) {
			string msg = "ERROR: invalid ActionState transition.";
			throw msg;
		}


		ActionState oldState = _state;
		_state = newState;

		this->transition(oldState, newState);
	}
}



bool Action::stateTransitionIsValid(ActionState from, ActionState to) {
	return (to >= from) && (from <= 0);
}


/////////////	Old Behavior.hpp stuff

SystemState *Action::systemState() const
{
	return _gameplayModule->state();
}




// Convenience functions
const Ball &Action::ball() const
{
	return systemState()->ball;
}

const GameState &Action::gameState() const
{
	return systemState()->gameState;
}

OurRobot *Action::self(int i) const
{
	return systemState()->self[i];
}

const OpponentRobot *Action::opp(int i) const
{
	return systemState()->opp[i];
}


/////////////////////////////



//==============================================================================



vector< map<string, ActionFactory *> > ActionFactory::_factoriesByLevel;
bool ActionFactory::_factoryRegistryIsSetup = false;


string &ActionFactory::name() {
	return _name;
}


void ActionFactory::_setupFactoryRegistryIfNecessary() {
	//	setup the registry if necessary
	if ( !_factoryRegistryIsSetup ) {
		for ( int i = 0; i < 3; i++ ) {
			map<string, ActionFactory *> registry;
			_factoriesByLevel.push_back(registry);
		}

		_factoryRegistryIsSetup = true;
	}
}

void ActionFactory::registerFactory(ActionFactory *factory, ActionAbstractionLevel abstractionLevel) {
	_setupFactoryRegistryIfNecessary();
	
	//	register the factory!
	map<string, ActionFactory *> registry = _factoriesByLevel[abstractionLevel];
	registry[factory->name()] = factory;
}

ActionFactory *ActionFactory::getRegisteredFactory(string & name, ActionAbstractionLevel abstractionLevel) {
	_setupFactoryRegistryIfNecessary();

	map<string, ActionFactory *> registry = _factoriesByLevel[abstractionLevel];
	return registry[name];
}

map<string, ActionFactory *> &ActionFactory::factoriesForAbstractionLevel(ActionAbstractionLevel absLevel) {
	_setupFactoryRegistryIfNecessary();
	return _factoriesByLevel[absLevel];
}



//==============================================================================



OurRobot *SingleRobotAction::robot() {
	if ( _role ) {
		return gameplayModule()->roleManager()->getAssignedRobot(_role);
	}
	
	return NULL;
}



//==============================================================================



void Tactic::setParameters(ValueTree *vtree) {
	_parameters = vtree;
	this->parseParameters();
}



//==============================================================================



Tactic *TacticStub::instantiate(GameplayModule *gameplayModule) {
	ActionFactory *factory = ActionFactory::getRegisteredFactory(name(), ActionAbstractionLevelTactic);
	if ( !factory ) {
		std::string errMsg = "ERROR: Unable to find factory for tactic named '" + name() + "'.";
		throw errMsg;
	}

	Tactic *t = (Tactic *)factory->create(gameplayModule);
	t->setParameters(_invocationParameters);

	return t;
}



//==============================================================================



Play::Play(PlayFactory *playFactory, GameplayModule *gameplayModule)
			: Action(gameplayModule, true, false) {
	if ( !playFactory ) throw string("ERROR: attempt to construct Play with NULL playFactory");

	initializeIvars();
}



Play::~Play() {
	//	delete all active Tactics
	for ( int i = 0; i < _tacticsAwaitingResults.size(); i++ ) {
		Tactic *t = _tacticsAwaitingResults[i];
		delete t;
	}
	for ( int i = 0; i < _tacticsBySequenceIndex.size(); i++ ) {
		Tactic *t = _tacticsBySequenceIndex[i];
		if ( t ) delete t;
	}
}



string Play::name() {
	return _playFactory->name();
}



void Play::transitionSequenceAtIndex(int seqIndex) {
	TacticSequence *sequence = _playFactory->tacticSequenceAtIndex(seqIndex);
	int sequenceState = _sequenceStateByIndex[seqIndex];


	Tactic *tactic = _tacticsBySequenceIndex[seqIndex];
	ActionState tacticState = tactic->state();

	//	handle the old Tactic appropriately
	if ( tacticState == ActionStateEvaluatingSuccess ) {
		_tacticsAwaitingResults.push_back(tactic);
	} else {
		delete tactic;
	}


	TacticStub *newTacticStub = NULL;

	//	see if the Tactic that just finished was the last one in the sequence
	if ( sequenceState >= sequence->size() - 1 ) {
		newTacticStub = _playFactory->placeholderTacticStub();
	} else {
		newTacticStub = (*sequence)[sequenceState + 1];
	}

	_sequenceStateByIndex[seqIndex]++;	//	increment the state for this sequence


	//	instantiate the new Tactic and record it
	if ( newTacticStub ) {
		Tactic *newTactic = newTacticStub->instantiate(gameplayModule());
		_tacticsBySequenceIndex[seqIndex] = newTactic;
	} else {
		//	TODO: throw real exception?
		throw string("STP.cpp error???");
	}
}



TacticStub *Play::tacticStubForStateForTacticSequenceAtIndex(int tacticSeqIdx, int state) {
	TacticSequence *sequence = _playFactory->tacticSequenceAtIndex(tacticSeqIdx);
	
	if ( state < 0 ) throw string("ERROR: no tactic stub for subzero sequence state");


	if ( state >= sequence->size() ) {
		return _playFactory->placeholderTacticStub();
	}

	return (*sequence)[state];
}



void Play::update() {

	//	get an updated status for each of our tactics awaiting results
	//	if one of them failed, we have to abort
	if ( !checkPendingTacticResults() ) {
		//	aaahhh fuck, one of those damn tactics failed again...
		setState(ActionStateFailed);
		return;
	}


	///	call update() on each of the Tactics currently being run by this play
	///	if the Tactic changes state, handle it appropriately
	for ( int sequenceIndex = 0; sequenceIndex < _tacticsBySequenceIndex.size(); sequenceIndex++ ) {
		Tactic *t = _tacticsBySequenceIndex[sequenceIndex];
		if ( t ) {
			t->update();

			ActionState state = t->state();

			if ( state == ActionStateCompleted || state == ActionStateEvaluatingSuccess ) {
				transitionSequenceAtIndex(sequenceIndex);
			} else if ( state == ActionStateFailed ) {
				setState(ActionStateFailed);	//	the Tactic failed, so the Play failed...
				return;
			}
		}
	}


	//	loop through unreached sync points and see if there are any we can reach now
	std::vector<int>::iterator syncPtItr;
	for ( int i = 0; i < _unreachedSyncPoints.size(); ) {
		int syncPtIndex = _unreachedSyncPoints[i];
		if ( syncPointAtIndexIsReachableNow(syncPtIndex) ) {

			//	do the transition
			//	note: the transition function does the work of removing syncPtIndex from _unreachedSyncPoints for us
			transitionToSyncPointAtIndex(syncPtIndex);
		} else {
			i++;	//	only increment the index if this sync point doesn't get removed
		}
	}


	//	if there are no sync points left, that means we're at the end
	if ( _unreachedSyncPoints.size() == 0 ) {
		if ( _tacticsAwaitingResults.size() > 0 ) {
			setState(ActionStateEvaluatingSuccess);
		} else {
			setState(ActionStateCompleted);
		}
	}

}



bool Play::syncPointAtIndexIsReachableNow(unsigned int syncPtIndex) {
	
	//	inputs to the given sync point
	std::vector<int> &inputs = _playFactory->_syncPointInputs[syncPtIndex];

	//	see if they're all done.  if we find one that's not finished yet, return false
	BOOST_FOREACH(unsigned int seqIdx, inputs) {
		TacticSequence *seq = _playFactory->tacticSequenceAtIndex(seqIdx);
		unsigned int seqLen = seq->size();

		unsigned int currentStateForSeq = _sequenceStateByIndex[seqIdx];

		//	see if we're on or past the last tactic in this sequence
		if ( currentStateForSeq >= seqLen - 1 ) {
			Tactic *t = _tacticsBySequenceIndex[seqIdx];

			if ( t && !tacticCanBeConsideredCompleted(t) ) return false;
		}
	}

	return true;
}



//	returns true if the tactic is completed OR it's running and continuous
bool Play::tacticCanBeConsideredCompleted(Tactic *t) {
	if ( !t ) return true;

	return t->state() == ActionStateCompleted ||
			(t->state() == ActionStateRunning && t->continuous());
}


///	note: this method doesn't handle allocation/deallocation of the role - that's the job of transitionToSyncPointAtIndex()
bool Play::transitionRole(shared_ptr<Role> role, int currSeqIdx, int newSeqIdx) {
	if ( currSeqIdx == -1 && newSeqIdx == -1 )
		throw string("ERROR: roles shouldn't transition from null sequence to null sequence...");


	//	if the role wasn't in use before, we need to allocate a robot for it
	if ( currSeqIdx == -1 ) {

		///	allocate the Role
		///	note: if allocation fails, this will throw an exception and the Play will be aborted by the GameplayModule
		// roleManager->allocateRole(role);	//	this is old.  transitionToSyncPointAtIndex now handles this

	} else {
		_sequenceStateByIndex[currSeqIdx]++;	//	increment the sequence state to show that the sequence is done
		
		//	delete the tactic we were running before
		Tactic *t = _tacticsBySequenceIndex[currSeqIdx];
		_tacticsBySequenceIndex[currSeqIdx] = NULL;
		delete t;
	}


	//	if there's no next sequence, we can deallocate the Role
	if ( newSeqIdx == -1 ) {
		// roleManager->freeRole(role);	//	this is old.  transitionToSyncPointAtIndex now handles this
	} else {
		_sequenceStateByIndex[newSeqIdx] = 0;	//	we're just starting this sequence, so we're at state/index 0

		TacticSequence *newSeq = _playFactory->tacticSequenceAtIndex(newSeqIdx);
		TacticStub *stub = (*newSeq)[0];


		//	create the new Tactic and begin tracking it
		Tactic *t = stub->instantiate(gameplayModule());
		t->setRole(role);
		_tacticsBySequenceIndex[newSeqIdx] = t;


		//	update the preferences for the role in case it hasn't been allocated yet
		t->setPreferencesForRole(role);
	}

	return true;
}



void Play::transitionToSyncPointAtIndex(int syncPtIndex) {
	
	vector<int> inputs = _playFactory->_syncPointInputs[syncPtIndex];
	vector<int> outputs = _playFactory->_syncPointOutputs[syncPtIndex];

	set<shared_ptr<Role> > rolesToAllocate;	//	roles that weren't in the inputs, but are in the outputs


	vector<int> transitionedOutputs;	//	keep track of which outputs have been transitioned to already

	//	transition each of the inputs
	BOOST_FOREACH(int inputSeqIdx, inputs) {
		shared_ptr<Role> role = _playFactory->roleForTacticSequenceAtIndex(inputSeqIdx);

		//	find the index of the output sequence corresponding to this role (if any)
		int outputSeqIdx = -1;
		BOOST_FOREACH(int output, outputs) {
			shared_ptr<Role> roleForOutput = _playFactory->roleForTacticSequenceAtIndex(output);
			if ( roleForOutput == role ) {
				outputSeqIdx = output;
				break;
			}
		}


		//	do the transition
		//	note: it's ok to pass -1 as a sequence index
		transitionRole(role, inputSeqIdx, outputSeqIdx);


		if ( outputSeqIdx == -1 ) {	//	there's no next sequence for this Role, so deallocate it
			gameplayModule()->deallocateRoleForToplevelAction(this, role);
		} else {
			transitionedOutputs.push_back(outputSeqIdx);	//	record that this output sequence index has been handled
		}
	}

	//	transition all of the output sequences that didn't correspond to an input sequence
	BOOST_FOREACH(int outputSeqIdx, outputs) {
		//	if it hasn't been transitioned yet, do it!
		bool needsTransitioning = find(transitionedOutputs.begin(), transitionedOutputs.end(), outputSeqIdx) == transitionedOutputs.end();
		if ( needsTransitioning ) {
			shared_ptr<Role> role = _playFactory->roleForTacticSequenceAtIndex(outputSeqIdx);
			transitionRole(role, -1, outputSeqIdx);
			rolesToAllocate.insert(role);	//	add this to the set of Role's we need to allocate
		}
	}


	//	we allocate the new roles all at once at the end so that the role manager can find an optimal matching for us.
	//	if we instead allocated roles one at a time, the role -> robot matching wouldn't be optimal in most cases
	gameplayModule()->allocateRolesForToplevelAction(this, rolesToAllocate);


	//	mark that we've reached this sync point
	vector<int>::iterator thisSyncPtPos = find(_unreachedSyncPoints.begin(), _unreachedSyncPoints.end(), syncPtIndex);
	_unreachedSyncPoints.erase(thisSyncPtPos);


	#if STP_DEBUG
	cout << "Play '" << name() << "' transitioned sync pt '" << _playFactory->_syncPointNames[syncPtIndex] << "'" << endl;
	#endif
}



bool Play::checkPendingTacticResults() {

	//	iterate through each of the pending tactics
	for ( int pendingTacticIdx = 0; pendingTacticIdx < _tacticsAwaitingResults.size(); ) {
		Tactic *t = _tacticsAwaitingResults[pendingTacticIdx];
		ActionState state = t->state();
		if ( state != ActionStateEvaluatingSuccess ) {	//	see if its state has changed
			if ( state == ActionStateFailed ) {
				//	TODO: increment the negative count for this play && tactic
				return false;
			} else if ( state == ActionStateCompleted ) {
				//	TODO: increment the positive count for this play && tactic and record conditions - who we're against, etc.

				vector<Tactic *>::iterator rmIdx = _tacticsAwaitingResults.begin() + pendingTacticIdx;
				_tacticsAwaitingResults.erase(rmIdx);	//	TODO: instead of doing this, add it to the array of tactics/roles that are going away?

			} else {
				throw string("C++ Programmer ERROR: Invalid Play state transition from EvaluatingSuccess -> !{Failed, Completed}");
			}
		} else {
			pendingTacticIdx++;	//	only increment the index if we're not removing this tactic
		}
	}

	return true;
}



void Play::initializeIvars() {
	int sequenceCount = _playFactory->_tacticSequences.size();
	_sequenceStateByIndex.resize(sequenceCount, -1);		//	set all states to -1
	_tacticsBySequenceIndex.resize(sequenceCount, NULL);	//	empty set of Tactics

	//	populate _unreachedSyncPoint array
	int syncPtCount = _playFactory->_syncPointNames.size();
	for ( int i = 0; i < syncPtCount; i++ ) {
		_unreachedSyncPoints.push_back(i);
	}
}


shared_ptr<Role> Play::roleForTacticSequenceAtIndex(int sequenceIndex) {
	return _playFactory->roleForTacticSequenceAtIndex(sequenceIndex);
}



//================================================================================



Action *PlayFactory::create(GameplayModule *gameplayModule) {
	Play *play = new Play(this, gameplayModule);
	return play;
}



shared_ptr<Role> PlayFactory::roleNamed(std::string &name) {
	return _rolesByName[name];
}



void PlayFactory::addTacticSequence(TacticSequence *sequence, std::string &roleName, std::string &startSyncPoint, std::string &endSyncPoint) {
	if ( _finalized ) throw "ERROR: Attempt to mutate PlayFactory after it has been finalized";

	ensureTacticSequenceValidity(sequence);

	//	add the tactic sequence
	_tacticSequences.push_back(sequence);
	int tacticSeqIdx = _tacticSequences.size() - 1;


	//	get the role
	shared_ptr<Role> role = roleNamed(roleName);
	if ( !role ) {
		role = addRoleNamed(roleName);
	}

	_rolesByTacticSequenceIndex.push_back(role);

	//	get sync point indices
	int startSyncPtIdx = indexForSyncPointNamed(startSyncPoint);
	int endSyncPtIdx = indexForSyncPointNamed(endSyncPoint);

	//	add sequence to start point's outputs
	std::vector<int> &startSyncPtOutputs = _syncPointOutputs[startSyncPtIdx];
	startSyncPtOutputs.push_back(tacticSeqIdx);

	//	add sequence to end point's inputs
	std::vector<int> &endSyncPtInputs = _syncPointInputs[endSyncPtIdx];
	endSyncPtInputs.push_back(tacticSeqIdx);
}



void PlayFactory::ensureTacticSequenceValidity(TacticSequence *ts) {
	if ( !ts ) throw "ERROR: TacticSequence can't be NULL";
	if ( ts->size() == 0 ) throw "ERROR: TacticSequence can't be empty";

	//	ensure that continuous Tactics only appear at the end (otherwise they'll cause a halt)
	for ( int i = 0; i < ts->size() - 1; i++ ) {
		TacticStub *stub = (*ts)[i];

		//	TODO: throw exception if Tactic is continuous
	}
}



void PlayFactory::updateRoleRequirements() {
	//	iterate over all of the sequences
	for ( int seqIdx = 0; seqIdx < _rolesByTacticSequenceIndex.size(); seqIdx++ ) {
		TacticSequence *sequence = tacticSequenceAtIndex(seqIdx);
		shared_ptr<Role> role = roleForTacticSequenceAtIndex(seqIdx);
		
		//	add requirements from the tasks in the sequence into the requirements for the role.
		RobotRequirements reqs;
		for ( int i = 0; i < sequence->size(); i++ ) {
			TacticStub *t = (*sequence)[i];
			TacticFactory *f = t->factory();
			if ( f ) reqs = (RobotRequirements)(reqs | f->robotRequirements());
		}
		reqs = (RobotRequirements)(reqs | role->robotRequirements() );
		role->setRobotRequirements(reqs);
	}
}



void PlayFactory::addSyncPointNamed(string &name) {
	if ( _finalized ) throw string("ERROR: addSyncPoint() called on finalized PlayFactory");
	_syncPointNames.push_back(name);

	vector<int> empty;
	_syncPointInputs.push_back(empty);
	_syncPointOutputs.push_back(empty);
}



shared_ptr<Role> PlayFactory::addRoleNamed(std::string &name) {
	shared_ptr<Role> role = make_shared<Role>(name);
	_rolesByName[name] = role;
	return role;
}


int PlayFactory::indexForSyncPointNamed(std::string &name) {
	vector<string>::iterator itr = find(_syncPointNames.begin(), _syncPointNames.end(), name);
	if ( itr == _syncPointNames.end() ) {
		addSyncPointNamed(name);
		return _syncPointNames.size() - 1;
	} else {
		return itr - _syncPointNames.begin();
	}
}



TacticStub *PlayFactory::_globalPlaceholderTacticStub = NULL;

TacticStub *PlayFactory::placeholderTacticStub() {
	
	if ( !_globalPlaceholderTacticStub ) {
		_globalPlaceholderTacticStub = new TacticStub("Halt");
	}

	return _globalPlaceholderTacticStub;
}

