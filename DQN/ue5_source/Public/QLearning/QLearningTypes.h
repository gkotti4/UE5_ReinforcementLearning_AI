
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
    bool bIsTargetDodging;      // New field
    bool bWasHitRecently;

    // --- Hashing support ---
    friend uint32 GetTypeHash(const FQState& Key)
    {
        uint32 Hash = 0;
        Hash = HashCombine(Hash, GetTypeHash(Key.HealthPercent));
        Hash = HashCombine(Hash, GetTypeHash(Key.TargetHealthPercent));
        Hash = HashCombine(Hash, GetTypeHash(Key.HealsLeft));
        Hash = HashCombine(Hash, GetTypeHash(Key.bIsInAttackRange));
        Hash = HashCombine(Hash, GetTypeHash(Key.bIsTargetAttacking));
        Hash = HashCombine(Hash, GetTypeHash(Key.bIsTargetGuarding));
        Hash = HashCombine(Hash, GetTypeHash(Key.bIsTargetDodging));
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
               bIsTargetDodging == Other.bIsTargetDodging &&
               bWasHitRecently == Other.bWasHitRecently;
    }

    // --- Debug ToString() ---
    FString ToString() const
    {
        return FString::Printf(TEXT("%d_%d_%d_%d_%d_%d_%d_%d"),
            HealthPercent,
            TargetHealthPercent,
            HealsLeft,
            bIsInAttackRange,
            bIsTargetAttacking,
            bIsTargetGuarding,
            bIsTargetDodging,
            bWasHitRecently);
    }

    static FQState FromString(const FString& Str)
    {
        FQState State;
        TArray<FString> Parts;
        Str.ParseIntoArray(Parts, TEXT("_"), true);

        if (Parts.Num() >= 8)
        {
            State.HealthPercent = FCString::Atoi(*Parts[0]);
            State.TargetHealthPercent = FCString::Atoi(*Parts[1]);
            State.HealsLeft = FCString::Atoi(*Parts[2]);
            State.bIsInAttackRange = FCString::Atoi(*Parts[3]) != 0;
            State.bIsTargetAttacking = FCString::Atoi(*Parts[4]) != 0;
            State.bIsTargetGuarding = FCString::Atoi(*Parts[5]) != 0;
            State.bIsTargetDodging = FCString::Atoi(*Parts[6]) != 0;
            State.bWasHitRecently = FCString::Atoi(*Parts[7]) != 0;
        }
        return State;
    }

	/** Convert each field into a float for DQN input. */
	TArray<float> ToFloatArray() const
    {
    	TArray<float> V;
    	V.Reserve(8);
    	V.Add(static_cast<float>(HealthPercent));
    	V.Add(static_cast<float>(TargetHealthPercent));
    	V.Add(static_cast<float>(HealsLeft));
    	V.Add(bIsInAttackRange		? 1.0f : 0.0f);
    	V.Add(bIsTargetAttacking	? 1.0f : 0.0f);
    	V.Add(bIsTargetGuarding		? 1.0f : 0.0f);
    	V.Add(bIsTargetDodging		? 1.0f : 0.0f);
    	V.Add(bWasHitRecently		? 1.0f : 0.0f);
    	return V;
    }
	
    FQState()
    : HealthPercent(0)
    , TargetHealthPercent(0)
    , HealsLeft(0)
    , bIsInAttackRange(false)
    , bIsTargetAttacking(false)
    , bIsTargetGuarding(false)
    , bIsTargetDodging(false)
    , bWasHitRecently(false)
    {}

    void Reset()
    {
        *this = FQState(); 
    }
};



struct QLearningRewards
{
	static constexpr float KillReward = 8.0f; // in use
	static constexpr float GotHit = -0.9f; // in use

	static constexpr float AttackHit = 1.5f; // in use
	static constexpr float AttackMiss = -0.75f; // in use
	static constexpr float AttackBlocked = -0.25f; // in use
	static constexpr float AttackInAttackRange = 0.5f; // in use
	static constexpr float AttackOutsideAttackRange = -0.6f;

	static constexpr float TargetAttackMiss = 0.5f; // in use
	
	static constexpr float DodgeTargetAttack = 1.15f; // in use
	static constexpr float Dodge = 0.01f; // in use // depreciated

	static constexpr float GuardOutsideAttackRange = -0.1f; // in use
	static constexpr float GuardOutsideCombatRange = -1.5f; // in use
	static constexpr float BlockTargetAttack = 0.75f; // in use 

	//static constexpr float HealReward = 0.01f;  // depreciated
	static constexpr float HealedAtLowHealth = 1.f; // in use
	static constexpr float HealedAtHighHealth = -0.5f; // in use
	static constexpr float DeathPenalty = -7.5f; // in use
	static constexpr float WaitPenalty = -0.1f; // in use
	
	/* Experimental */
	static constexpr float HealAttemptWithNoPotionsLeft = -2.f; // in use - shaping DQN not to heal past 3 times
	static constexpr float RepeatedActionPenalty = -0.3f;
};


