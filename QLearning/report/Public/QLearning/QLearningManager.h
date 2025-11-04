#pragma once


#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QLearningTypes.h"
#include "QLearningManager.generated.h"


class AQLearningEnemy;

UCLASS()
class UDEMYACTIONRPG_API AQLearningManager : public AActor
{
	GENERATED_BODY()
	
public:
	static AQLearningManager* Get(UWorld* World);
	AQLearningManager();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void Tick(float DeltaTime) override;
	
	void MergeAndSaveQTables();
	void MergeQTableFromEnemy(const TMap<FQState, TMap<EQAction, float>>& OtherTable);
	void SaveMergedQTableToDisk(const FString& Filename);
	
protected:
	virtual void BeginPlay() override;
	
	TArray<AQLearningEnemy*> FindAllQEnemies();
	TMap<FQState, TMap<EQAction, float>> MergedQTable;
	UPROPERTY(EditAnywhere) FString SharedFilename = "SharedQTable.json";
	
	
};
