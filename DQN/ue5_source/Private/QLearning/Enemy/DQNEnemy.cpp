
#include "QLearning/Enemy/DQNEnemy.h"
#include "Items/Weapons/Weapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Characters/KnightCharacter.h"
#include "AIController.h"

#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"









ADQNEnemy::ADQNEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	
}

void ADQNEnemy::BeginPlay()
{
	ABaseCharacter::BeginPlay();

	/* Enemy BeginPlay */
	EnemyController = Cast<AAIController>(GetController());

	Tags.Add(FName("Enemy"));
	Tags.Add(FName("QEnemy"));
	Tags.Add(FName("DQNEnemy"));

	ShowHealthBar();

	if (UWorld* World = GetWorld(); World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		DefaultWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		EquippedWeapon = DefaultWeapon;
	}
	
	FindQTarget();
	if (QTarget) CombatTarget = QTarget; // needed?

	/* DQN Networking */
	if (!Comm.Init())	UE_LOG(LogTemp, Error, TEXT("[DQN] Socket init failed"));
						// optionally destroy


	/* Server Socket Echo (UE config parameters (Num States and Actions)) */
	FString Ping = TEXT("{\"ping\":\"hello from UE\", \"NumStates\":\"8\", \"NumActions\":\"5\"}");
	if (Comm.SendJson(Ping))
		UE_LOG(LogTemp, Display, TEXT("[DQN] Sent: %s"), *Ping);

	FString Pong;
	if (Comm.RecvJson(Pong))
		UE_LOG(LogTemp, Display, TEXT("[DQN] Received: %s"), *Pong);
	

	/* Set initial Server state */
	FString InitialPacket = BuildPacket(QState->ToFloatArray(), 0, IsDone());
	Comm.SendJson(InitialPacket);
}

void ADQNEnemy::Tick(float DeltaTime)
{
	ABaseCharacter::Tick(DeltaTime);

	static float ChaseAccumulator = 0.f;
	ChaseAccumulator += DeltaTime;

	if (ChaseAccumulator > 0.25f)
	{
		ChaseAccumulator = 0.f;
		if (CanChaseQTarget()) ChaseQTarget(); // Automated Basic Movement 
	}

	if (QTarget && (IsAttacking() || IsGuarding()))
	{
		InterpRotationToTarget(QTarget, DeltaTime, /*Rotation Speed*/7.5f); // Rotate To Target While Attacking or Guarding
	}
	
	static float AccumulatedTime = 0.f;
	AccumulatedTime += DeltaTime;

	if (AccumulatedTime > DqnUpdateCooldown)
	{
		AccumulatedTime = 0.f;
		UpdateFunction_Phase1(); // start a new decision cycle
	}
	
}



void ADQNEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ABaseCharacter::EndPlay(EndPlayReason);
	EndTraining(); // Send 'done' flag to server
}

void ADQNEnemy::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("DQN Enemy Die Triggered"));
	AddQReward(QLearningRewards::DeathPenalty);
	UpdateFunction_Phase2();

	// Allow additional training
	if (--TrainingDeaths <= 0)
		Super::Die();
	else
	{
		Attributes->SetHealthAmount(Attributes->GetMaxHealth());
		Attributes->SetHealPotionsLeft(Attributes->GetMaxNumHealPotions()); 
	}
}


void ADQNEnemy::CheckRepeatActions()
{
	if (ChosenQAction == PrevQAction) RepeatActionsCounter++;
	else RepeatActionsCounter = 0;
	if (RepeatActionsCounter > 3) AddQReward(QLearningRewards::RepeatedActionPenalty * RepeatActionsCounter);
}

void ADQNEnemy::UpdateFunction_Phase1()
{
	if (bWaitingForActionCompletion) return;

	CheckRepeatActions();
	
	PrevQAction = ChosenQAction;
	
	/// Try to recieve next action ID
	uint32 ActionId = 4;//UINT32_MAX; //4-Wait
	if (Comm.RecvUInt32(ActionId))
	{
		ChosenQAction = static_cast<EQAction>(ActionId);
		PerformAction(ChosenQAction); // make sure ActionID in range [0, max(EQAction)]
		
		GetWorldTimerManager().SetTimer(FallbackPhase2Timer, this, &ADQNEnemy::FallbackPhase2, 2.f, false); // Safety fallback: force phase2 after timeout

		UE_LOG(LogTemp, Log, TEXT("[DQN] Phase1: Executed action %u"), ActionId);
		UE_LOG(LogTemp, Display, TEXT("[DQN] Phase1: Executed action %u"), ActionId);
	}
	
}

void ADQNEnemy::UpdateFunction_Phase2()
{
	UpdateQState();

	/// Build & send the transition packet
	FString Packet = BuildPacket(QState->ToFloatArray(), GetReward(), IsDone());
	Comm.SendJson(Packet);

	/// Cleanup for next decision
	PendingRewards.Empty();
	bWaitingForActionCompletion = false;
	ClearFallbackPhase2Timer();
	
	UE_LOG(LogTemp, Log, TEXT("[DQN] Phase2: Sent"));
	UE_LOG(LogTemp, Display, TEXT("[DQN] Phase2: Sent"));
	
}

void ADQNEnemy::EndTraining()
{
	UpdateQState();

	FString Packet = BuildPacket(QState->ToFloatArray(), GetReward(), true); // DONE FLAG SET
	Comm.SendJson(Packet);

	PendingRewards.Empty();
	bWaitingForActionCompletion = false;
	ClearFallbackPhase2Timer();
	
	UE_LOG(LogTemp, Log, TEXT("[DQN] Phase2: Sent"));
	UE_LOG(LogTemp, Display, TEXT("[DQN] Phase2: Sent"));

	//SetLifeSpan(0.5f); // called in EndPlay so not needed (when using multiple training deaths)
}


FString ADQNEnemy::BuildPacket(const TArray<float>& StateObs, const float Reward, const bool bDone)
{
	/// Wrap as JSON values
	TArray<TSharedPtr<FJsonValue>> JsonArr;
	JsonArr.Reserve(StateObs.Num());
	for (float Val : StateObs)
	{
		JsonArr.Add(MakeShared<FJsonValueNumber>(Val));
	}

	/// Build the JSON object
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetArrayField(TEXT("state"), JsonArr);
	Obj->SetNumberField(TEXT("reward"), Reward);
	Obj->SetBoolField(TEXT("done"), bDone);

	/// Serialize to string
	FString Packet;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Packet);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Packet;
	
}




























































