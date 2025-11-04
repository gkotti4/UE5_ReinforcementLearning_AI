#pragma once

#include "CoreMinimal.h"
#include "QLearning/Enemy/QLearningEnemy.h"
#include "QLearning/Networking/NetComm.h"
#include "DQNEnemy.generated.h"


/* Forward Declarations */


UCLASS()
class UDEMYACTIONRPG_API ADQNEnemy : public AQLearningEnemy
{
	GENERATED_BODY()

public:
	ADQNEnemy();
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, Category = "DQN Parameters") int32 TrainingDeaths = 5;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* Networking */
	NetComm Comm;
	FString BuildPacket(const TArray<float>& StateObs, float Reward, bool bDone);


	/* Decision-phase parameters */
	const float DqnUpdateCooldown = 0.55f; // replaces QUpdateIntervalSecs
	
	/* Core update loop */
	virtual void UpdateFunction_Phase1() override; 
	virtual void UpdateFunction_Phase2() override;
	
	
	/* Training Done */
	bool IsDone() const { return EnemyState == EEnemyState::EES_Dead; }
	void EndTraining();

	/* Enemy Overrides */
	virtual void Die() override;
	void CheckRepeatActions();
};




/* Decision-phase state */
//enum class EAIStage : uint8 { Idle, Acting };
//EAIStage	Stage		= EAIStage::Idle;
//uint32	PendingActionID = UINT32_MAX; 	/* Latest action received from DQN server */
