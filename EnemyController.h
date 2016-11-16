//Copyright @ Will Burgess

#pragma once

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CharacterBase.h"
#include "StaticLibrary.h"
#include "ObjectiveMajor.h"
#include "BaseEnemyUnit.h"
#include "EnemyController.generated.h"

/// Behavior component of all AI enemy units. 
/**
<UL>
<LI>Handle COMMANDS from COMMANDER_AI</LI>
<LI>Handles Movement and mesh navigation</LI>
<LI>Uses a subtree to delegate all combat behavior</LI>
<LI>Defines a set of STATES that represent subsets of behavior:</LI>
<OL>
<LI>IDLE</LI>
<LI>FLEE</LI>
<LI>GUARD</LI>
<LI>SEEK</LI>
<LI>ENGAGE</LI>
</OL>
</UL>
*/
UCLASS()
class HEAVYMETALMARAUDERS_API AEnemyController : public AAIController
{
	GENERATED_BODY()

private:
	/// Constructor
	AEnemyController(const FObjectInitializer& ObjectInitializer);

	/// AController::Possess override; what happens upon possession of a unit by this controller
	virtual void Possess(class APawn *InPawn) override;

	/// AActor::BeginPlay override
	virtual void BeginPlay() override;

	/// AAIController::OnMoveCompleted override
	virtual void AEnemyController::OnMoveCompleted(FAIRequestID RequestID,
		const FPathFollowingResult& Result) override;
	// FAIRequestID RequestID, const FPathFollowingResult& Result

	/// Used to track the Guard Command's ForceMove
	UPROPERTY()
		bool ForceMove_Guard;

	/// Pointer to possessed unit
	UPROPERTY()
		ABaseEnemyUnit* AIUnit;

	/// Array of pointers to each objective on the map; TODO - needs to be stored at GameMode lvl, and referenced by units (so each unit does not have a unique reference array)
	UPROPERTY()
		TArray<AObjectiveMajor*> MajorObjectives;

protected:

	// Base Blackboard key ID refs
	FBlackboard::FKey IsExecutingCommandID;	/// Bool if unit is executing command from COMMANDER
	FBlackboard::FKey CurrentEnemyID;		/// UObject currently focused on
	FBlackboard::FKey KnownEnemyLocationID;	/// FVector of where unit thinks P1 is
	FBlackboard::FKey PointToGuardID;		/// FVector where unit is sent to guard
	FBlackboard::FKey HaveLoSID;			/// Bool; Does unit have LoS on CurrentEnemy
	FBlackboard::FKey AIStateID;			/// Current behavior state
	FBlackboard::FKey EnemyInAggroRangeID;	/// Bool if enemy is within AggroRange of unit
	FBlackboard::FKey EnemyInGuardAreaID;	/// Bool if enemy is within AggroRange radius around PointToGuard
	FBlackboard::FKey EnemyInLeashRangeID;	/// Bool if enemy is within LeashRange of GuardArea
	FBlackboard::FKey GuardedObjectiveID;	/// UObject of which objective is being guarded
	FBlackboard::FKey PointToFleeID;		/// FVector of safe area unit can away from P1 to

											// Combat Blackboard key ID refs


public:

	/// Reference to Player character class
	UPROPERTY()
		class ACharacterBase* PlayerInstance;

	/// Instance of EBaseUnitAIStates; tracks this unit's AI state as updated by BT services
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Enum)
		EBaseUnitAIStates AIStatesEnum;

	/// The distance at which a unit "hears" the player, and will ignore LOS to move to them
	UPROPERTY(EditAnywhere, Category = "Behavior, General")
		float AggroRange;

	/// Pointer to Base Blackboard Component
	UPROPERTY(transient)
		UBlackboardComponent* BaseBlackboard;

	/// Pointer to Base BehaviorTree Component
	UPROPERTY(transient)
		UBehaviorTreeComponent* BaseBehaviorTree;

	/// Pointer to Combat Blackboard Component
	UPROPERTY(transient)
		UBlackboardComponent* CombatBlackboard;

	/// Pointer to Combat BehaviorTree Component
	UPROPERTY(transient)
		UBehaviorTreeComponent* CombatBehaviorTree;

	/*-----------------------------------------------------------------------------
	Blackboard Updaters
	-----------------------------------------------------------------------------*/

	/// Checks location of CurrentEnemy within LoS; Updates BB key <STRONG>KnownEnemyLocation</STRONG><BR>
	/** Will eventually be called by other units for communication system
	*/
	UFUNCTION(BlueprintCallable, Category = "Behavior, General")
		void UpdateKnownEnemyLocation();

	/// Checks LoS of <STRONG>CurrentEnemy</STRONG>; Updates BB key <STRONG>HaveLoS</STRONG> <BR>
	/**Uses <CODE>LineOfSightTo</CODE>, which checks from center of unit to <CODE>EyePosition</CODE> of player. Might cause issues later; can null out z value
	*/
	UFUNCTION(BlueprintCallable, Category = "Behavior, General")
		void UpdateLoSOfP1();

	/// Checks if <STRONG>CurrentEnemy</STRONG> is within <STRONG>AggroRange</STRONG> Updates BB key <STRONG>EnemyInAggroRange</STRONG>
	UFUNCTION(BlueprintCallable, Category = "Behavior, General")
		void UpdateEnemyInAggroRange();

	/// Finds nearest 'safe point'; Updates BB key <STRONG>PointToFlee</STRONG>
	/**
	Safe points include:
	<UL>
	<LI>Enemy controlled point</LI>
	<LI>Units with highest 'threat' value</LI>
	<LI>Patrolling units</LI>
	</UL>
	*/
	UFUNCTION(BlueprintCallable, Category = "Behavior, Navigation")
		void UpdateSafePoint();

	/*-----------------------------------------------------------------------------
	AI COMMANDs
	-----------------------------------------------------------------------------*/

	/// Sends this unit to engage an enemy player or assault a point
	UFUNCTION(BlueprintCallable, Category = "Behavior, CommanderCallable")
		void COMMAND_EngageEnemy(UObject* Enemy);

	/// Called by Commander to make unit search for P1 around themselves
	UFUNCTION(BlueprintCallable, Category = "Behavior, CommanderCallable")
		void COMMAND_SeekEnemy(UObject* Enemy);

	/// Called by Commander to make unit search for P1 to and at a point
	UFUNCTION(BlueprintCallable, Category = "Behavior, CommanderCallable")
		void COMMAND_SeekEnemyAtPoint(UObject* Enemy, FVector SeekPoint);

	/// Called by Commander to send a unit to a specific point and guard it
	UFUNCTION(BlueprintCallable, Category = "Behavior, CommanderCallable")
		void COMMAND_GuardPoint(AActor* Objective, UObject* Enemy);

	/*-----------------------------------------------------------------------------
	Combat & Internal Functions
	-----------------------------------------------------------------------------*/

	/// Allows unit to change it's own state
	UFUNCTION(BlueprintCallable, Category = "Behavior, General")
		void ChangeOwnAIState(EBaseUnitAIStates AIState);
	/*
	UFUNCTION(BlueprintCallable, Category = "Behavior, Combat")
	void Flee();	// Will be called when morale <= threshold event fires from Character

	UFUNCTION(BlueprintCallable, Category = "Behavior, Combat")
	void Attack();

	UFUNCTION(BlueprintCallable, Category = "Behavior, Combat")
	void Block();

	UFUNCTION(BlueprintCallable, Category = "Behavior, Combat")
	void Reposition();
	*/
};