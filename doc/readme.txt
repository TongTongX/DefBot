Compiling Instructions:

1. In Config.cpp, make sure ConfigFileLocation is initialized to the correct path of our
   configuration file UAlbertaBot_Config.txt. If it is not pointed to the correct location,
   an error message will be printed on game start.
2. Import project into Visual Studio.
3. In Solution Explorer, right-click the project node and then select Rename from the
   context menu. Type "DefBot" (no quotation marks) and hit Enter key. This will ensure that
   the compiled .dll file is DefBot.dll instead of the previous UAlbertaBot.dll.
4. Select Release mode. Compile.
3. Individual flags for displaying visual information inside of UAlbertaBot_Config.txt
   have been disabled. You'll have manually to enable any you'd like to use for testing. 



Changes and Additions List:

StrategyManager::
- shouldExpandNow()
- getTerranBuildOrderGoal()
- getBuildOrderGoal()

BuildingManager::
- checkForDeadTerranBuilders()
- assignWorkersToUnassignedBuildings()
- update(const BWAPI::Unitset & combatUnits)

BuildingPlacer::
- getBuildLocationNear(const Building & b, int buildDist, bool horizontalOnly)

CombatCommander::
- _tanksReady, _vulturesReady, _scvsReady, _airAttackReady, _vesselsReady, _attackMult
- initializeSquads()
- update(const BWAPI::Unitset & combatUnits)
- getNumType(BWAPI::Unitset & units, BWAPI::UnitType type)
- findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target)
- updateAttackSquads()
- updateTankSquads()
- updateVultureSquads()
- updateSCVSquads()
- updateAirAttackSquad()
- updateVesselSquads()
- updateScoutDefenseSquad()
- updateDefenseSquads()
- updateStandbyDefenseUnits(Squad & defenseSquad, const size...))
- updateDefenseSquadUnits()

WorkerData::
- getNumBuildWorkers()
- getNumCombatWorkers()
- getNumRepairWorkers()
- getNumMoveWorkers()
- getNumScoutWorkers()
- getNumDefaultWorkers()

WorkerManager::
- handleCombatWorkers()
- handleIdleWorkers()
- getNumCloseEnemyUnit(BWAPI::Unit worker)
- getClosestMineralWorkerTo(BWAPI::Unit targetUnit)
- getClosestCombatWorkerTo(BWAPI::Unit targetUnit)
- getWorkerScout()
- finishedWithRepairWorker(BWAPI::Unit unit)
- isWorkerCombat(BWAPI::Unit worker)
- getNumWorkers()
- getNumBuildWorkers()
- getNumCombatWorkers()
- getNumRepairWorkers()
- getNumMoveWorkers()
- getNumScoutWorkers()
- getNumDefaultWorkers()
- getNumAssignedWorkers(BWAPI::Unit unit) <-? did we edit?

InformationManager::
- isCombatUnit(BWAPI::UnitType type)

ProductionManager::
- update()
