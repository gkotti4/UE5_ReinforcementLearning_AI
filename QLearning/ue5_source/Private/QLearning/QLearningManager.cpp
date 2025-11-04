#include "QLearning/QLearningManager.h"
#include "QLearning/Enemy/QLearningEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"


AQLearningManager::AQLearningManager()
{
	PrimaryActorTick.bCanEverTick = true;

}

void AQLearningManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	/*if (!MergedQTable.IsEmpty())
	{
		SaveMergedQTableToDisk(SharedFilename);
	}*/
	
}

void AQLearningManager::BeginDestroy()
{
	Super::BeginDestroy();

	//MergeAndSaveQTables();
	SaveMergedQTableToDisk(SharedFilename);
}

void AQLearningManager::BeginPlay()
{
	Super::BeginPlay();

	Tags.Add(FName("QManager"));
	
}

void AQLearningManager::Tick(float DeltaTime)
{
	//Super::Tick(DeltaTime);

}

AQLearningManager* AQLearningManager::Get(UWorld* World)
{
	for (TActorIterator<AQLearningManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

TArray<AQLearningEnemy*> AQLearningManager::FindAllQEnemies()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQLearningEnemy::StaticClass(), FoundActors);

	TArray<AQLearningEnemy*> Enemies;
	for (AActor* Actor : FoundActors)
	{
		if (AQLearningEnemy* Enemy = Cast<AQLearningEnemy>(Actor))
		{
			if (Enemy->IsUsingSharedTable())
				Enemies.Add(Enemy);
		}
	}
	return Enemies;
}

void AQLearningManager::MergeQTableFromEnemy(const TMap<FQState, TMap<EQAction, float>>& OtherTable)
{
	for (const auto& StatePair : OtherTable)
	{
		const FQState& State = StatePair.Key;
		const TMap<EQAction, float>& Actions = StatePair.Value;

		if (!MergedQTable.Contains(State))
		{
			MergedQTable.Add(State, Actions);
		}
		else
		{
			for (const auto& ActionPair : Actions)
			{
				EQAction Action = ActionPair.Key;
				float OtherValue = ActionPair.Value;

				float& ExistingValue = MergedQTable[State].FindOrAdd(Action);
				ExistingValue = (ExistingValue + OtherValue) / 2.0f; // Simple average
				// experiment with merging algorithms
			}
		}
	}
}

void AQLearningManager::SaveMergedQTableToDisk(const FString& Filename)
{
	const FString SavePath = FPaths::ProjectSavedDir() + Filename;

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

	for (const auto& StatePair : MergedQTable)
	{
		FString StateKey = StatePair.Key.ToString();
		TSharedPtr<FJsonObject> ActionMap = MakeShareable(new FJsonObject);

		for (const auto& ActionPair : StatePair.Value)
		{
			FString ActionKey = FString::FromInt(static_cast<int32>(ActionPair.Key));
			ActionMap->SetNumberField(ActionKey, ActionPair.Value);
		}

		Root->SetObjectField(StateKey, ActionMap);
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	FFileHelper::SaveStringToFile(Output, *SavePath);
	UE_LOG(LogTemp, Warning, TEXT("Merged QTable saved to %s"), *SavePath);
}

void AQLearningManager::MergeAndSaveQTables()
{
	MergedQTable.Empty();

	for (AQLearningEnemy* Enemy : FindAllQEnemies())
	{
		MergeQTableFromEnemy(Enemy->GetQTable()); // Add `GetQTable()` to your QEnemy
	}

	SaveMergedQTableToDisk(SharedFilename);
}


