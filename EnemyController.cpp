//Copyright @ Will Burgess TODO, TAG_HACKY, TAG_DEBUG, TAG_SINGLEPLAYER

#include "HeavyMetalMarauders.h"
#include "EnemyController.h"
#include "BaseEnemyUnit.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CharacterBase.h"
#include "StaticLibrary.h"
#include "ObjectiveMajor.h"

// Constructor
AEnemyController::AEnemyController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// References to the Blackboard and BehaviorTree components
	BaseBlackboard = ObjectInitializer.CreateDefaultSubobject<UBlackboardComponent>(this, TEXT("BaseBlackboard"));
	BaseBehaviorTree = ObjectInitializer.CreateDefaultSubobject<UBehaviorTreeComponent>(this, TEXT("BaseBehaviorTree"));
	CombatBlackboard = ObjectInitializer.CreateDefaultSubobject<UBlackboardComponent>(this, TEXT("CombatBlackboard"));
	CombatBehaviorTree = ObjectInitializer.CreateDefaultSubobject<UBehaviorTreeComponent>(this, TEXT("CombatBehaviorTree"));
}

void AEnemyController::Possess(class APawn *InPawn)
{
	Super::Possess(InPawn);

	// Reference to type of unit possessed by this controller
	AIUnit = Cast<ABaseEnemyUnit>(InPawn);

	// Set Defaults
	AggroRange = 750.0f;

	// Turn RVO on
	GetCharacter()->GetCharacterMovement()->SetAvoidanceEnabled(true);

	// Set initial KnownEnemyLocation to player spawn location (currently set to TAG_DEBUG location of the spawner)
	BaseBlackboard->SetValue<UBlackboardKeyType_Vector>(KnownEnemyLocationID, FVector(2080.0f, -4020.0f, 200.0f));

	// Initialize and get the primary tree started
	if (AIUnit && AIUnit->UnitBehavior)
	{
		// Tie the Blackboard Asset (defined in the unit's blueprint) to this unit's Blackboard Component
		BaseBlackboard->InitializeBlackboard(*(AIUnit->UnitBehavior->BlackboardAsset));

		// Tie key references to BlackBoard values
		IsExecutingCommandID = BaseBlackboard->GetKeyID("IsExecutingCommand");
		CurrentEnemyID = BaseBlackboard->GetKeyID("CurrentEnemy");
		KnownEnemyLocationID = BaseBlackboard->GetKeyID("KnownEnemyLocation");
		PointToGuardID = BaseBlackboard->GetKeyID("PointToGuard");
		HaveLoSID = BaseBlackboard->GetKeyID("HaveLoS");
		AIStateID = BaseBlackboard->GetKeyID("AIState");
		EnemyInAggroRangeID = BaseBlackboard->GetKeyID("EnemyInAggroRange");
		EnemyInGuardAreaID = BaseBlackboard->GetKeyID("EnemyInGuardArea");
		EnemyInLeashRangeID = BaseBlackboard->GetKeyID("EnemyInLeashRange");
		GuardedObjectiveID = BaseBlackboard->GetKeyID("GuardedObjective");
		PointToFleeID = BaseBlackboard->GetKeyID("PointToFlee");

		// Start the designated BehaviorTree for this unit
		BaseBehaviorTree->StartTree(*AIUnit->UnitBehavior);
		RunBehaviorTree(AIUnit->UnitBehavior);
	}

	// Initialize the Combat subtree TODO
	if (AIUnit && AIUnit->CombatBehavior)
	{
		// Tie the Blackboard Asset (defined in the unit's blueprint) to this unit's Blackboard Component
		BaseBlackboard->InitializeBlackboard(*(AIUnit->CombatBehavior->BlackboardAsset));

		// Tie key references to BlackBoard values

	}

	/*-----------------------------------------------------------------------------
	Build references to in-game Objects
	-----------------------------------------------------------------------------*/

	// #Multiplayer version of PlayerCharacter reference builder
	for (TActorIterator<ACharacterBase> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		// Try GetName().Contains()? This is #HACKY - need better solution for initializing who the player is.
		// New solution should support a new player being set as well, and switching between the two per Commander AI
		
		PlayerInstance = *ActorItr;
	}

	// Get an array of references to Objectives
	for (TActorIterator<AObjectiveMajor> ObjectiveItr(GetWorld()); ObjectiveItr; ++ObjectiveItr)
	{
		if (ObjectiveItr->GetName().Contains(FString(TEXT("ObjectiveMaj"))))
		{
			UE_LOG(LogTemp, Warning, TEXT("Objective's class name: %s"), *ObjectiveItr->GetName());
			MajorObjectives.Add(*ObjectiveItr);
		}
	}

	/*-----------------------------------------------------------------------------
	Default Blackboard Key Values
	-----------------------------------------------------------------------------*/

}

void AEnemyController::BeginPlay()
{
	Super::BeginPlay();
}

/// Used to control what happens when this unit completes a move request
void AEnemyController::OnMoveCompleted(FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{

	// If COMMAND_Guard is issuing a ForceMove and move was successful 
	if (ForceMove_Guard && Result.Code == EPathFollowingResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("Activating Guard tree"));

		// Set state key to Guard
		BaseBlackboard->SetValue<UBlackboardKeyType_Enum>(AIStateID,
			static_cast<UBlackboardKeyType_Enum::FDataType>(EBaseUnitAIStates::AIGuarding));

		// No longer ForceMoving
		ForceMove_Guard = false;
	}
}

/*-----------------------------------------------------------------------------
Blackboard Updaters
-----------------------------------------------------------------------------*/

void AEnemyController::UpdateKnownEnemyLocation()
{
	// TAG_SINGLEPLAYER - relies on PlayerInstance variable
	BaseBlackboard->SetValue<UBlackboardKeyType_Vector>(KnownEnemyLocationID, PlayerInstance->GetActorLocation());
}

void AEnemyController::UpdateLoSOfP1()
{
	//UE_LOG(LogTemp, Warning, TEXT("Inside UpdateLoSofP1"));

	// Get AActor* instance of targetPlayer 
	const AActor* TargetPlayer = Cast<AActor>(BaseBlackboard->GetValue<UBlackboardKeyType_Object>(CurrentEnemyID));

	bool LoSResult = LineOfSightTo(TargetPlayer, FVector(0.0f, 0.0f, 0.0f), true);

	if (LoSResult == true)
	{
		UE_LOG(LogTemp, Warning, TEXT("HAVE LINE OF SIGHT"));

		// Update HasLoS key
		BaseBlackboard->SetValue<UBlackboardKeyType_Bool>(HaveLoSID, true);

		// Update KnownEnemyLocation key
		UpdateKnownEnemyLocation();
	}
	else if (LoSResult == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("CRAP, I LOST HIM"));
		BaseBlackboard->SetValue<UBlackboardKeyType_Bool>(HaveLoSID, false);
	}
}

void AEnemyController::UpdateEnemyInAggroRange()
{
	// Get AACtor* instance of Enemy
	const AActor* Enemy = Cast<AActor>(BaseBlackboard->GetValue<UBlackboardKeyType_Object>(CurrentEnemyID));

	// .SizeSquare is faster, but to be used for comparisons only (does not return actual distance)
	// It just gets rid of negative numbers; .Size does the same, then Sqrt()'s it returning actual distance
	float DistanceToEnemy = (GetCharacter()->GetActorLocation() - Enemy->GetActorLocation()).SizeSquared();

	// Compare distance b/t unit and enemy versus threshold
	if (DistanceToEnemy <= (AggroRange * AggroRange))
	{
		BaseBlackboard->SetValue<UBlackboardKeyType_Bool>(EnemyInAggroRangeID, true);
	}
	else if (DistanceToEnemy > (AggroRange * AggroRange))
	{
		BaseBlackboard->SetValue<UBlackboardKeyType_Bool>(EnemyInAggroRangeID, false);
	}
}

void AEnemyController::UpdateSafePoint()
{

}

/*-----------------------------------------------------------------------------
AI COMMANDs
-----------------------------------------------------------------------------*/

void AEnemyController::COMMAND_EngageEnemy(UObject* Enemy)
{
	// Set CurrentEnemy to instanced player object
	BaseBlackboard->SetValue<UBlackboardKeyType_Object>(CurrentEnemyID, Enemy);

	// Set state key to Engage
	BaseBlackboard->SetValue<UBlackboardKeyType_Enum>(AIStateID,
		static_cast<UBlackboardKeyType_Enum::FDataType>(EBaseUnitAIStates::AIEngaging));
}

void AEnemyController::COMMAND_GuardPoint(AActor* Objective, UObject* Enemy)
{
	// Set AIState to invalid to prevent BehaviorTree overriding initial move to Point
	BaseBlackboard->ClearValue(AIStateID);

	// Get random reachable Point within Objective's GuardArea
	TArray<UActorComponent*> ObjectiveBoxArray = Objective->GetComponentsByTag(UBoxComponent::StaticClass(), "Guard");
	FVector GuardPoint = FMath::RandPointInBox(Cast<USceneComponent>(ObjectiveBoxArray[0])->Bounds.GetBox());

	// Issuing a ForceMove
	ForceMove_Guard = true;

	// Make unit go to the randomized location inside Trigger component
	MoveToLocation(GuardPoint);

	// #TODO - Change unit's speed (or movement state) to SPRINTING since the unit won't be attacking players while doing this

	//UE_LOG(YourLog, Warning, TEXT("Moving to offset Point: %s"), NavLocationData.Location.ToString());

	// Set PointToGuard to Point
	BaseBlackboard->SetValue<UBlackboardKeyType_Vector>(PointToGuardID, GuardPoint);

	// Set CurrentEnemy to instanced player object
	BaseBlackboard->SetValue<UBlackboardKeyType_Object>(CurrentEnemyID, Enemy);

	// Set GuardedObjective
	BaseBlackboard->SetValue<UBlackboardKeyType_Object>(GuardedObjectiveID, Objective);

	// NOTE - OnMoveCompleted() has a handler that will change the unit's state based on ForceMove_Guard
	// TODO (feature) - overload this command to provide a way that changes the state without requiring initial move
}

void AEnemyController::COMMAND_SeekEnemy(UObject* Enemy)
{
	// Set CurrentEnemy to instanced player object
	BaseBlackboard->SetValue<UBlackboardKeyType_Object>(CurrentEnemyID, Enemy);

	// Set state key to Seek
	BaseBlackboard->SetValue<UBlackboardKeyType_Enum>(AIStateID,
		static_cast<UBlackboardKeyType_Enum::FDataType>(EBaseUnitAIStates::AISeeking));
}

void AEnemyController::COMMAND_SeekEnemyAtPoint(UObject* Enemy, FVector SeekPoint)
{
	// Set CurrentEnemy to instanced player object
	BaseBlackboard->SetValue<UBlackboardKeyType_Object>(CurrentEnemyID, Enemy);

	// Set state key to Seek
	BaseBlackboard->SetValue<UBlackboardKeyType_Enum>(AIStateID,
		static_cast<UBlackboardKeyType_Enum::FDataType>(EBaseUnitAIStates::AISeeking));

	// Send unit to specified point
	MoveToLocation(SeekPoint);
}

// REF - UBTTask_BlackboardBase
// REF - UBTTask_MoveTo
void AEnemyController::ChangeOwnAIState(EBaseUnitAIStates AIState)
{
	BaseBlackboard->SetValue<UBlackboardKeyType_Enum>(AIStateID,
		static_cast<UBlackboardKeyType_Enum::FDataType>(AIState));
}

