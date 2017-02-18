#pragma once

#include <Common.h>
#include "BuildingManager.h"
#include "WorkerData.h"

namespace UAlbertaBot
{
class Building;

class WorkerManager
{
    WorkerData  workerData;
    BWAPI::Unit previousClosestWorker;

    void        setMineralWorker(BWAPI::Unit unit);
    bool        isGasStealRefinery(BWAPI::Unit unit);
    
    void        handleIdleWorkers();
    void        handleGasWorkers();
    void        handleMoveWorkers();
    void        handleCombatWorkers();
    void        handleRepairWorkers();

    WorkerManager();

public:

    void        update();
    void        onUnitDestroy(BWAPI::Unit unit);
    void        onUnitMorph(BWAPI::Unit unit);
    void        onUnitShow(BWAPI::Unit unit);
    void        onUnitRenegade(BWAPI::Unit unit);
    void        finishedWithWorker(BWAPI::Unit unit);

    void        finishedWithCombatWorkers();
    void        finishedWithRepairWorker(BWAPI::Unit unit);
    void        finishedWithRepairCombatWorker(BWAPI::Unit unit);


    void        drawResourceDebugInfo();
    void        updateWorkerStatus();
    void        drawWorkerInformation(int x,int y);

    // all possible cases for number of workers, for further uses
    int         getNumMineralWorkers();
    int         getNumGasWorkers();
    int         getNumIdleWorkers();
    int getNumWorkers();
    int getNumBuildWorkers();
    int getNumCombatWorkers();
    int getNumRepairWorkers();
    int getNumMoveWorkers();
    int getNumScoutWorkers();
    int getNumDefaultWorkers();
    // =========================================================

    // temporary part of the evaluation function
    int getNumCloseEnemyUnit(BWAPI::Unit worker);

    void        setScoutWorker(BWAPI::Unit worker);

    bool        isWorkerScout(BWAPI::Unit worker);
    bool        isFree(BWAPI::Unit worker);
    bool        isBuilder(BWAPI::Unit worker);
    bool        isWorkerCombat(BWAPI::Unit worker);

    BWAPI::Unit getBuilder(Building & b,bool setJobAsBuilder = true);
    BWAPI::Unit getMoveWorker(BWAPI::Position p);
    BWAPI::Unit getClosestDepot(BWAPI::Unit worker);
    BWAPI::Unit getGasWorker(BWAPI::Unit refinery);
    BWAPI::Unit getClosestEnemyUnit(BWAPI::Unit worker);
    BWAPI::Unit getClosestMineralWorkerTo(BWAPI::Unit targetUnit);
    BWAPI::Unit getClosestCombatWorkerTo(BWAPI::Unit targetUnit);
    BWAPI::Unit getWorkerScout();

    void        setBuildingWorker(BWAPI::Unit worker,Building & b);
    void        setRepairWorker(BWAPI::Unit worker,BWAPI::Unit unitToRepair);
    void        stopRepairing(BWAPI::Unit worker);
    void        setMoveWorker(int m,int g,BWAPI::Position p);
    void        setCombatWorker(BWAPI::Unit worker);

    bool        willHaveResources(int mineralsRequired,int gasRequired,double distance);
    void        rebalanceWorkers();

    static WorkerManager &  Instance();
};
}