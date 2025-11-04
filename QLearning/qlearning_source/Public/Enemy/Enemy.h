
#pragma once

#include "CoreMinimal.h"
#include "Characters/BaseCharacter.h"
#include "Enemy.generated.h"

class UWidgetComponent;
class UHealthBarComponent;
class UBoxComponent; // GuardBox
class AAIController;
class UPawnSensingComponent;


UCLASS()
class UDEMYACTIONRPG_API AEnemy : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AEnemy();
	virtual void Tick(float DeltaTime) override;
	virtual void GetHit_Implementation(const FVector& ImpactPoint) override; // From HitInterface
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Destroyed() override;
	
protected:
	UPROPERTY(EditAnywhere) UBoxComponent* GuardBoxComponent;

	/* Enemy States */
	UPROPERTY(BlueprintReadOnly) EDeathPose DeathPose;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EEnemyState EnemyState = EEnemyState::EES_Patrolling; // VisibleAnywhere
	
	virtual void BeginPlay() override;
	virtual void PlayAttackMontage() override;
	virtual void Attack() override;
	virtual bool CanAttack();
	virtual void Dodge() override;
	virtual void Die() override;
	virtual void HandleDamage(float DamageAmount) override;

	UFUNCTION() void PawnSeen(APawn* SeenPawn); // bound to delegate

	/* private: */
	UPROPERTY(EditAnywhere) TSubclassOf<AWeapon> WeaponClass;
	
	/* Components */
	UPROPERTY(VisibleAnywhere) UHealthBarComponent* HealthBarWidgetComponent;
	UPROPERTY(VisibleAnywhere) UPawnSensingComponent* PawnSensing;
	
	/* General Movement Settings */
	UPROPERTY(EditAnywhere, Category = Movement, BlueprintReadWrite) float MaxWalkSpeed = 300.f;
	UPROPERTY(EditAnywhere, Category = Movement, BlueprintReadWrite) float MaxPatrolWalkSpeed = 150.f;

	/* Navigation & Patrol */
	UPROPERTY() AAIController* EnemyController;
	UPROPERTY(EditInstanceOnly, Category = "AI Navigation", BlueprintReadWrite) AActor* PatrolTarget; // Target Point Actor // add allow private access when private object
	UPROPERTY(EditInstanceOnly, Category = "AI Navigation") TArray<AActor*>	PatrolTargets;
	UPROPERTY(EditAnywhere) double PatrolRadius = 200.f;
	UPROPERTY(EditAnywhere, Category = "AI Navigation") float WatchTimeMin = 2.f; // Wait time in patrol timer
	UPROPERTY(EditAnywhere, Category = "AI Navigation") float WatchTimeMax = 7.f;
	
	FTimerHandle PatrolTimer;
	void PatrolTimerFinished();
	AActor* ChoosePatrolTarget();
	void MoveToTarget(const AActor* Target, double AcceptanceRadius);

	
	/* AI Behavior */
	virtual void CheckPatrolTarget();
	virtual void CheckCombatTarget();
	virtual void SetToChasing();
	virtual void SetToPatrolling();
	virtual void SetToAttacking();
	virtual void SetToGuarding();
	virtual void SetToDodging();
	virtual void SetToHealing();
	virtual void LoseInterest();
	void HideHealthBar();
	void ShowHealthBar();
	
	/* Combat */
	UPROPERTY(VisibleAnywhere) AActor* CombatTarget;
	UPROPERTY(EditAnywhere) double CombatRadius = 1000.f;
	UPROPERTY(EditAnywhere) double AttackRadius = 275.f;

	/* Attack */
	FTimerHandle AttackTimer;
	UPROPERTY(EditAnywhere, Category = Combat) float AttackTimeMin = 0.1f;
	UPROPERTY(EditAnywhere, Category = Combat) float AttackTimeMax = 1.f;
	virtual void OnAttackEnd() override;
	virtual void StartAttackTimer();

	/* Guarding */
	FTimerHandle GuardTimer;
	virtual void StartGuardTimer();

	/* Dodging */
	UPROPERTY(EditAnywhere) float DodgeCooldownSecs = 1.5f;
	virtual void OnDodgeEnd() override;
	
	/* Helper Functions */
	bool IsOutsideCombatRadius() const { return !InTargetRange(CombatTarget, CombatRadius); }
	bool IsInsideCombatRadius() const { return InTargetRange(CombatTarget, CombatRadius); }
	bool IsOutsideAttackRadius() const { return !InTargetRange(CombatTarget, AttackRadius); }
	bool IsInsideAttackRadius() const { return InTargetRange(CombatTarget, AttackRadius); }
	bool IsDead() const { return EnemyState == EEnemyState::EES_Dead; }
	bool IsPatrolling() const { return EnemyState == EEnemyState::EES_Patrolling; }
	bool IsChasing() const { return EnemyState == EEnemyState::EES_Chasing; }
	bool IsAttacking() const { return EnemyState == EEnemyState::EES_Attacking; }
	bool IsGuarding() const { return EnemyState == EEnemyState::EES_Guarding; }
	bool IsDodging() const { return EnemyState == EEnemyState::EES_Dodging; }
	bool IsEngaged() const { return EnemyState == EEnemyState::EES_Engaged; }
	bool IsUnoccupied() const { return EnemyState == EEnemyState::EES_NoState; }
	virtual void ClearPatrolTimer() { GetWorldTimerManager().ClearTimer(PatrolTimer); }
	virtual void ClearAttackTimer() { GetWorldTimerManager().ClearTimer(AttackTimer); }
	virtual void ClearGuardTimer();
	
	bool InTargetRange(const AActor* Target, double Radius) const;
	FVector GetDirectionToTarget(const AActor* Target) const { return (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal(); }
	void InterpRotationToTarget(AActor* Target, float DeltaTime, float RotationSpeed);

};




/*
 * GetHit -> changed to -> GetHit_Implementation b/c we changed GetHit in HitInterface to BlueprintNativeEvent -
 *		meaning GetHit can no longer be virtual
 */
