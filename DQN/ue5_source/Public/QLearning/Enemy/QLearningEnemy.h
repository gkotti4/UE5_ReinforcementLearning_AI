
#pragma once

#include "CoreMinimal.h"
#include "Enemy/Enemy.h"
#include "GameFramework/Character.h"
#include "QLearning/QLearningManager.h"
#include "QLearning/QLearningTypes.h"
#include "QLearningEnemy.generated.h"




UCLASS()
class UDEMYACTIONRPG_API AQLearningEnemy : public AEnemy
{
	GENERATED_BODY()

public:
	AQLearningEnemy();
	virtual void Tick(float DeltaTime) override;
	virtual void GetHit_Implementation(const FVector& ImpactPoint) override; // From HitInterface

	bool bWaitingForActionCompletion = false;

	TMap<FQState, TMap<EQAction, float>> QTable; // QTable[State][Action] = QValue
	TUniquePtr<FQState> QState = MakeUnique<FQState>();
	TUniquePtr<FQState> PrevQState = MakeUnique<FQState>();
	EQAction ChosenQAction = EQAction::Wait;
	EQAction PrevQAction;
	TArray<float> PendingRewards;


	/* Main Update Functions */
	virtual void UpdateFunction_Phase1();
	virtual void UpdateFunction_Phase2();

	//virtual void InstantUpdate(); // consume reward immediately for quick asynchronous events

	int32 RepeatActionsCounter = 0;
	EQAction ChooseAction(const FQState& State);
	void UpdateQValue(const FQState& PrevState, EQAction ActionTaken, float Reward, const FQState& NewState);

	
	/* Action */
	void PerformAction(EQAction Action);
	
	/* Q Learning Update Helper Functions */
	void UpdateQState();
	void UpdateQState_TargetValues(float TargetHealthPercent, bool bIsTargetAttacking, bool bIsTargetGuarding, bool bIsTargetDodging);
	void UpdateQState_UpdateTargetValues();
	void UpdateQState_HealthPercent();
	void UpdateQState_HealsLeft();
	void UpdateQState_DistanceValues();
	void UpdateQState_WasHitRecently();

	void AddQReward(const float Value) { PendingRewards.Add(Value); UE_LOG(LogTemp, Warning, TEXT("AddQReward: %f"), Value); }
	float GetReward();
	
	/* Storage */
	bool IsUsingSharedTable() const { return QFilename.Contains("Shared", ESearchCase::IgnoreCase); }

	
	/* Get */
	TMap<FQState, TMap<EQAction, float>> GetQTable() { return QTable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Die() override;
	
	/* Q Learning Parameters */
	UPROPERTY(EditAnywhere) float QLearningRate = 0.1f;
	UPROPERTY(EditAnywhere) float QExplorationRate = 0.25f;
	UPROPERTY(EditAnywhere) float QDiscountFactor = 0.95f;
	UPROPERTY(EditAnywhere, Category=QLearning) float QUpdateIntervalSecs = 1.f;

	/* Q State Parameters */
	UPROPERTY(EditAnywhere, Category=QLearning) float QAttackRadius = 300.f;
	UPROPERTY(EditAnywhere, Category=QLearning) float QCombatRadius = 1000.f;

	/* Target */
	UPROPERTY(VisibleAnywhere, Category=QLearning) class AKnightCharacter* QTarget; // Possibly change to ABaseCharacter - to accept other enemy types 
	void FindQTarget();

	/* Automated Movement */
	void ChaseQTarget();
	bool CanChaseQTarget();

	
	/* Hit Recently */
	FTimerHandle HitRecentlyTimer;
	UPROPERTY(EditAnywhere, Category=QLearning) float HitRecentlyDurationSecs = 2.f;

	/* Actions + Enemy Overrides */
	virtual void SetToAttacking() override;
	virtual void SetToDodging() override;
	virtual void SetToGuarding() override;
	virtual void SetToHealing() override;
	void SetToWaiting();
	FTimerHandle WaitTimer;

	virtual void OnAttackEnd() override;
	virtual void OnDodgeEnd() override;
	virtual void ClearGuardTimer() override;


	/* Storage */
	UPROPERTY(EditAnywhere, Category=QLearning) FString QFilename = FString::Printf(TEXT("%s_QTable.json"), *GetName());
	void SaveQTableToDisk();
	void LoadQTableFromDisk();

	/* Merge Storage */ // Multiple QEnemy storage
	UPROPERTY() AQLearningManager* QManager;
	void FindQManager();
	void SubmitQTableToManager() const;

	/* Update Phase 2 Recovery */
	FTimerHandle FallbackPhase2Timer;
	void FallbackPhase2();
	void ClearFallbackPhase2Timer() { GetWorldTimerManager().ClearTimer(FallbackPhase2Timer); }

	/* Timer Cleanup */
	virtual void ClearAllTimers();
	
	/* Logging Functions */
	//FTimerHandle DisplayInfoTimer;
	void DisplayQTable() const;
	void DisplayQState() const;
	void DisplayQReward() { UE_LOG(LogTemp, Warning, TEXT("Pending Reward: %f"), GetReward()); }

};





/* Encoding */
//FString EncodeState() const;