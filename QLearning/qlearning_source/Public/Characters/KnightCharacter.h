#pragma once

// Q Learning
#include "QLearning/QLearningTypes.h"

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "GameFramework/Character.h"
#include "KnightCharacter.generated.h"


class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class AItem;
class UAnimMontage;
class UCapsuleComponent;
class UBoxComponent;

/*
*enum class E_QAction
{
	None,
	Attack,
	Guard,
	Dodge,
	Heal,
	Wait
	// RunSpeed?
};
 *
 *
*struct F_QState
{
	int Health;
	int EnemyHealth;
	float DistanceToEnemy;
	bool bIsEnemyAttacking;
	bool bIsEnemyGuarding;
	bool bWasHitRecently;
	
};
 */

UCLASS()
class UDEMYACTIONRPG_API AKnightCharacter : public ABaseCharacter
{
	GENERATED_BODY()
	
public:
	AKnightCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetHit_Implementation(const FVector& ImpactPoint) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	/* -------------------- Input Callbacks -------------------- */
	virtual void Jump() override;

	/* -------------------- Q Learning -------------------- */
	UPROPERTY(VisibleAnywhere) TArray<class AQLearningEnemy*> QEnemies;

	ECharacterState GetPlayerActionState() const { return CharacterState; }
	
	virtual void AddQRewardToEnemies(float Reward);


protected:
	virtual void BeginPlay() override;

	/* -------------------- Components -------------------- */
	UPROPERTY(EditAnywhere) UCapsuleComponent* Capsule;
	UPROPERTY(EditAnywhere) UBoxComponent* GuardBoxComponent;

	UPROPERTY(VisibleAnywhere) USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere) UCameraComponent* ViewCamera;

	UPROPERTY(VisibleInstanceOnly) AItem* OverlappingItem;

	/* -------------------- Input Setup -------------------- */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputMappingContext* SlashMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* MovementAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* LookingAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* EKeyPressedAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* DodgeAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* AttackAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* GuardAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* GuardReleasedAction;
	UPROPERTY(EditAnywhere, Category = Input) UInputAction* Heal;

	/* -------------------- Animation Montages -------------------- */
	UPROPERTY(EditDefaultsOnly, Category = Montages) UAnimMontage* EquipMontage;

	/* -------------------- Character States -------------------- */
	UPROPERTY(BlueprintReadWrite, meta = (AllowPrivateAccess = "true")) EActionState ActionState = EActionState::EAS_Unoccupied;
	ECharacterState CharacterState = ECharacterState::ECS_Unequipped;

	/* -------------------- Animation Montage Triggers -------------------- */
	virtual void PlayAttackMontage() override;
	virtual void PlayEquipMontage(const FName& SectionName);
	virtual void PlayDodgeMontage() override;

	/* -------------------- Anim Notify Events -------------------- */
	virtual void OnAttackEnd() override;
	virtual void OnDodgeEnd() override;
	UFUNCTION(BlueprintCallable) virtual void OnDisarm();
	UFUNCTION(BlueprintCallable) virtual void OnArm();
	UFUNCTION(BlueprintCallable) virtual void OnFinishedEquipping();

	/* -------------------- Input Functions -------------------- */
	virtual void MoveInput(const FInputActionValue& Value);
	virtual void LookInput(const FInputActionValue& Value);
	virtual void EKeyPressedInput(const FInputActionValue& Value); // Equip
	virtual void DodgeInput(const FInputActionValue& Value) override;
	virtual void AttackInput(const FInputActionValue& Value) override;
	virtual void GuardInput(const FInputActionValue& Value);
	virtual void GuardReleasedInput(const FInputActionValue& Value);
	virtual void HealInput(const FInputActionValue& Value);

	/* -------------------- Combat -------------------- */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite) int32 AttackMontageSelection = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DodgeStrength = 1500.f; // Check
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DodgeSpeed = 1.5f;

	virtual void Die() override; // In Testing - uses bIsDead

	virtual bool CanAttack() const;
	virtual bool CanDodge() const;
	virtual bool CanGuard() const;
	virtual bool CanDisarm() const;
	virtual bool CanArm() const;

	/* -------------------- Combat Radius & Targeting -------------------- */
	UPROPERTY(EditAnywhere, Category=Combat) float CombatRadius = 10000.f; // TEMPORARY VALUE
	UPROPERTY(VisibleAnywhere, Category=Combat) TArray<class AEnemy*> CombatEnemies;

	virtual void CheckCombatEnemies();

	UPROPERTY() AActor* LastDamageCauser = nullptr;

	/* -------------------- Q-Learning -------------------- */
	virtual void FindQEnemies(); // Replaces CheckCombatEnemies + UpdateQEnemies

	float UpdateTimeSecs = 1.f; // needed?
	FTimerHandle QUpdateTimer; // needed?

	virtual void StartQEnemyUpdateTimer(); // In Testing
	virtual void UpdateQEnemyData();



	/* Boolean Checks */
	bool IsAttacking() const { return ActionState == EActionState::EAS_Attacking; }
	bool IsDodging() const { return ActionState == EActionState::EAS_Dodging; }
	bool IsGuarding() const { return ActionState == EActionState::EAS_Guarding; }
	bool IsUnoccupied() const { return ActionState == EActionState::EAS_Unoccupied; }

public:
	/* -------------------- Getters / Setters -------------------- */
	FORCEINLINE void SetOverlappingItem(AItem* Item) { OverlappingItem = Item; }
	FORCEINLINE ECharacterState GetCharacterState() const { return CharacterState; }
};




