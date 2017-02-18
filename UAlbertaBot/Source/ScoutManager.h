#pragma once

#include "Common.h"
#include "MicroManager.h"
#include "InformationManager.h"

namespace UAlbertaBot
{
class ScoutManager 
{
	BWAPI::Unit	        _workerScout;
    std::string                     _scoutStatus;
    std::string                     _gasStealStatus;
	int				                _numWorkerScouts;
	bool			                _scoutUnderAttack;
    bool                            _didGasSteal;
    bool                            _gasStealFinished;
    int                             _currentRegionVertexIndex;
    int                             _previousScoutHP;
	std::vector<BWAPI::Position>    _enemyRegionVertices;
	BWAPI::Unit						_tankScout;
	int								_numTankScouts;

	bool                            enemyWorkerInRadius();
    bool			                immediateThreat();
    void                            gasSteal();
    int                             getClosestVertexIndex(BWAPI::Unit unit);
    BWAPI::Position                 getFleePosition();
	BWAPI::Unit	        getEnemyGeyser();
	BWAPI::Unit	        closestEnemyWorker();
    void                            followPerimeter();
	void                            moveScouts();
    void                            drawScoutInformation(int x, int y);
    void                            calculateEnemyRegionVertices();
	void							tankScouting();
	ScoutManager();

public:

    static ScoutManager & Instance();

	void update();

    void setWorkerScout(BWAPI::Unit unit);
	void setTankScout(BWAPI::Unit unit);
	void finishedWithTankScout(BWAPI::Unit unit);

	void onSendText(std::string text);
	void onUnitShow(BWAPI::Unit unit);
	void onUnitHide(BWAPI::Unit unit);
	void onUnitCreate(BWAPI::Unit unit);
	void onUnitRenegade(BWAPI::Unit unit);
	void onUnitDestroy(BWAPI::Unit unit);
	void onUnitMorph(BWAPI::Unit unit);
};
}