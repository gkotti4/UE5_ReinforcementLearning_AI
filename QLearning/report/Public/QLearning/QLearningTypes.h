
#pragma once

#include "CoreMinimal.h"

#define ADD_HASH(Hash, Field) Hash = HashCombine(Hash, GetTypeHash(Field)); // not used atm

UENUM(BlueprintType)
enum class EQAction : uint8
{
	Attack,
	Guard,
	Dodge,
	Heal,
	Wait
	// RunSpeed?
};

struct FQState
{
	int8 HealthPercent;
	int8 TargetHealthPercent;
	int8 HealsLeft;
	bool bIsInAttackRange;
	bool bIsTargetAttacking;
	bool bIsTargetGuarding;
	bool bWasHitRecently;
	//float DistanceToTarget;
	//bool bIsInCombatRange;
	

	// --- Hashing support ---
	friend uint32 GetTypeHash(const FQState& Key) // used in TMap or TSet
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(Key.HealthPercent));
		Hash = HashCombine(Hash, GetTypeHash(Key.TargetHealthPercent));
		Hash = HashCombine(Hash, GetTypeHash(Key.HealsLeft));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIsInAttackRange));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIsTargetAttacking));
		Hash = HashCombine(Hash, GetTypeHash(Key.bIsTargetGuarding));
		Hash = HashCombine(Hash, GetTypeHash(Key.bWasHitRecently));
		return Hash;
	}

	// --- Equality operator for use as a key in TMap ---
	bool operator==(const FQState& Other) const
	{
		return HealthPercent == Other.HealthPercent &&
			   TargetHealthPercent == Other.TargetHealthPercent &&
			   HealsLeft == Other.HealsLeft &&
			   bIsInAttackRange == Other.bIsInAttackRange &&
			   bIsTargetAttacking == Other.bIsTargetAttacking &&
			   bIsTargetGuarding == Other.bIsTargetGuarding &&
			   bWasHitRecently == Other.bWasHitRecently;
	}

	// --- Debug ToString() ---
	FString ToString() const
	{
		return FString::Printf(TEXT("%d_%d_%d_%d_%d_%d_%d"), // %d for compatibility
			HealthPercent,
			TargetHealthPercent,
			HealsLeft,
			bIsInAttackRange,
			bIsTargetAttacking,
			bIsTargetGuarding,
			bWasHitRecently);
	}

	static FQState FromString(const FString& Str)
	{
		FQState State;
		TArray<FString> Parts;
		Str.ParseIntoArray(Parts, TEXT("_"), true);

		if (Parts.Num() >= 7)
		{
			State.HealthPercent = FCString::Atoi(*Parts[0]);
			State.TargetHealthPercent = FCString::Atoi(*Parts[1]);
			State.HealsLeft = FCString::Atoi(*Parts[2]);
			State.bIsInAttackRange = FCString::Atoi(*Parts[3]) != 0;
			State.bIsTargetAttacking = FCString::Atoi(*Parts[4]) != 0;
			State.bIsTargetGuarding = FCString::Atoi(*Parts[5]) != 0;
			State.bWasHitRecently = FCString::Atoi(*Parts[6]) != 0;
		}
		return State;
	}

	FQState()
	: HealthPercent(0)
	, TargetHealthPercent(0)
	, HealsLeft(0)
	, bIsInAttackRange(false)
	, bIsTargetAttacking(false)
	, bIsTargetGuarding(false)
	, bWasHitRecently(false)
	{}

	void Reset()
	{
		*this = FQState(); // Calls the default constructor to reset all values
	}
	
};




struct QLearningRewards
{
	static constexpr float KillReward = 10.0f; // in use
	static constexpr float GotHit = -0.9f; // in use

	static constexpr float AttackHit = 1.f; // in use
	static constexpr float AttackMiss = -0.9f; // in use
	static constexpr float AttackBlocked = -0.25f; // in use
	static constexpr float AttackInAttackRange = 1.f; // in use
	static constexpr float AttackOutsideAttackRange = -1.1f;

	static constexpr float TargetAttackMiss = 0.5f; // in use
	
	static constexpr float DodgeTargetAttack = 1.1f;
	static constexpr float Dodge = 0.2f; // in use
	
	static constexpr float BlockTargetAttack = 1.f;

	static constexpr float HealReward = 0.25f; // in use
	static constexpr float DeathPenalty = -10.0f; // in use
	static constexpr float WaitPenalty = -0.5f; // in use
	
	
};