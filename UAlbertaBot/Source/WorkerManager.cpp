#include "Common.h"
#include "WorkerManager.h"
#include "Micro.h"

using namespace UAlbertaBot;

WorkerManager::WorkerManager() 
{
    previousClosestWorker = nullptr;
}

WorkerManager & WorkerManager::Instance() 
{
	static WorkerManager instance;
	return instance;
}

void WorkerManager::update() 
{
	updateWorkerStatus();
	handleGasWorkers();
	handleIdleWorkers();
	handleMoveWorkers();
	handleCombatWorkers();

	drawResourceDebugInfo();
	drawWorkerInformation(450,20);

	workerData.drawDepotDebugInfo();

    handleRepairWorkers();
}

void WorkerManager::updateWorkerStatus() 
{
	// for each of our Workers
	for (auto & worker : workerData.getWorkers())
	{
		if (!worker->isCompleted())
		{
			continue;
		}

		// if it's idle
		if (worker->isIdle() && 
			(workerData.getWorkerJob(worker) != WorkerData::Build) && 
			(workerData.getWorkerJob(worker) != WorkerData::Move) &&
			(workerData.getWorkerJob(worker) != WorkerData::Scout)) 
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		// if its job is gas
		if (workerData.getWorkerJob(worker) == WorkerData::Gas)
		{
			BWAPI::Unit refinery = workerData.getWorkerResource(worker);

			// if the refinery doesn't exist anymore
			if (!refinery || !refinery->exists() ||	refinery->getHitPoints() <= 0)
			{
				setMineralWorker(worker);
			}
		}
	}
}

void WorkerManager::setRepairWorker(BWAPI::Unit worker, BWAPI::Unit unitToRepair)
{
    workerData.setWorkerJob(worker, WorkerData::Repair, unitToRepair);
}

void WorkerManager::stopRepairing(BWAPI::Unit worker)
{
    workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
}

void WorkerManager::handleGasWorkers() 
{
	// for each unit we have
	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		// if that unit is a refinery
		if (unit->getType().isRefinery() && unit->isCompleted() && !isGasStealRefinery(unit))
		{
			// get the number of workers currently assigned to it
			int numAssigned = workerData.getNumAssignedWorkers(unit);

			// if it's less than we want it to be, fill 'er up
			for (int i=0; i<(Config::Macro::WorkersPerRefinery-numAssigned); ++i)
			{
				BWAPI::Unit gasWorker = getGasWorker(unit);
				if (gasWorker)
				{
					workerData.setWorkerJob(gasWorker, WorkerData::Gas, unit);
				}
			}
		}
	}

}

bool WorkerManager::isGasStealRefinery(BWAPI::Unit unit)
{
    BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->enemy());
    if (!enemyBaseLocation)
    {
        return false;
    }
    
    if (enemyBaseLocation->getGeysers().empty())
    {
        return false;
    }
    
	for (auto & u : enemyBaseLocation->getGeysers())
	{
        if (unit->getTilePosition() == u->getTilePosition())
        {
            return true;
        }
	}

    return false;
}

void WorkerManager::handleIdleWorkers() 
{
	// for each of our workers
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		// if it is idle
		if (workerData.getWorkerJob(worker) == WorkerData::Idle) 
		{
			// send it to the nearest mineral patch
			setMineralWorker(worker);
		}
	}
}

void WorkerManager::handleRepairWorkers()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		// closest mineral worker repair buildings
		if (unit->getType().isBuilding() && (unit->getHitPoints() < unit->getType().maxHitPoints()))
		{
			BWAPI::Unit repairWorker = getClosestMineralWorkerTo(unit);
			setRepairWorker(repairWorker, unit);
			break;
		}
		// closest combat worker repair Siege Tanks -- do this in CombatCommander
		// if ((unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
		// 	|| unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode)
		// 	&& (unit->getHitPoints() < unit->getType().maxHitPoints()))
		// {
		// 	BWAPI::Unit repairWorker = getClosestCombatWorkerTo(unit);
		// 	setRepairWorker(repairWorker, unit);
		// 	break;
		// }
	}
}

// bad micro for combat workers
void WorkerManager::handleCombatWorkers()
{
	// =========================================old approach==========================================
	for (auto & worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker != nullptr, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Combat)
		{
			BWAPI::Broodwar->drawCircleMap(worker->getPosition().x, worker->getPosition().y, 4, BWAPI::Colors::Yellow, true);
			BWAPI::Unit target = getClosestEnemyUnit(worker);

			if (target)
			{
				Micro::SmartAttackUnit(worker, target);
			}
		}
	}

	// /*
	// 	Evaluation function for the current scenario
	// 	For now just compare the number of combat works with the number of close enemy units
	// 	- if the former value is way greater than the latter value, which means that DefBot has many extra
	// 	combat workers, in this case, if every single enemy unit is already under attack, it is not necessary
	// 	to set more combat worker to attack them, finishedWithCombatWorkers take care of the extra ones
	// 	- if DefBot does not has this type of advantage, use the original micro, i.e., set all combat
	// 	workers to attack closest enemy units
	// */
	// int numCombatWorkers = getNumCombatWorkers();
	// // TODO find a better way to implement the evaluation function
	// // TODO option 1, calculate numCloseEnemyUnit for several random combat workers, take the average
	// // TODO option 2, re-calculate it as the game runs
	// // TODO option 3, add a weight for different enemy units
	// int numCloseEnemyUnit;
	// for (auto & worker : workerData.getWorkers())
	// {
	// 	UAB_ASSERT(worker != nullptr, "Worker was null");

	// 	if (workerData.getWorkerJob(worker) == WorkerData::Combat)
	// 	{
	// 		numCloseEnemyUnit = getNumCloseEnemyUnit(worker);
	// 		break;
	// 	}
	// }


	// for (auto & worker : workerData.getWorkers())
	// {
	// 	UAB_ASSERT(worker != nullptr, "Worker was null");

	// 	if (workerData.getWorkerJob(worker) == WorkerData::Combat)
	// 	{
	// 		BWAPI::Broodwar->drawCircleMap(worker->getPosition().x, worker->getPosition().y, 4, BWAPI::Colors::Yellow, true);
	// 		BWAPI::Unit target = getClosestEnemyUnit(worker);

	// 		if (target)
	// 		{
	// 			if (target->isUnderAttack() && (double)numCombatWorkers > (double)numCloseEnemyUnit * 1.5)
	// 			{
	// 				continue;
	// 			}
	// 			// if (target->isTargetable())
	// 			// {
	// 				Micro::SmartAttackUnit(worker, target);
	// 			// }
	// 		}
	// 	}
	// }
	
}

/*
	temporary part of the evaluation function, implemented based on getClosestEnemyUnit
	probably needs optimization
*/
int WorkerManager::getNumCloseEnemyUnit(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	int count = 0;

	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 10000;

	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		double dist = unit->getDistance(worker);

		if ((dist < 400) && (!closestUnit || (dist < closestDist)))
		{
			count++;
		}
	}

	return count;
}

BWAPI::Unit WorkerManager::getClosestEnemyUnit(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 10000;

	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		double dist = unit->getDistance(worker);

		if ((dist < 400) && (!closestUnit || (dist < closestDist)))
		{
			closestUnit = unit;
			closestDist = dist;
		}
	}

	return closestUnit;
}

void WorkerManager::finishedWithCombatWorkers()
{
	for (auto & worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker != nullptr, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Combat)
		{
			setMineralWorker(worker);
		}
	}
}

void WorkerManager::finishedWithRepairWorker(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Worker was null");

	if (workerData.getWorkerJob(unit) == WorkerData::Repair)
	{
		setMineralWorker(unit);
	}
}

// set repair workers in scvSquad to combat workers once repair is done
// since they are originally combat units
void WorkerManager::finishedWithRepairCombatWorker(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Worker was null");

	if (workerData.getWorkerJob(unit) == WorkerData::Repair)
	{
		setCombatWorker(unit);
	}
}


BWAPI::Unit WorkerManager::getClosestMineralWorkerTo(BWAPI::Unit targetUnit)
{
    UAB_ASSERT(targetUnit != nullptr, "targetUnit was null");

    BWAPI::Unit closestMineralWorker = nullptr;
    double closestDist = 100000;

    if (previousClosestWorker)
    {
        if (previousClosestWorker->getHitPoints() > 0)
        {
            return previousClosestWorker;
        }
    }

    // for each of our workers
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");
		if (!worker)
		{
			continue;
		}
		// if it is a move worker
        if (workerData.getWorkerJob(worker) == WorkerData::Minerals) 
		{
			double dist = worker->getDistance(targetUnit);

            if (!closestMineralWorker || dist < closestDist)
            {
                closestMineralWorker = worker;
                dist = closestDist;
            }
		}
	}

    previousClosestWorker = closestMineralWorker;
    return closestMineralWorker;
}

// get Closest Combat Worker To the target Unit
BWAPI::Unit WorkerManager::getClosestCombatWorkerTo(BWAPI::Unit targetUnit)
{
	UAB_ASSERT(targetUnit != nullptr, "targetUnit was null");

	BWAPI::Unit closestCombatWorker = nullptr;
	double closestDist = 100000;

	if (previousClosestWorker)
	{
		if (previousClosestWorker->getHitPoints() > 0)
		{
			return previousClosestWorker;
		}
	}

	// for each of our workers
	for (auto & worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker != nullptr, "Worker was null");
		if (!worker)
		{
			continue;
		}
		// if it is a move worker
		if (workerData.getWorkerJob(worker) == WorkerData::Combat) 
		{
			double dist = worker->getDistance(targetUnit);

			if (!closestCombatWorker || dist < closestDist)
			{
				closestCombatWorker = worker;
				dist = closestDist;
			}
		}
	}

	previousClosestWorker = closestCombatWorker;
	return closestCombatWorker;
}


BWAPI::Unit WorkerManager::getWorkerScout()
{
    // for each of our workers
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");
		if (!worker)
		{
			continue;
		}
		// if it is a move worker
        if (workerData.getWorkerJob(worker) == WorkerData::Scout) 
		{
			return worker;
		}
	}

    return nullptr;
}

void WorkerManager::handleMoveWorkers() 
{
	// for each of our workers
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		// if it is a move worker
		if (workerData.getWorkerJob(worker) == WorkerData::Move) 
		{
			WorkerMoveData data = workerData.getWorkerMoveData(worker);
			
			Micro::SmartMove(worker, data.position);
		}
	}
}

// set a worker to mine minerals
void WorkerManager::setMineralWorker(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");

	// check if there is a mineral available to send the worker to
	BWAPI::Unit depot = getClosestDepot(unit);

	// if there is a valid mineral
	if (depot)
	{
		// update workerData with the new job
		workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
	}
	else
	{
		// BWAPI::Broodwar->printf("No valid depot for mineral worker");
	}
}

BWAPI::Unit WorkerManager::getClosestDepot(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	BWAPI::Unit closestDepot = nullptr;
	double closestDistance = 0;

	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (unit->getType().isResourceDepot() && (unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair) && !workerData.depotIsFull(unit))
		{
			double distance = unit->getDistance(worker);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}


// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	//BWAPI::Broodwar->printf("BuildingManager finished with worker %d", unit->getID());
	if (workerData.getWorkerJob(unit) != WorkerData::Scout)
	{
		workerData.setWorkerJob(unit, WorkerData::Idle, nullptr);
	}
}

BWAPI::Unit WorkerManager::getGasWorker(BWAPI::Unit refinery)
{
	UAB_ASSERT(refinery != nullptr, "Refinery was null");

	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		if (workerData.getWorkerJob(unit) == WorkerData::Minerals)
		{
			double distance = unit->getDistance(refinery);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	return closestWorker;
}

 void WorkerManager::setBuildingWorker(BWAPI::Unit worker, Building & b)
 {
     UAB_ASSERT(worker != nullptr, "Worker was null");

     workerData.setWorkerJob(worker, WorkerData::Build, b.type);
 }

// gets a builder for BuildingManager to use
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
BWAPI::Unit WorkerManager::getBuilder(Building & b, bool setJobAsBuilder)
{
	// variables to hold the closest worker of each type to the building
	BWAPI::Unit closestMovingWorker = nullptr;
	BWAPI::Unit closestMiningWorker = nullptr;
	double closestMovingWorkerDistance = 0;
	double closestMiningWorkerDistance = 0;

	// look through each worker that had moved there first
	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

        // gas steal building uses scout worker
        if (b.isGasSteal && (workerData.getWorkerJob(unit) == WorkerData::Scout))
        {
            if (setJobAsBuilder)
            {
                workerData.setWorkerJob(unit, WorkerData::Build, b.type);
            }
            return unit;
        }

		// mining worker check
		if (unit->isCompleted() && (workerData.getWorkerJob(unit) == WorkerData::Minerals))
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (!closestMiningWorker || distance < closestMiningWorkerDistance)
			{
				closestMiningWorker = unit;
				closestMiningWorkerDistance = distance;
			}
		}

		// moving worker check
		if (unit->isCompleted() && (workerData.getWorkerJob(unit) == WorkerData::Move))
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (!closestMovingWorker || distance < closestMovingWorkerDistance)
			{
				closestMovingWorker = unit;
				closestMovingWorkerDistance = distance;
			}
		}
	}

	// if we found a moving worker, use it, otherwise using a mining worker
	BWAPI::Unit chosenWorker = closestMovingWorker ? closestMovingWorker : closestMiningWorker;

	// if the worker exists (one may not have been found in rare cases)
	if (chosenWorker && setJobAsBuilder)
	{
		workerData.setWorkerJob(chosenWorker, WorkerData::Build, b.type);
	}

	// return the worker
	return chosenWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Scout, nullptr);
}

// gets a worker which will move to a current location
BWAPI::Unit WorkerManager::getMoveWorker(BWAPI::Position p)
{
	// set up the pointer
	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	// for each worker we currently have
	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		// only consider it if it's a mineral worker
		if (unit->isCompleted() && workerData.getWorkerJob(unit) == WorkerData::Minerals)
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(p);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	// return the worker
	return closestWorker;
}

// sets a worker to move to a given location
void WorkerManager::setMoveWorker(int mineralsNeeded, int gasNeeded, BWAPI::Position p)
{
	// set up the pointer
	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	// for each worker we currently have
	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Unit was null");

		// only consider it if it's a mineral worker
		if (unit->isCompleted() && workerData.getWorkerJob(unit) == WorkerData::Minerals)
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(p);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	if (closestWorker)
	{
		//BWAPI::Broodwar->printf("Setting worker job Move for worker %d", closestWorker->getID());
		workerData.setWorkerJob(closestWorker, WorkerData::Move, WorkerMoveData(mineralsNeeded, gasNeeded, p));
	}
	else
	{
		//BWAPI::Broodwar->printf("Error, no worker found");
	}
}

// will we have the required resources by the time a worker can travel a certain distance
bool WorkerManager::willHaveResources(int mineralsRequired, int gasRequired, double distance)
{
	// if we don't require anything, we will have it
	if (mineralsRequired <= 0 && gasRequired <= 0)
	{
		return true;
	}

	// the speed of the worker unit
	double speed = BWAPI::Broodwar->self()->getRace().getWorker().topSpeed();

    UAB_ASSERT(speed > 0, "Speed is negative");

	// how many frames it will take us to move to the building location
	// add a second to account for worker getting stuck. better early than late
	double framesToMove = (distance / speed) + 50;

	// magic numbers to predict income rates
	double mineralRate = getNumMineralWorkers() * 0.045;
	double gasRate     = getNumGasWorkers() * 0.07;

	// calculate if we will have enough by the time the worker gets there
	if (mineralRate * framesToMove >= mineralsRequired &&
		gasRate * framesToMove >= gasRequired)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void WorkerManager::setCombatWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	workerData.setWorkerJob(worker, WorkerData::Combat, nullptr);
}

void WorkerManager::onUnitMorph(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
	{
		workerData.addWorker(unit);
	}

	// if something morphs into a building, it was a worker?
	if (unit->getType().isBuilding() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
	{
		//BWAPI::Broodwar->printf("A Drone started building");
		workerData.workerDestroyed(unit);
	}
}

void WorkerManager::onUnitShow(BWAPI::Unit unit)
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	// add the depot if it exists
	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.addDepot(unit);
	}

	// if something morphs into a worker, add it
	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
	{
		//BWAPI::Broodwar->printf("A worker was shown %d", unit->getID());
		workerData.addWorker(unit);
	}
}


void WorkerManager::rebalanceWorkers()
{
	// for each worker
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker != nullptr, "Worker was null");

		if (!workerData.getWorkerJob(worker) == WorkerData::Minerals)
		{
			continue;
		}

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);

		if (depot && workerData.depotIsFull(depot))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
		else if (!depot)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
	}
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit != nullptr, "Unit was null");

	if (unit->getType().isResourceDepot() && unit->getPlayer() == BWAPI::Broodwar->self())
	{
		workerData.removeDepot(unit);
	}

	if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self()) 
	{
		workerData.workerDestroyed(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)
	{
		rebalanceWorkers();
	}
}

void WorkerManager::drawResourceDebugInfo() 
{
    if (!Config::Debug::DrawResourceInfo)
    {
        return;
    }

	for (auto & worker : workerData.getWorkers()) 
    {
        UAB_ASSERT(worker != nullptr, "Worker was null");

		char job = workerData.getJobCode(worker);

		BWAPI::Position pos = worker->getTargetPosition();

		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y - 5, "\x07%c", job);

		BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, pos.x, pos.y, BWAPI::Colors::Cyan);

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);
		if (depot)
		{
			BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, depot->getPosition().x, depot->getPosition().y, BWAPI::Colors::Orange);
		}
	}
}

void WorkerManager::drawWorkerInformation(int x, int y) 
{
    if (!Config::Debug::DrawWorkerInfo)
    {
        return;
    }

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Workers %d", workerData.getNumMineralWorkers());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UnitID");
	BWAPI::Broodwar->drawTextScreen(x+50, y+20, "\x04 State");

	int yspace = 0;

	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit != nullptr, "Worker was null");

		BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03 %d", unit->getID());
		BWAPI::Broodwar->drawTextScreen(x+50, y+40+((yspace++)*10), "\x03 %c", workerData.getJobCode(unit));
	}
}

bool WorkerManager::isFree(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	return workerData.getWorkerJob(worker) == WorkerData::Minerals || workerData.getWorkerJob(worker) == WorkerData::Idle;
}

bool WorkerManager::isWorkerScout(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	return (workerData.getWorkerJob(worker) == WorkerData::Scout);
}

bool WorkerManager::isBuilder(BWAPI::Unit worker)
{
    UAB_ASSERT(worker != nullptr, "Worker was null");

	return (workerData.getWorkerJob(worker) == WorkerData::Build);
}

bool WorkerManager::isWorkerCombat(BWAPI::Unit worker)
{
	UAB_ASSERT(worker != nullptr, "Worker was null");

	return (workerData.getWorkerJob(worker) == WorkerData::Combat);
}


int WorkerManager::getNumMineralWorkers() 
{
	return workerData.getNumMineralWorkers();	
}

int WorkerManager::getNumIdleWorkers() 
{
	return workerData.getNumIdleWorkers();	
}

int WorkerManager::getNumGasWorkers() 
{
	return workerData.getNumGasWorkers();
}

int WorkerManager::getNumWorkers()
{
	return workerData.getNumWorkers();
}

int WorkerManager::getNumBuildWorkers()
{
	return workerData.getNumBuildWorkers();
}

int WorkerManager::getNumCombatWorkers()
{
	return workerData.getNumCombatWorkers();
}

int WorkerManager::getNumRepairWorkers()
{
	return workerData.getNumRepairWorkers();
}

int WorkerManager::getNumMoveWorkers()
{
	return workerData.getNumMoveWorkers();
}

int WorkerManager::getNumScoutWorkers()
{
	return workerData.getNumScoutWorkers();
}

int WorkerManager::getNumDefaultWorkers()
{
	return workerData.getNumDefaultWorkers();
}
