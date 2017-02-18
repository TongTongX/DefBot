#pragma once

#include "Common.h"
#include "Squad.h"
#include "SquadData.h"
#include "InformationManager.h"
#include "StrategyManager.h"

namespace UAlbertaBot
{
class CombatCommander
{
	SquadData       _squadData;
    BWAPI::Unitset  _combatUnits;
	BWAPI::Unitset	_attackUnits;
    bool            _initialized;
	bool			_tanksReady;
	bool			_vulturesReady;
	bool			_scvsReady;
	bool			_airAttackReady;
	bool			_vesselsReady;
	bool			_tankDef;
	bool			_vultureDef;
	bool			_airDef;
	size_t			_attackMult;
	bool			_chokeDefense;

    void            updateScoutDefenseSquad();
	void            updateDefenseSquads();
	void            updateAttackSquads();
	void			updateAttackUnits();
	void			updateTankSquads();
	void			updateVultureSquads();
	void			updateSCVSquads();
	void			updateAirAttackSquads();
	void			updateVesselSquads();
    void            updateDropSquads();
	void            updateIdleSquad();
	bool            isSquadUpdateFrame();
	int             getNumType(BWAPI::Unitset & units, BWAPI::UnitType type);

	BWAPI::Unit     findClosestDefender(const Squad & defenseSquad, BWAPI::Position pos, bool flyingDefender);
    BWAPI::Unit     findClosestWorkerToTarget(BWAPI::Unitset & unitsToAssign, BWAPI::Unit target);
	BWAPI::Unit		findStandbyDefender(const Squad & defenseSquad, BWAPI::Position pos, BWAPI::UnitType type);

	BWAPI::Position getDefendLocation();
    BWAPI::Position getMainAttackLocation();

    void            initializeSquads();
    void            verifySquadUniqueMembership();
    void            assignFlyingDefender(Squad & squad);
    void            emptySquad(Squad & squad, BWAPI::Unitset & unitsToAssign);
    int             getNumGroundDefendersInSquad(Squad & squad);
    int             getNumAirDefendersInSquad(Squad & squad);

	void			updateStandbyDefenseUnits(Squad & defenseSquad, const size_t & tankNeeded, const size_t & vultureNeeded,
		const size_t & airAttackersNeeded, const size_t & vesselsNeeded, const size_t & numBases, const size_t & unitsAvailable);
    void            updateDefenseSquadUnits(Squad & defenseSquad, const size_t & flyingDefendersNeeded, const size_t & groundDefendersNeeded);
    int             defendWithWorkers();

    int             numZerglingsInOurBase();
    bool            beingBuildingRushed();

public:

	CombatCommander();

	void update(const BWAPI::Unitset & combatUnits);
    
	void drawSquadInformation(int x, int y);
};
}
