#include "QLearning/Enemy/QLearningEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AttributeComponent.h"
#include "Components/BoxComponent.h"
#include "Characters/KnightCharacter.h"
#include "Kismet/GameplayStatics.h"

// Might not be needed
// --------------------------------------------------------------------------------------------------
#include "UdemyActionRPG/DebugMacros.h"
#include "Components/CapsuleComponent.h"
#include "HUD/HealthBarComponent.h"
#include "AIController.h"
#include "Items/Weapons/Weapon.h"
// --------------------------------------------------------------------------------------------------




AQLearningEnemy::AQLearningEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	
}


void AQLearningEnemy::BeginPlay()
{
	ABaseCharacter::BeginPlay();

	/* Enemy BeginPlay */
	EnemyController = Cast<AAIController>(GetController());

	Tags.Add(FName("Enemy"));
	Tags.Add(FName("QEnemy"));

	ShowHealthBar();

	if (UWorld* World = GetWorld(); World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		DefaultWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		EquippedWeapon = DefaultWeapon;
	}

	/* Q Learning BeginPlay */
	LoadQTableFromDisk();
	DisplayQTable();
	
	//FindQManager();
	
	FindQTarget();
	if (QTarget) CombatTarget = QTarget; // needed?
}

void AQLearningEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if (!IsUsingSharedTable()) SaveQTableToDisk();
	else SubmitQTableToManager();
}

void AQLearningEnemy::Tick(float DeltaTime)
{
	ABaseCharacter::Tick(DeltaTime);

	static float ChaseAccumulator = 0.f;
	ChaseAccumulator += DeltaTime;

	if (ChaseAccumulator > 0.25f)
	{
		ChaseAccumulator = 0.f;
		if (CanChaseQTarget()) ChaseQTarget(); // Automated Basic Movement - Add as State in Next Project
	}

	if (QTarget && (IsAttacking() || IsGuarding()))
	{
		InterpRotationToTarget(QTarget, DeltaTime, /*Rotation Speed*/7.5f); // Rotate To Target While Attacking or Guarding - Add as State in Next Project
	}
	
	static float AccumulatedTime = 0.f;
	AccumulatedTime += DeltaTime;

	if (AccumulatedTime > QUpdateIntervalSecs) // && !bWaitingForActionCompletion
	{
		AccumulatedTime = 0.f;
		UpdateFunction_Phase1(); // start a new decision cycle
	}
	
}

void AQLearningEnemy::GetHit_Implementation(const FVector& ImpactPoint)
{
	if (IsAlive())
	{
		DirectionalHitReact(ImpactPoint); // CHECK (hit montage issues?)
	}
	else
	{
		Die();
	}

	ShowHealthBar();
	PlayHitSound(ImpactPoint);
	SpawnHitParticles(ImpactPoint);

	UpdateQState_WasHitRecently();
	AddQReward(QLearningRewards::GotHit);
	
	//UpdateFunction(); // TESTING PLACEMENT
}


/* Q Learning Update Loop */
void AQLearningEnemy::UpdateFunction_Phase1()
{
	if (bWaitingForActionCompletion) return;

	// 1. Save current state to PrevQState
	UpdateQState();
	PrevQState = MakeUnique<FQState>(*QState); // Deep copy of current state
	PrevQAction = ChosenQAction;

	// 2. Choose action (ε-greedy)
	ChosenQAction = ChooseAction(*PrevQState);

	// 3. Perform action
	PerformAction(ChosenQAction); 	// #. Set flag to wait for completion -> Now set in PerformAction

	// 4. Set fallback timer to auto-call Phase2 after a timeout
	GetWorldTimerManager().SetTimer(FallbackPhase2Timer, this, &AQLearningEnemy::FallbackPhase2, 2.f, false);

	UE_LOG(LogTemp, Warning, TEXT("Phase 1 Executed"));
}

void AQLearningEnemy::UpdateFunction_Phase2()
{
	if (!PrevQState.IsValid() || !QState.IsValid()) return;

	UpdateQState();
	const FQState NewState = *QState;
	const float Reward = GetReward();

	UpdateQValue(*PrevQState, ChosenQAction, Reward, NewState);
	
	PendingRewards.Empty();
	bWaitingForActionCompletion = false;
	ClearFallbackPhase2Timer();

	UE_LOG(LogTemp, Warning, TEXT("Phase2: [%s] -> [%s] | Action: %s | Reward: %.2f"),
		*PrevQState->ToString(),
		*NewState.ToString(),
		*StaticEnum<EQAction>()->GetNameStringByValue(static_cast<int32>(ChosenQAction)),
		Reward);
	
}

EQAction AQLearningEnemy::ChooseAction(const FQState& State) // State - EncodedState, Epsilon - 0 to 1 - exploration rate
{
	float Epsilon = QExplorationRate;
	if (!QTable.Contains(State))
	{
		// Initialize default Q-values for all actions
		QTable.Add(State, {
			{ EQAction::Attack, 0.f },
			{ EQAction::Guard, 0.f },
			{ EQAction::Dodge, 0.f },
			{ EQAction::Heal, 0.f },
			{ EQAction::Wait, 0.f }
		});
	}

	// Exploration
	if (FMath::RandRange(0,1) < Epsilon)
	{
		int32 Index = FMath::RandRange(0, static_cast<int32>(EQAction::Wait)); // Random Action
		return static_cast<EQAction>(Index);
	}

	// Exploitation
	TMap<EQAction, float>& ActionValues = QTable[State];
	EQAction BestAction = EQAction::Wait;
	float BestValue = -FLT_MAX;
	for (const auto& Pair : ActionValues)
	{
		if (Pair.Value > BestValue)
		{
			BestValue = Pair.Value;
			BestAction = Pair.Key;
		}
	}
	return BestAction;
}

void AQLearningEnemy::UpdateQValue(const FQState& PrevState, EQAction ActionTaken, float Reward,
	const FQState& NewState) // Update Rule
{
	float Alpha = QLearningRate; // Learning rate
	float Gamma = QDiscountFactor; // discount factor

	if (!QTable.Contains(PrevState))
		return;

	if (!QTable.Contains(NewState))
	{
		// Initialize all actions
		QTable.Add(NewState, {
			{ EQAction::Attack, 0.f },
			{ EQAction::Guard, 0.f },
			{ EQAction::Dodge, 0.f },
			{ EQAction::Heal, 0.f },
			{ EQAction::Wait, 0.f }
		});
	}

	const float OldQ = QTable[PrevState][ActionTaken];

	float MaxFutureQ = -FLT_MAX;
	for (const auto& Pair : QTable[NewState]) // Choose Action
	{
		MaxFutureQ = FMath::Max(MaxFutureQ, Pair.Value);
	}

	const float NewQ = OldQ + Alpha * (Reward + Gamma * MaxFutureQ - OldQ);
	QTable[NewState][ActionTaken] = NewQ;
}



void AQLearningEnemy::PerformAction(const EQAction Action)
{
	bWaitingForActionCompletion = true;
	switch (Action)
	{
		case EQAction::Attack: SetToAttacking(); UE_LOG(LogTemp, Warning, TEXT("Perform Attack"));
			break;
		case EQAction::Guard: SetToGuarding(); UE_LOG(LogTemp, Warning, TEXT("Perform Guard"));
			break;
		case EQAction::Dodge: SetToDodging(); UE_LOG(LogTemp, Warning, TEXT("Perform Dodge"));
			break;
		case EQAction::Heal: SetToHealing(); UE_LOG(LogTemp, Warning, TEXT("Perform Healing"));
			break;
		case EQAction::Wait: SetToWaiting(); UE_LOG(LogTemp, Warning, TEXT("Perform Wait"));
			
		default:
			break;
	}
}


float AQLearningEnemy::GetReward()
{
	if (PendingRewards.IsEmpty()) return 0.f;
	float Reward = 0.f;
	for (const auto Value : PendingRewards)
	{
		Reward += Value;
	}
	return Reward;
}

/* QState Value Update Functions */

void AQLearningEnemy::UpdateQState()
{
	UpdateQState_UpdateTargetValues();
	UpdateQState_HealthPercent();
	UpdateQState_HealsLeft();
	UpdateQState_DistanceValues();
	// Was hit - updated in Get_Hit (timer-based)
}


void AQLearningEnemy::UpdateQState_TargetValues(const float TargetHealthPercent, const bool bIsTargetAttacking, const bool bIsTargetGuarding,
	const bool bIsTargetDodging)
{
	QState->TargetHealthPercent = static_cast<uint8>(100 * TargetHealthPercent);

	QState->bIsTargetAttacking = bIsTargetAttacking;
	
	QState->bIsTargetGuarding = bIsTargetGuarding;

	QState->bIsTargetDodging = bIsTargetDodging;
	
}



void AQLearningEnemy::UpdateQState_HealthPercent()
{
	if (const float HealthPercent = Attributes->GetHealthPercent())
	{
		QState->HealthPercent = static_cast<uint8>(100 * HealthPercent);
	}
}

void AQLearningEnemy::UpdateQState_HealsLeft()
{
	QState->HealsLeft = Attributes->GetNumHealPotionsLeft();
}

void AQLearningEnemy::UpdateQState_DistanceValues()
{
	if (!QTarget) return;
	
	const float Distance = FVector::Dist(GetActorLocation(), QTarget->GetActorLocation());
	
	QState->bIsInAttackRange = Distance < QAttackRadius;
}


void AQLearningEnemy::UpdateQState_WasHitRecently()
{
	QState->bWasHitRecently = true;
	GetWorldTimerManager().ClearTimer(HitRecentlyTimer);
	GetWorldTimerManager().SetTimer(HitRecentlyTimer, FTimerDelegate::CreateLambda([this]()
	{
		QState->bWasHitRecently = false;
	}), HitRecentlyDurationSecs, false);
}

void AQLearningEnemy::UpdateQState_UpdateTargetValues()
{
	if (auto* Player = Cast<AKnightCharacter>(QTarget))
	{
		UpdateQState_TargetValues(Player->GetHealthPercent(), Player->GetIsAttacking(), Player->GetIsGuarding(), Player->GetIsDodging());
	}
}


/* Targeting + Automated Movement */

void AQLearningEnemy::FindQTarget()
{
	TArray<AActor*> FoundTargets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseCharacter::StaticClass(), FoundTargets);

	for (AActor* Actor : FoundTargets)
	{
		if (AKnightCharacter* PlayerTarget = Cast<AKnightCharacter>(Actor); PlayerTarget)
		{
			QTarget = PlayerTarget;
			return;
		}
		if (ABaseCharacter* CharacterTarget = Cast<ABaseCharacter>(Actor); CharacterTarget)
		{
			//QTarget = CharacterTarget; // TODO: Only Accepts AKnightCharacter ATM 
		}
	}
}

void AQLearningEnemy::ChaseQTarget()
{
	//if (EnemyState != EEnemyState::EES_NoState || InTargetRange(QTarget, AttackRadius)) return;
	if (IsValid(QTarget) && InTargetRange(QTarget, QAttackRadius)) return;
	
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeed;
	MoveToTarget(QTarget, QAttackRadius/2); // Acceptance Radius, Issue: Enemy attacking too close
	
}

bool AQLearningEnemy::CanChaseQTarget()
{
	return !InTargetRange(QTarget, QAttackRadius) && !IsGuarding() && !IsDodging() && !IsAttacking(); // condense?
}


/* Actions + Enemy Overrides */

void AQLearningEnemy::SetToAttacking() // Blocking
{
	//bBlockUpdateLoop = true; // in Phase1, Phase2 resets it
	
	EnemyState = EEnemyState::EES_Attacking;
	if (InTargetRange(QTarget, QAttackRadius)) AddQReward(QLearningRewards::AttackInAttackRange);
	else AddQReward(QLearningRewards::AttackOutsideAttackRange);
	Attack();
}

void AQLearningEnemy::SetToGuarding() // Blocking
{
	//bBlockUpdateLoop = true;
	if		(IsOutsideCombatRadius()) AddQReward(QLearningRewards::GuardOutsideCombatRange);
	else if (IsOutsideAttackRadius()) AddQReward(QLearningRewards::GuardOutsideAttackRange);
	
	EnemyState = EEnemyState::EES_Guarding;
	StartGuardTimer();
}

void AQLearningEnemy::SetToDodging() // Blocking
{
	//bBlockUpdateLoop = true;
	
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); // I-FRAMES // testing

	EnemyState = EEnemyState::EES_Dodging;
	Dodge();
}

void AQLearningEnemy::SetToHealing() // Synchronous
{
	/* Healing Rewards */
	if (Attributes->GetNumHealPotionsLeft() > 0)	Attributes->Heal();
	else AddQReward(QLearningRewards::HealAttemptWithNoPotionsLeft);

	if (Attributes->GetHealthPercent() > 0.75f) AddQReward(QLearningRewards::HealedAtHighHealth);
	else if (Attributes->GetHealthPercent() < 0.5f) AddQReward(QLearningRewards::HealedAtLowHealth);


	//bWaitingForActionCompletion = false; // in Phase2
	
	UpdateFunction_Phase2();
	//ClearFallbackPhase2Timer();

	EnemyState = EEnemyState::EES_NoState;
}

void AQLearningEnemy::SetToWaiting() // Synchronous at the moment
{
	// EnemyState =
	//bBlockUpdateLoop = true;
	//bWaitingForActionCompletion = false; // in Phase2

	/*
	FTimerHandle WaitHandle;
	GetWorldTimerManager().SetTimer(WaitHandle,[this]()
	{
		bBlockUpdateLoop = false;
		EnemyState = EEnemyState::EES_NoState;
	}, 0.5f, false); // ~0.5s penalty duration (Update loop interval greater than this time so may not matter)
	*/

	UpdateFunction_Phase2();
	//ClearFallbackPhase2Timer();

	EnemyState = EEnemyState::EES_NoState;
}

void AQLearningEnemy::Die()
{
	Super::Die();
	AddQReward(QLearningRewards::DeathPenalty);
	UpdateFunction_Phase2(); // check
	
	ClearAllTimers(); // test placement
}


void AQLearningEnemy::OnAttackEnd()
{
	UpdateFunction_Phase2();
	//ClearFallbackPhase2Timer();
	
	EnemyState = EEnemyState::EES_NoState;
}

void AQLearningEnemy::OnDodgeEnd()
{
	AddQReward(QLearningRewards::Dodge); // encourage dodges (small reward)
	
	UpdateFunction_Phase2();
	//ClearFallbackPhase2Timer();

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // I-FRAMES // testing

	EnemyState = EEnemyState::EES_NoState;
}

void AQLearningEnemy::ClearGuardTimer()
{
	if (GuardBoxComponent) GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // or in ABP?
	GetWorldTimerManager().ClearTimer(GuardTimer);
	StopMontage(GuardMontage);

	UpdateFunction_Phase2();
	//ClearFallbackPhase2Timer();

	EnemyState = EEnemyState::EES_NoState;
}






/* Storage */
void AQLearningEnemy::SaveQTableToDisk()
{
	const FString SavePath = FPaths::ProjectSavedDir() + QFilename;

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	for (const auto& StatePair : QTable)
	{
		const FString StateKey = StatePair.Key.ToString();
		TSharedPtr<FJsonObject> ActionMap = MakeShareable(new FJsonObject);

		for (const auto& ActionPair : StatePair.Value)
		{
			const FString ActionStr = FString::FromInt(static_cast<int32>(ActionPair.Key));
			ActionMap->SetNumberField(ActionStr, ActionPair.Value);
		}

		RootObject->SetObjectField(StateKey, ActionMap);
	}

	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	// Save to file
	FFileHelper::SaveStringToFile(OutputString, *SavePath);
	UE_LOG(LogTemp, Warning, TEXT("Saved Q-Table: %s"), *SavePath);
}

void AQLearningEnemy::LoadQTableFromDisk()
{
	const FString LoadPath = FPaths::ProjectSavedDir() + QFilename;
	FString FileContents;

	if (!FFileHelper::LoadFileToString(FileContents, *LoadPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("No Q-Table found at: %s"), *LoadPath);
		return;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	TSharedPtr<FJsonObject> RootObject;

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse Q-Table JSON"));
		return;
	}

	QTable.Empty();

	for (const auto& StatePair : RootObject->Values)
	{
		FQState StateKey = FQState::FromString(StatePair.Key);
		TSharedPtr<FJsonObject> ActionMap = StatePair.Value->AsObject();

		TMap<EQAction, float> Actions;

		for (const auto& ActionPair : ActionMap->Values)
		{
			const EQAction Action = static_cast<EQAction>(FCString::Atoi(*ActionPair.Key));
			const float QValue = ActionPair.Value->AsNumber();
			Actions.Add(Action, QValue);
		}

		QTable.Add(StateKey, Actions);
	}

	UE_LOG(LogTemp, Warning, TEXT("Loaded Q-Table: %s"), *LoadPath);
}

void AQLearningEnemy::FindQManager()
{
	TArray<AActor*> FoundTargets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQLearningManager::StaticClass(), FoundTargets);

	for (AActor* Actor : FoundTargets)
	{
		if (AQLearningManager* Manager = Cast<AQLearningManager>(Actor))
		{
			QManager = Manager;
		}
	}
}

void AQLearningEnemy::SubmitQTableToManager() const
{
	//if (!QManager) return;
	//QManager->MergeQTableFromEnemy(QTable);

	if (UWorld* World = GetWorld())
	{
		if (AQLearningManager* Manager = AQLearningManager::Get(World))
		{
			Manager->MergeQTableFromEnemy(QTable);
			UE_LOG(LogTemp, Warning, TEXT("Submitted QTable to %s from %s"), *Manager->GetName(), *GetName());
		}
	}
	
}

void AQLearningEnemy::FallbackPhase2()
{
	if (bWaitingForActionCompletion)
	{
		UE_LOG(LogTemp, Warning, TEXT("FallbackPhase2 triggered — action may have stalled at %s"), *GetName());
		UpdateFunction_Phase2();
	}
}


void AQLearningEnemy::ClearAllTimers()
{
	ClearPatrolTimer();
	ClearAttackTimer();
	ClearGuardTimer();
	ClearFallbackPhase2Timer();
}

/* Display */
void AQLearningEnemy::DisplayQTable() const
{
	UE_LOG(LogTemp, Warning, TEXT("========= Q TABLE ========="));
	for (const auto& StatePair : QTable)
	{
		const FString& State = StatePair.Key.ToString();
		const TMap<EQAction, float>& ActionMap = StatePair.Value;

		UE_LOG(LogTemp, Warning, TEXT("State: %s"), *State);

		for (const auto& ActionPair : ActionMap)
		{
			const FString ActionName = StaticEnum<EQAction>()->GetNameStringByValue(static_cast<int64>(ActionPair.Key));
			UE_LOG(LogTemp, Warning, TEXT("   %s: %.2f"), *ActionName, ActionPair.Value);
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("============================"));
}


void AQLearningEnemy::DisplayQState() const
{
	UE_LOG(LogTemp, Warning, TEXT("Current Q State: %s"), *QState->ToString());
}

















// ----------------- Older Version Functions -----------------------
/*FString AQLearningEnemy::EncodeState() const
{
	return FString::Printf(TEXT("%.2f_%.2f_%.2f_%d_%d_%d"),
		QState->HealthPercent,
		QState->TargetHealthPercent,
		QState->DistanceToTarget,
		QState->bIsTargetAttacking ? 1 : 0,
		QState->bIsTargetGuarding ? 1 : 0,
		QState->bWasHitRecently ? 1 : 0
	);
}*/



/*
void AQLearningEnemy::UpdateFunction()
{

	// Testing Phase
	//UpdateFunction_Phase1();
	//if (bWaitingForActionCompletion) return; // testing placement

	//UpdateFunction_Phase2(); // testing placement
	
	/*
	// 1. Encode current state before taking action
	const FQState PrevState = *QState;

	// 2. Choose an action using ε-greedy
	const EQAction ActionTaken = ChooseAction(PrevState);

	// 3. Save action (optional but useful for logging/debug)
	*QAction = ActionTaken;

	// 4. Perform the action (this will likely change the game state asynchronously)
	PerformAction(ActionTaken);

	// Timeout. Asynch behavior from Action animation times, must wait for it to synch or reward and state will be the same 
	
	
	// 5. Accumulate reward(s)
	const float Reward = GetReward();

	// 6. Encode resulting state (may not fully reflect real result yet!)
	const FQState NewState = *QState;

	// 7. Update Q-table using learning rule
	UpdateQValue(PrevState, ActionTaken, Reward, NewState);

	// 8. Reset reward buffer
	PendingRewards.Empty();

	UE_LOG(LogTemp, Warning, TEXT("UpdateFunction: [%s] Action: %s | Reward: %.2f -> %s"),
		*PrevState.ToString(),
		*StaticEnum<EQAction>()->GetNameStringByValue(static_cast<int32>(ActionTaken)),
		Reward,
		*NewState.ToString());
	// * / here
	

  }
 */