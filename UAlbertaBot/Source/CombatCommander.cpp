#include "CombatCommander.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

const size_t IdlePriority = 0;
const size_t AttackPriority = 1;
const size_t BaseDefensePriority = 2;
const size_t ScoutDefensePriority = 3;
const size_t DropPriority = 4;

CombatCommander::CombatCommander()
	: _initialized(false), _tanksReady(false), _vulturesReady(false), _scvsReady(false), _airAttackReady(false), _vesselsReady(false),
	_attackMult(1), _chokeDefense(false)
{

}

void CombatCommander::initializeSquads()
{
	SquadOrder idleOrder(SquadOrderTypes::Idle, BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()), 100, "Chill Out");
	_squadData.addSquad("Idle", Squad("Idle", idleOrder, IdlePriority));

	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	// the scout defense squad will handle chasing the enemy worker scout
	SquadOrder enemyScoutDefense(SquadOrderTypes::Defend, ourBasePosition, 900, "Get the scout");
	_squadData.addSquad("ScoutDefense", Squad("ScoutDefense", enemyScoutDefense, ScoutDefensePriority));

	// add a drop squad if we are using a drop strategy
	if (Config::Strategy::StrategyName == "Protoss_Drop")
	{
		SquadOrder zealotDrop(SquadOrderTypes::Drop, ourBasePosition, 900, "Wait for transport");
		_squadData.addSquad("Drop", Squad("Drop", zealotDrop, DropPriority));
	}

	// make our own squad using Terran Defense strategy
	if (Config::Strategy::StrategyName == "Terran_DefBot")
	{
		//SquadOrder tankTurtling(SquadOrderTypes::Regroup, ourBasePosition, 900, "Getting ready");
		//_squadData.addSquad("Turtle", Squad("Turtle", tankTurtling, TurtlePriority)); // note - figure out the priority thing

		SquadOrder vultureSquad(SquadOrderTypes::Regroup, ourBasePosition, 900, "Vultures not ready");
		SquadOrder tankSquad(SquadOrderTypes::Regroup, ourBasePosition, 900, "Tanks not ready");
		SquadOrder scvSquad(SquadOrderTypes::Regroup, ourBasePosition, 900, "SCVs not ready");
		SquadOrder airAttackSquad(SquadOrderTypes::Regroup, ourBasePosition, 900, "Air attackers not ready");
		SquadOrder vesselSquad(SquadOrderTypes::Regroup, ourBasePosition, 900, "Vessels not ready");

		const size_t priority = AttackPriority; // This is here for quickly changing priority during testing
		_squadData.addSquad("Vultures", Squad("Vultures", vultureSquad, priority));
		_squadData.addSquad("Tanks", Squad("Tanks", tankSquad, priority));
		_squadData.addSquad("SCVs", Squad("SCVs", scvSquad, priority));
		_squadData.addSquad("AirAttackers", Squad("AirAttackers", airAttackSquad, priority));
		_squadData.addSquad("Vessels", Squad("Vessels", vesselSquad, priority));
	}
	else{
		// the main attack squad that will pressure the enemy's closest base location
		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		_squadData.addSquad("MainAttack", Squad("MainAttack", mainAttackOrder, AttackPriority));
	}

	_initialized = true;
}

bool CombatCommander::isSquadUpdateFrame()
{
	return BWAPI::Broodwar->getFrameCount() % 10 == 0;
}

// get number of units of a particular type
int CombatCommander::getNumType(BWAPI::Unitset & units, BWAPI::UnitType type)
{
	int num = 0;

	// if type is SCV, only count number of combat SCVs
	if (type == BWAPI::UnitTypes::Terran_SCV)
	{
		for (auto & unit : units)
		{
			if (unit->getType() == type && WorkerManager::Instance().isWorkerCombat(unit))
			{
				num++;
			}
		}
	}
	else
	{
		for (auto & unit : units)
		{
			if (unit->getType() == type)
			{
				num++;
			}
		}
	}

	return num;
}

void CombatCommander::update(const BWAPI::Unitset & combatUnits)
{
	if (!Config::Modules::UsingCombatCommander)
	{
		return;
	}

	if (!_initialized)
	{
		initializeSquads();
	}

	_combatUnits = combatUnits;


	if (isSquadUpdateFrame())
	{
		//BWAPI::Broodwar->printf("%d", _attackMult);

		updateIdleSquad();
		updateDropSquads();
		updateScoutDefenseSquad();
		updateDefenseSquads();
		if (Config::Strategy::StrategyName == "Terran_DefBot")
		{
			updateTankSquads();
			updateVultureSquads();
			updateSCVSquads();
			updateAirAttackSquads();
			updateVesselSquads();
		}
		else
		{
			updateAttackSquads();
		}
	}

	_squadData.update();
}

void CombatCommander::updateIdleSquad()
{
	Squad & idleSquad = _squadData.getSquad("Idle");
	for (auto & unit : _combatUnits)
	{
		// if it hasn't been assigned to a squad yet, put it in the low priority idle squad
		if (_squadData.canAssignUnitToSquad(unit, idleSquad))
		{
			idleSquad.addUnit(unit);
		}
	}
}

void CombatCommander::updateAttackSquads()
{
	// It's fine if we put units into the main attack squad, but how do we make units from
	// the attack squad act as a single Unitset instead? Where do we do the attacking?
	Squad & mainAttackSquad = _squadData.getSquad("MainAttack");

	for (auto & unit : _combatUnits)
	{
		// some zerg code?
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge && UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Zerg_Hydralisk) < 30)
		{
			continue;
		}

		// get every unit of a lower priority and put it into the attack squad
		if (!unit->getType().isWorker() && (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord) && _squadData.canAssignUnitToSquad(unit, mainAttackSquad))
		{
			_squadData.assignUnitToSquad(unit, mainAttackSquad);
		}
	}

	BWAPI::Unitset & mainAttackSquadUnits = const_cast<BWAPI::Unitset &>(mainAttackSquad.getUnits());

	int count = getNumType(mainAttackSquadUnits, BWAPI::UnitTypes::AllUnits);

	// If we have less than n units, regroup, else start attacking.
	int n = 25;
	if (count > n) {
		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		mainAttackSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, mainAttackSquad.calcRegroupPosition(), 800, "Low Numbers Regroup");
		mainAttackSquad.setSquadOrder(mainAttackRetreat);
	}

}

/* Ratio for units while attacking:
* 1 SCV for every 3 Tanks
* 1 Vulture for every 1 Tank
* 1 Goliath per squad unless we are fighting Zerg, then have 1 Goliath for every 1 Tank.
* Initialize a flag for each squad to signal when ready
*/

void CombatCommander::updateTankSquads()
{
	Squad & tankSquad = _squadData.getSquad("Tanks");
	BWAPI::UnitType type1 = BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode;
	BWAPI::UnitType type2 = BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode;

	size_t tankCount = UnitUtil::GetAllUnitCount(type1) + UnitUtil::GetAllUnitCount(type2);

	if (tankCount >= 2 * _attackMult || BWAPI::Broodwar->self()->minerals() >= 4000) {
		_tanksReady = true;
	}
	else
	{
		// add to our next attack if our original attack failed -> controlled by number of tanks we have
		if (_tanksReady && _airAttackReady && _vulturesReady && _scvsReady && _vesselsReady && tankCount < _attackMult) {
			_attackMult++;
		}
		_tanksReady = false;
		return;
	}


	if (_tanksReady && _vulturesReady && _scvsReady && _airAttackReady && _vesselsReady)
	{
		for (auto & unit : _combatUnits)
		{
			if ((unit->getType() == type1 ||
				unit->getType() == type2) &&
				_squadData.canAssignUnitToSquad(unit, tankSquad)) {

				_squadData.assignUnitToSquad(unit, tankSquad);
			}
		}

		// get count of tanks added to tankSquad, useful for future releases
		// BWAPI::Unitset & tankSquadUnits = const_cast<BWAPI::Unitset &>(tankSquad.getUnits());
		// int tankCount = getNumType(tankSquadUnits, BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
		// 	+ getNumType(tankSquadUnits, BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);

		// If we have less than n units, regroup, else tanks are ready.
		// Use something else to determine this... Use simulation to see if we can win before attack

		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		tankSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, ourBasePosition, 800, "Low Numbers Regroup");
		tankSquad.setSquadOrder(mainAttackRetreat);
	}

	// Defend the bases
	// Prepare an attack using extra tanks once bases are protected
}

void CombatCommander::updateVultureSquads()
{
	Squad & vultureSquad = _squadData.getSquad("Vultures");
	BWAPI::UnitType type = BWAPI::UnitTypes::Terran_Vulture;

	size_t vultureCount = UnitUtil::GetAllUnitCount(type);

	if (vultureCount >= 2 * (_attackMult + 1) || BWAPI::Broodwar->self()->minerals() >= 4000) {
		_vulturesReady = true;
	}
	else
	{
		// add to our next attack if our original attack failed -> controlled by number of tanks we have
		if (_tanksReady && _airAttackReady && _vulturesReady && _scvsReady && _vesselsReady && vultureCount < (_attackMult + 1)) {
			_attackMult++;
		}
		_vulturesReady = false;
		return;
	}


	if (_tanksReady && _vulturesReady && _scvsReady && _airAttackReady && _vesselsReady)
	{
		for (auto & unit : _combatUnits)
		{
			if ((unit->getType() == type) &&
				_squadData.canAssignUnitToSquad(unit, vultureSquad)) {

				_squadData.assignUnitToSquad(unit, vultureSquad);
			}
		}
		//}

		// attempt to get squad to group together before attacking, prioritized other features.
		//bool grouped = true;
		/*BWAPI::Position pos = vultureSquad.calcRegroupPosition();
		for (auto & unit : vultureSquad.getUnits()) {
		if (unit->getDistance(pos) > 800) {
		grouped = false;
		}
		}*/

		//if (grouped)
		//{
		// get count of vultures added to vultureSquad, useful for future releases
		// BWAPI::Unitset & vultureSquadUnits = const_cast<BWAPI::Unitset &>(vultureSquad.getUnits());
		// int vultureCount = getNumType(vultureSquadUnits, BWAPI::UnitTypes::Terran_Vulture);

		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		vultureSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, ourBasePosition, 800, "Low Numbers Regroup");
		vultureSquad.setSquadOrder(mainAttackRetreat);
	}

	// if tanks are leapfrogging, scout ahead so tanks can attack early
	// else? sit and wait? defend? attack? what?
}

void CombatCommander::updateSCVSquads()
{
	Squad & scvSquad = _squadData.getSquad("SCVs");
	Squad & tankSquad = _squadData.getSquad("Tanks");
	Squad & vultureSquad = _squadData.getSquad("Vultures");
	Squad & airAttackSquad = _squadData.getSquad("AirAttackers");

	BWAPI::UnitType type = BWAPI::UnitTypes::Terran_SCV;

	size_t scvCount = UnitUtil::GetAllUnitCount(type);

	if (scvCount > 0)
	{
		_scvsReady = true;
	}
	else
	{
		_scvsReady = false;
	}

	size_t tankCount = tankSquad.getUnits().size();
	size_t vultureCount = vultureSquad.getUnits().size();
	size_t airAttackerCount = airAttackSquad.getUnits().size();
	size_t repairCount = (tankCount + vultureCount + airAttackerCount) / 3;

	if (_tanksReady && _vulturesReady && _scvsReady && _airAttackReady && _vesselsReady)
	{
		for (auto & unit : _combatUnits)
		{
			// if statements - repair count for proportions, max 10
			if (unit->getType() == type
				&& _squadData.canAssignUnitToSquad(unit, scvSquad)
				&& scvSquad.getUnits().size() < repairCount
				&& scvSquad.getUnits().size() < 5)
			{
				// if it is a combat worker
				WorkerManager::Instance().setCombatWorker(unit);
				_squadData.assignUnitToSquad(unit, scvSquad);
			}
		}

		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		scvSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

		// TODO: release workers back to worker manager instead of having them regroup and wait.
		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, ourBasePosition, 800, "Low Numbers Regroup");
		scvSquad.setSquadOrder(mainAttackRetreat);

		WorkerManager::Instance().finishedWithCombatWorkers();
	}

	// get SCVs already assigned to scvSquad
	BWAPI::Unitset & scvSquadUnits = const_cast<BWAPI::Unitset &>(scvSquad.getUnits());
	// size_t scvCount = getNumType(scvSquadUnits, BWAPI::UnitTypes::Terran_SCV);

	// If a defending tank is not at full health, heal the tank
	// If a group of tanks is leapfrogging, follow to heal the tanks as long as tankSquad and scvSquad are together
	BWAPI::Unitset tanksNeedRepair;

	for (auto & tank : _squadData.getSquad("Tanks").getUnits())
	{
		if ((tank->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
			|| tank->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
			&& tank->getHitPoints() < tank->getType().maxHitPoints())
		{
			tanksNeedRepair.insert(tank);
		}
	}

	BWAPI::Unitset vulturesNeedRepair;

	for (auto & vulture : _squadData.getSquad("Vultures").getUnits())
	{
		if (vulture->getType() == BWAPI::UnitTypes::Terran_Vulture
			&& vulture->getHitPoints() < vulture->getType().maxHitPoints())
		{
			vulturesNeedRepair.insert(vulture);
		}
	}

	if (!scvSquad.isEmpty() && tanksNeedRepair.size() > 0)
	{
		for (auto & tank : tanksNeedRepair)
		{
			BWAPI::Unit repairWorker = findClosestWorkerToTarget(scvSquadUnits, tank);
			if (tank && repairWorker)
			{
				WorkerManager::Instance().setRepairWorker(repairWorker, tank);
				// a worker is set to repair a tank, erase this tank from tanksNeedRepair
				if (tanksNeedRepair.find(tank) != tanksNeedRepair.end())
				{
					tanksNeedRepair.erase(tanksNeedRepair.find(tank));
				}
			}
		}
	}
	else if (!scvSquad.isEmpty() && tanksNeedRepair.size() == 0 && vulturesNeedRepair.size() > 0)
	{
		for (auto & vulture : vulturesNeedRepair)
		{
			BWAPI::Unit repairWorker = findClosestWorkerToTarget(scvSquadUnits, vulture);
			if (vulture && repairWorker)
			{
				WorkerManager::Instance().setRepairWorker(repairWorker, vulture);
				// a worker is set to repair a vulture, erase this vulture from vulturesNeedRepair
				if (vulturesNeedRepair.find(vulture) != vulturesNeedRepair.end())
				{
					vulturesNeedRepair.erase(vulturesNeedRepair.find(vulture));
				}
			}
		}
	}
	else if (!scvSquad.isEmpty() && tanksNeedRepair.size() == 0 && vulturesNeedRepair.size() == 0)
	{
		for (auto & worker : scvSquadUnits)
		{
			if (worker)
			{
				WorkerManager::Instance().finishedWithRepairCombatWorker(worker);
			}
		}
	}

	// clear tanksNeedRepair and vulturesNeedRepair since they need to be re-calculated for the next update
	tanksNeedRepair.clear();
	vulturesNeedRepair.clear();
}

void CombatCommander::updateAirAttackSquads()
{
	// Note: the amount of Goliaths we have will be dependent on the race of our enemy (more if Zerg)
	Squad & airAttackSquad = _squadData.getSquad("AirAttackers");

	BWAPI::UnitType type1 = BWAPI::UnitTypes::Terran_Goliath;
	BWAPI::UnitType type2 = BWAPI::UnitTypes::Terran_Battlecruiser;

	size_t airAttackCount = UnitUtil::GetAllUnitCount(type1) + UnitUtil::GetAllUnitCount(type2);

	if (airAttackCount >= 2 * _attackMult || BWAPI::Broodwar->self()->minerals() >= 4000) {
		_airAttackReady = true;
	}
	else
	{
		// add to our next attack if our original attack failed
		if (_tanksReady && _airAttackReady && _vulturesReady && _scvsReady && _vesselsReady && airAttackCount < _attackMult) {
			_attackMult++;
		}
		_airAttackReady = false;
		return;
	}

	if (_tanksReady && _vulturesReady && _scvsReady && _airAttackReady && _vesselsReady)
	{
		for (auto & unit : _combatUnits)
		{
			if ((unit->getType() == type1 || unit->getType() == type2) &&
				_squadData.canAssignUnitToSquad(unit, airAttackSquad)) {

				_squadData.assignUnitToSquad(unit, airAttackSquad);
			}
		}

		SquadOrder mainAttackOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
		airAttackSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, ourBasePosition, 800, "Low Numbers Regroup");
		airAttackSquad.setSquadOrder(mainAttackRetreat);
	}
	// Defend turtled tanks against air units
	// Defend leapfrogging tanks against air units
}

void CombatCommander::updateVesselSquads()
{
	Squad & vesselSquad = _squadData.getSquad("Vessels");
	BWAPI::UnitType type = BWAPI::UnitTypes::Terran_Science_Vessel;

	size_t vesselCount = UnitUtil::GetAllUnitCount(type);

	// assuming we only ever build one vessel, once we've built one we never build one again, never reset to false
	// that way if we are attacked and vessel is destroyed, we won't wait for one to never be rebuilt.
	if (vesselCount >= 0) {
		_vesselsReady = true;
	}

	if (_tanksReady && _vulturesReady && _scvsReady && _airAttackReady && _vesselsReady)
	{
		for (auto & unit : _combatUnits)
		{
			if ((unit->getType() == type) &&
				_squadData.canAssignUnitToSquad(unit, vesselSquad)) {

				_squadData.assignUnitToSquad(unit, vesselSquad);
			}
		}

		SquadOrder mainAttackOrder(SquadOrderTypes::Regroup, getMainAttackLocation(), 800, "Attack Enemy Base");
		vesselSquad.setSquadOrder(mainAttackOrder);
	}
	else
	{
		BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

		SquadOrder mainAttackRetreat(SquadOrderTypes::Regroup, ourBasePosition, 800, "Low Numbers Regroup");
		vesselSquad.setSquadOrder(mainAttackRetreat);
	}

	// Detector units
	// Want them in with the defense and attackers
}

// Idea for TurtleSquads, not exactly what we want.
/*void CombatCommander::updateTurtleSquads()
{
if (Config::Strategy::StrategyName != "Terran_DefBot")
{
return;
}

Squad & turtleSquad = _squadData.getSquad("Turtle");

bool hasTanks = false;
bool hasVultures = false;
bool hasSCVs = false;

int multiple; // use this to determine a ratio of how many units we need to go against our enemies.
// 3 tanks, 1 SCV, 3 Vultures, Goliaths depend on Zerg enemy or not, go for 1 for now.

auto & turtleUnits = turtleSquad.getUnits();

// figure out what we need
for (auto & unit : turtleUnits)
{
if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
{
hasTanks = true;
}
else if (unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
{
hasVultures = true;
}
else if (unit->getType() == BWAPI::UnitTypes::Terran_SCV)
{
hasSCVs = true;
}
}

// if we have the units, add them to the squad
if (!hasTanks || !hasVultures) // || !hasSCVs) // check if SCV adding works later
{
for (auto & unit : _combatUnits)
{
if (!hasTanks && (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
{
_squadData.assignUnitToSquad(unit, turtleSquad);
}
else if (!hasVultures && unit->getType() == BWAPI::UnitTypes::Terran_Vulture)
{
_squadData.assignUnitToSquad(unit, turtleSquad);
}
// do something different for SCVs? if isWorker && is Terran_SCV then assign to squad?

// get every unit of a lower priority and put it into the attack squad
//if (!unit->getType().isWorker() && _squadData.canAssignUnitToSquad(unit, turtleSquad))
//{
//	_squadData.assignUnitToSquad(unit, turtleSquad);
//}
}
}
// otherwise we are ready? Attack? Do this again for a defense squad, give it a higher priority?
// also decide more precisely when we want to attack, for now just do it.
else
{
SquadOrder turtleOrder(SquadOrderTypes::Attack, getMainAttackLocation(), 800, "Attack Enemy Base");
turtleSquad.setSquadOrder(turtleOrder);
}
}*/

void CombatCommander::updateDropSquads()
{
	if (Config::Strategy::StrategyName != "Protoss_Drop")
	{
		return;
	}

	Squad & dropSquad = _squadData.getSquad("Drop");

	// figure out how many units the drop squad needs
	bool dropSquadHasTransport = false;
	int transportSpotsRemaining = 8;
	auto & dropUnits = dropSquad.getUnits();

	for (auto & unit : dropUnits)
	{
		if (unit->isFlying() && unit->getType().spaceProvided() > 0)
		{
			dropSquadHasTransport = true;
		}
		else
		{
			transportSpotsRemaining -= unit->getType().spaceRequired();
		}
	}

	// if there are still units to be added to the drop squad, do it
	if (transportSpotsRemaining > 0 || !dropSquadHasTransport)
	{
		// take our first amount of combat units that fill up a transport and add them to the drop squad
		for (auto & unit : _combatUnits)
		{
			// if this is a transport unit and we don't have one in the squad yet, add it
			if (!dropSquadHasTransport && (unit->getType().spaceProvided() > 0 && unit->isFlying()))
			{
				_squadData.assignUnitToSquad(unit, dropSquad);
				dropSquadHasTransport = true;
				continue;
			}

			if (unit->getType().spaceRequired() > transportSpotsRemaining)
			{
				continue;
			}

			// get every unit of a lower priority and put it into the attack squad
			if (!unit->getType().isWorker() && _squadData.canAssignUnitToSquad(unit, dropSquad))
			{
				_squadData.assignUnitToSquad(unit, dropSquad);
				transportSpotsRemaining -= unit->getType().spaceRequired();
			}
		}
	}
	// otherwise the drop squad is full, so execute the order
	else
	{
		SquadOrder dropOrder(SquadOrderTypes::Drop, getMainAttackLocation(), 800, "Attack Enemy Base");
		dropSquad.setSquadOrder(dropOrder);
	}
}

void CombatCommander::updateScoutDefenseSquad()
{
	if (_combatUnits.empty())
	{
		return;
	}

	// if the current squad has units in it then we can ignore this
	Squad & scoutDefenseSquad = _squadData.getSquad("ScoutDefense");

	// get the region that our base is located in -- replaced by code block below
	// BWTA::Region * myRegion = BWTA::getRegion(BWAPI::Broodwar->self()->getStartLocation());
	// if (!myRegion && myRegion->getCenter().isValid())
	// {
	//     return;
	// }

	// get the regions that mineral fields are located in
	std::set<BWTA::Region *> mineralRegion;
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().isMineralField())
		{
			BWTA::Region * mineRegion = BWTA::getRegion(BWAPI::TilePosition(unit->getPosition()));
			if (!mineRegion && mineRegion->getCenter().isValid())
			{
				return;
			}
			mineralRegion.insert(mineRegion);
		}
	}

	// get all of the enemy units in this region -- replaced by code block below
	// BWAPI::Unitset enemyUnitsInRegion;
	// for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	// {
	// 	if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
	// 	{
	// 		enemyUnitsInRegion.insert(unit);
	// 	}
	// }

	// get all of the enemy units in our mineral region
	BWAPI::Unitset enemyUnitsInMineralRegion;
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		// check for enemy unit region's existence in mineralRegion
		if (mineralRegion.find(BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())))
			!= mineralRegion.end())
		{
			enemyUnitsInMineralRegion.insert(unit);
		}
	}

	// if there's an enemy worker in our region then assign someone to chase him -- inappropriate
	// priority is still resource collecting, we only need to chase him out of the regions where 
	// the mineral fields locate. Once he is out of the region, the SCV should go back to minerals
	bool assignScoutDefender = enemyUnitsInMineralRegion.size() == 1
		&& (*enemyUnitsInMineralRegion.begin())->getType().isWorker()
		&& ((*enemyUnitsInMineralRegion.begin())->isMoving()
		|| (*enemyUnitsInMineralRegion.begin())->isStartingAttack()
		|| (*enemyUnitsInMineralRegion.begin())->isAttacking());

	// if our current squad is empty and we should assign a worker, do it
	if (scoutDefenseSquad.isEmpty() && assignScoutDefender)
	{
		// the enemy worker that is attacking us
		BWAPI::Unit enemyWorker = *enemyUnitsInMineralRegion.begin();

		// get our worker unit that is mining that is closest to it
		BWAPI::Unit workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);

		// in case we have to assign a scout defending worker, ensure it is assigned (not nullptr)
		while (!workerDefender)
		{
			workerDefender = findClosestWorkerToTarget(_combatUnits, enemyWorker);
		}

		if (enemyWorker && workerDefender)
		{
			// grab it from the worker manager and put it in the squad
			if (_squadData.canAssignUnitToSquad(workerDefender, scoutDefenseSquad))
			{
				WorkerManager::Instance().setCombatWorker(workerDefender);
				_squadData.assignUnitToSquad(workerDefender, scoutDefenseSquad);
			}
		}
	}
	// if our squad is not empty and we shouldn't have a worker chasing then take him out of the squad
	else if (!scoutDefenseSquad.isEmpty() && !assignScoutDefender)
	{
		for (auto & unit : scoutDefenseSquad.getUnits())
		{
			unit->stop();
			if (unit->getType().isWorker())
			{
				WorkerManager::Instance().finishedWithWorker(unit);
			}
		}

		scoutDefenseSquad.clear();
	}
}

void CombatCommander::updateDefenseSquads()
{
	//TODO: if our base is destroyed, remove the squad

	if (_combatUnits.empty())
	{
		return;
	}

	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
	BWTA::Region * enemyRegion = nullptr;
	if (enemyBaseLocation)
	{
		enemyRegion = BWTA::getRegion(enemyBaseLocation->getPosition());
	}

	BWTA::BaseLocation * mainBase = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self());

	size_t numBases = 0;
	// for each of our occupied regions
	for (BWTA::Region * myRegion : InformationManager::Instance().getOccupiedRegions(BWAPI::Broodwar->self()))
	{
		// don't defend inside the enemy region, this will end badly when we are stealing gas
		++numBases;
		if (myRegion == enemyRegion)
		{
			continue;
		}

		// mainBase->getPosition != regionCenter...

		BWAPI::Position regionCenter = myRegion->getCenter();
		BWTA::Chokepoint * choke = BWTA::getNearestChokepoint(regionCenter);
		BWAPI::Position chokeCenter = choke->getCenter();
		int move = (int)chokeCenter.getDistance(regionCenter) / 5;

		if (!regionCenter.isValid())
		{
			continue;
		}

		// start off assuming all enemy units in region are just workers
		int numDefendersPerEnemyUnit = 3;

		// all of the enemy units in this region
		BWAPI::Unitset enemyUnitsInRegion;
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			// if it's an overlord, don't worry about it for defense, we don't care what they see
			if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
			{
				continue;
			}

			if (BWTA::getRegion(BWAPI::TilePosition(unit->getPosition())) == myRegion)
			{
				enemyUnitsInRegion.insert(unit);
			}
		}

		// we can ignore the first enemy worker in our region since we assume it is a scout
		for (auto & unit : enemyUnitsInRegion)
		{
			if (unit->getType().isWorker())
			{
				enemyUnitsInRegion.erase(unit);
				break;
			}
		}

		int numEnemyFlyingInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return u->isFlying(); });
		int numEnemyGroundInRegion = std::count_if(enemyUnitsInRegion.begin(), enemyUnitsInRegion.end(), [](BWAPI::Unit u) { return !u->isFlying(); });

		std::stringstream squadName;
		squadName << "Base Defense " << regionCenter.x << " " << regionCenter.y;

		// if there's nothing in this region to worry about
		/*if (enemyUnitsInRegion.empty())
		{
		// if a defense squad for this region exists, remove it
		if (_squadData.squadExists(squadName.str()))
		{
		_squadData.getSquad(squadName.str()).clear();
		}

		// and return, nothing to defend here
		continue;
		} // We want to defend all the time, therefore we don't want this to happen
		else
		{*/
		// if we don't have a squad assigned to this region already, create one
		if (!_squadData.squadExists(squadName.str()))
		{
			if (!_chokeDefense) {

				// Defend the top of the chokepoint instead of the main base
				if (chokeCenter.x > regionCenter.x) {
					chokeCenter.x -= move;
				}
				else {
					chokeCenter.x += move;
				}

				if (chokeCenter.y > regionCenter.y) {
					chokeCenter.y += move;
				}
				else {
					chokeCenter.y -= move;
				}

				SquadOrder defendChoke(SquadOrderTypes::Defend, chokeCenter, 40 * 25, "Defend Main Base!");
				_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendChoke, BaseDefensePriority));
				_chokeDefense = true;
			}
			else {
				SquadOrder defendRegion(SquadOrderTypes::Defend, regionCenter, 32 * 25, "Defend Region!");
				_squadData.addSquad(squadName.str(), Squad(squadName.str(), defendRegion, BaseDefensePriority));
			}
		}
		//}

		// assign units to the squad
		if (_squadData.squadExists(squadName.str()))
		{
			Squad & defenseSquad = _squadData.getSquad(squadName.str());

			int numCC = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Command_Center);
			if (numCC > 1 && defenseSquad.getSquadOrder().getStatus() == "Defend Main Base!") {

				if (chokeCenter.x > regionCenter.x) {
					chokeCenter.x += move;
				}
				else {
					chokeCenter.x -= move;
				}

				if (chokeCenter.y > regionCenter.y) {
					chokeCenter.y -= move;
				}
				else {
					chokeCenter.y += move;
				}

				SquadOrder defendRegion(SquadOrderTypes::Defend, chokeCenter, 48 * 32, "Defend Choke Point!");
				defenseSquad.setSquadOrder(defendRegion);
			}

			//if (!enemyUnitsInRegion.empty())
			//{
			// figure out how many units we need on defense - this is for when we are attacked
			int flyingDefendersNeeded = numDefendersPerEnemyUnit * numEnemyFlyingInRegion;
			int groundDefensersNeeded = numDefendersPerEnemyUnit * numEnemyGroundInRegion;

			updateDefenseSquadUnits(defenseSquad, flyingDefendersNeeded, groundDefensersNeeded);
			/*}
			else // else we just defend normally, we need a tank, goliath, and vulture in each squad
			{
			int tanksNeeded = 1;
			int vulturesNeeded = 1;
			int goliathsNeeded = 1;

			updateBasicDefenseUnits(defenseSquad, tanksNeeded, vulturesNeeded, goliathsNeeded);
			}*/
		}
		else
		{
			UAB_ASSERT_WARNING(false, "Squad should have existed: %s", squadName.str().c_str());
		}
	}

	// for each of our defense squads, if there aren't any enemy units near the position, remove the squad
	// this only empties the squad, it will not delete the squad
	std::set<std::string> uselessDefenseSquads;
	for (const auto & kv : _squadData.getSquads())
	{
		const Squad & squad = kv.second;
		const SquadOrder & order = squad.getSquadOrder();

		if (order.getType() != SquadOrderTypes::Defend || order.getStatus() == "Get the scout")
		{
			continue;
		}

		bool enemyUnitInRange = false;
		for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
		{
			if (unit->getPosition().getDistance(order.getPosition()) < order.getRadius())
			{
				enemyUnitInRange = true;
				break;
			}
		}

		if (!enemyUnitInRange)
		{
			_squadData.getSquad(squad.getName()).clear();

			// Add standby defense units back in.
			// If we aren't ready to attack, divide all combat units between bases.
			size_t tankCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) +
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode);
			size_t vultureCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Vulture);
			size_t airAttackCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath) + UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Battlecruiser);
			size_t vesselCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Science_Vessel);
			size_t scvCount = UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_SCV);
			size_t totalCount = tankCount + vultureCount + airAttackCount + vesselCount;

			// Otherwise, just have one of each combat unit at each base and attack with the rest.
			// Use numBases to determine how many defense units we can have.
			/*BWAPI::Broodwar->printf("B: %d, g: %d, c: %d, s: %d", numBases, UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Goliath),
				UnitUtil::GetAllUnitCount(BWAPI::UnitTypes::Terran_Battlecruiser), vesselCount);*/

			if (!_tanksReady || !_vulturesReady || !_airAttackReady || !_scvsReady || !_vesselsReady) {
				updateStandbyDefenseUnits(_squadData.getSquad(squad.getName()), (tankCount / numBases) + 1, (vultureCount / numBases) + 1, (airAttackCount / numBases) + 1, (vesselCount / numBases) + 1, numBases, totalCount);
			}
			else {
				updateStandbyDefenseUnits(_squadData.getSquad(squad.getName()), 0, 0, 0, 0, numBases, totalCount);
			}
		}
	}
}

void CombatCommander::updateStandbyDefenseUnits(Squad & defenseSquad, const size_t & tanksNeeded, const size_t & vulturesNeeded, const size_t & airAttackersNeeded,
	const size_t & vesselsNeeded, const size_t & numBases, const size_t & availableUnits)
{
	//TODO::Uniformly distribute all units across all squads instead of filling the first one first.

	if (tanksNeeded == 0 && vulturesNeeded == 0 && airAttackersNeeded == 0 && vesselsNeeded == 0)
	{
		return;
	}

	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();

	size_t tanksInSquad = 0;
	size_t vulturesInSquad = 0;
	size_t airAttackersInSquad = 0;
	size_t vesselsInSquad = 0;
	size_t unitsInSquad = 0;

	// count what units we have in the squad
	for (auto & unit : squadUnits) {
		BWAPI::UnitType type = unit->getType();
		unitsInSquad++;
		if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
			tanksInSquad++;
		}
		if (type == BWAPI::UnitTypes::Terran_Vulture) {
			vulturesInSquad++;
		}
		if (type == BWAPI::UnitTypes::Terran_Goliath || type == BWAPI::UnitTypes::Terran_Battlecruiser) {
			airAttackersInSquad++;
		}
		/*if (type == BWAPI::UnitTypes::Terran_Science_Vessel) {
		vesselsInSquad++; // don't need vessels to defend, they will just wait at main base.
		}*/
	}

	// add tank defenders if we still need them
	size_t tankDefendersAdded = 0;
	while (tanksNeeded > tanksInSquad + tankDefendersAdded && availableUnits / numBases > unitsInSquad + tankDefendersAdded)
	{
		// can't use closestDefender, make our own for tanksToAdd
		BWAPI::Unit tankToAdd = findStandbyDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode);

		// if we find a valid tank, add it to the squad
		if (tankToAdd)
		{
			_squadData.assignUnitToSquad(tankToAdd, defenseSquad);
			++tankDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}
	unitsInSquad += tankDefendersAdded;

	// add tank defenders if we still need them
	size_t vultureDefendersAdded = 0;
	while (vulturesNeeded > vulturesInSquad + vultureDefendersAdded && availableUnits / numBases > unitsInSquad + vultureDefendersAdded)
	{
		// can't use closestDefender, make our own for vulturesToAdd
		BWAPI::Unit vultureToAdd = findStandbyDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), BWAPI::UnitTypes::Terran_Vulture);

		// if we find a valid vulture, add it to the squad
		if (vultureToAdd)
		{
			_squadData.assignUnitToSquad(vultureToAdd, defenseSquad);
			++vultureDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}
	unitsInSquad += vultureDefendersAdded;

	// add tank defenders if we still need them
	size_t airDefendersAdded = 0;
	while ((airAttackersNeeded > airAttackersInSquad + airDefendersAdded) && availableUnits / numBases > unitsInSquad + airDefendersAdded)
	{
		// can't use closestDefender, make our own for tanksToAdd
		BWAPI::Unit airAttackerToAdd = findStandbyDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), BWAPI::UnitTypes::Terran_Goliath);

		// check if we can add a battlecruiser instead -> this will assign goliaths to squads before battlecruisers
		if (!airAttackerToAdd) {
			airAttackerToAdd = findStandbyDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), BWAPI::UnitTypes::Terran_Battlecruiser);
		}

		// if we find a valid goliath, add it to the squad
		if (airAttackerToAdd)
		{
			_squadData.assignUnitToSquad(airAttackerToAdd, defenseSquad);
			++airDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}
	unitsInSquad += airDefendersAdded;

	// add vessel defenders if we still need them
	/*size_t vesselDefendersAdded = 0;
	while (vesselsNeeded > vesselsInSquad + vesselDefendersAdded && availableUnits / numBases > unitsInSquad + vesselDefendersAdded)
	{
	// can't use closestDefender, make our own for tanksToAdd
	BWAPI::Unit vesselToAdd = findStandbyDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), BWAPI::UnitTypes::Terran_Science_Vessel);

	// if we find a valid tank, add it to the squad
	if (vesselToAdd)
	{
	_squadData.assignUnitToSquad(vesselToAdd, defenseSquad);
	++vesselDefendersAdded;
	}
	// otherwise we'll never find another one so break out of this loop
	else
	{
	break;
	}
	}
	unitsInSquad += vesselDefendersAdded;*/
}

BWAPI::Unit CombatCommander::findStandbyDefender(const Squad & defenseSquad, BWAPI::Position pos, BWAPI::UnitType type)
{
	// Note: Only call tank types using Siege mode for this to work

	BWAPI::Unit standbyUnit = nullptr;

	for (auto & unit : _combatUnits)
	{
		// If we are a tank, account for tanked/sieged tanks
		if (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
		{
			if (unit->getType() != BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode && unit->getType() != type)
			{
				continue;
			}
		}
		else if (unit->getType() != type)
		{
			continue;
		}

		if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
		{
			continue;
		}
		// if it is a tank, make it the standbyUnit
		else if (!standbyUnit) {
			standbyUnit = unit;
		}


		// add workers to the defense squad if we are being rushed very quickly
		// TODO:: Add this in later?
		/*if (!Config::Micro::WorkersDefendRush || (unit->getType().isWorker() && !zerglingRush && !beingBuildingRushed()))
		{
		continue;
		}*/

		// We shouldn't need this part for our code
		/*double dist = unit->getDistance(pos);
		if (!closestDefender || (dist < minDistance))
		{
		closestDefender = unit;
		minDistance = dist;
		}*/
	}
	return standbyUnit;
}

void CombatCommander::updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded)
{
	const BWAPI::Unitset & squadUnits = defenseSquad.getUnits();
	size_t flyingDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackAir);
	size_t groundDefendersInSquad = std::count_if(squadUnits.begin(), squadUnits.end(), UnitUtil::CanAttackGround);

	// if there's nothing left to defend, clear the squad
	if (flyingDefendersNeeded == 0 && groundDefendersNeeded == 0)
	{
		defenseSquad.clear();
		return;
	}

	// add flying defenders if we still need them
	size_t flyingDefendersAdded = 0;
	while (flyingDefendersNeeded > flyingDefendersInSquad + flyingDefendersAdded)
	{
		BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), true);

		// if we find a valid flying defender, add it to the squad
		if (defenderToAdd)
		{
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			++flyingDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}

	// add ground defenders if we still need them
	size_t groundDefendersAdded = 0;
	while (groundDefendersNeeded > groundDefendersInSquad + groundDefendersAdded)
	{
		BWAPI::Unit defenderToAdd = findClosestDefender(defenseSquad, defenseSquad.getSquadOrder().getPosition(), false);

		// if we find a valid ground defender add it
		if (defenderToAdd)
		{
			_squadData.assignUnitToSquad(defenderToAdd, defenseSquad);
			++groundDefendersAdded;
		}
		// otherwise we'll never find another one so break out of this loop
		else
		{
			break;
		}
	}
}

BWAPI::Unit CombatCommander::findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender)
{
	BWAPI::Unit closestDefender = nullptr;
	double minDistance = std::numeric_limits<double>::max();

	int zerglingsInOurBase = numZerglingsInOurBase();
	bool zerglingRush = zerglingsInOurBase > 0 && BWAPI::Broodwar->getFrameCount() < 5000;

	for (auto & unit : _combatUnits)
	{
		if ((flyingDefender && !UnitUtil::CanAttackAir(unit)) || (!flyingDefender && !UnitUtil::CanAttackGround(unit)))
		{
			continue;
		}

		if (!_squadData.canAssignUnitToSquad(unit, defenseSquad))
		{
			continue;
		}

		// add workers to the defense squad if we are being rushed very quickly
		if (!Config::Micro::WorkersDefendRush || (unit->getType().isWorker() && !zerglingRush && !beingBuildingRushed()))
		{
			continue;
		}

		double dist = unit->getDistance(pos);
		if (!closestDefender || (dist < minDistance))
		{
			closestDefender = unit;
			minDistance = dist;
		}
	}

	return closestDefender;
}

BWAPI::Position CombatCommander::getDefendLocation()
{
	return BWTA::getRegion(BWTA::getStartLocation(BWAPI::Broodwar->self())->getTilePosition())->getCenter();
}

void CombatCommander::drawSquadInformation(int x, int y)
{
	_squadData.drawSquadInformation(x, y);
}

BWAPI::Position CombatCommander::getMainAttackLocation()
{
	BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());

	// First choice: Attack an enemy region if we can see units inside it
	if (enemyBaseLocation)
	{
		BWAPI::Position enemyBasePosition = enemyBaseLocation->getPosition();

		// get all known enemy units in the area
		BWAPI::Unitset enemyUnitsInArea;
		MapGrid::Instance().GetUnits(enemyUnitsInArea, enemyBasePosition, 800, false, true);

		bool onlyOverlords = true;
		for (auto & unit : enemyUnitsInArea)
		{
			if (unit->getType() != BWAPI::UnitTypes::Zerg_Overlord)
			{
				onlyOverlords = false;
			}
		}

		if (!BWAPI::Broodwar->isExplored(BWAPI::TilePosition(enemyBasePosition)) || !enemyUnitsInArea.empty())
		{
			if (!onlyOverlords)
			{
				return enemyBaseLocation->getPosition();
			}
		}
	}

	// Second choice: Attack known enemy buildings
	for (const auto & kv : InformationManager::Instance().getUnitInfo(BWAPI::Broodwar->enemy()))
	{
		const UnitInfo & ui = kv.second;

		if (ui.type.isBuilding() && ui.lastPosition != BWAPI::Positions::None)
		{
			return ui.lastPosition;
		}
	}

	// Third choice: Attack visible enemy units that aren't overlords
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord)
		{
			continue;
		}

		if (UnitUtil::IsValidUnit(unit) && unit->isVisible())
		{
			return unit->getPosition();
		}
	}

	// Fourth choice: We can't see anything so explore the map attacking along the way
	return MapGrid::Instance().getLeastExplored();
}

BWAPI::Unit CombatCommander::findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
{
	UAB_ASSERT(target != nullptr, "target was null");

	if (!target)
	{
		return nullptr;
	}

	BWAPI::Unit closestWorkerToTarget = nullptr;
	double closestDist = 100000;

	// for each of our workers
	for (auto & unit : unitsToAssign)
	{
		if (!unit->getType().isWorker())
		{
			continue;
		}

		// if it is a move worker -- not limited to mineral or idle
		// unitsToAssign can also be a Unitset of combat workers, e.g., _combatUnits
		if (WorkerManager::Instance().isFree(unit)
			|| WorkerManager::Instance().isWorkerCombat(unit))
		{
			double dist = unit->getDistance(target);

			if (!closestWorkerToTarget || dist < closestDist)
			{
				closestWorkerToTarget = unit;
				dist = closestDist;
			}
		}
	}

	return closestWorkerToTarget;
}

// when do we want to defend with our workers?
// this function can only be called if we have no fighters to defend with
int CombatCommander::defendWithWorkers()
{
	// our home nexus position
	BWAPI::Position homePosition = BWTA::getStartLocation(BWAPI::Broodwar->self())->getPosition();;

	// enemy units near our workers
	int enemyUnitsNearWorkers = 0;

	// defense radius of nexus
	int defenseRadius = 300;

	// fill the set with the types of units we're concerned about
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		// if it's a zergling or a worker we want to defend
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling)
		{
			if (unit->getDistance(homePosition) < defenseRadius)
			{
				enemyUnitsNearWorkers++;
			}
		}
	}

	// if there are enemy units near our workers, we want to defend
	return enemyUnitsNearWorkers;
}

int CombatCommander::numZerglingsInOurBase()
{
	int concernRadius = 600;
	int zerglings = 0;
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	// check to see if the enemy has zerglings as the only attackers in our base
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling)
		{
			continue;
		}

		if (unit->getDistance(ourBasePosition) < concernRadius)
		{
			zerglings++;
		}
	}

	return zerglings;
}

bool CombatCommander::beingBuildingRushed()
{
	int concernRadius = 1200;
	BWAPI::Position ourBasePosition = BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation());

	// check to see if the enemy has zerglings as the only attackers in our base
	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->getType().isBuilding())
		{
			return true;
		}
	}

	return false;
}