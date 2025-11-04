#pragma once

#include "CoreMinimal.h"
#include "CharacterTypes.h"
#include "Interfaces/HitInterface.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

#include "BaseCharacter.generated.h"


class UAnimMontage;
class AWeapon;
class UAttributeComponent;



UCLASS()
class UDEMYACTIONRPG_API ABaseCharacter : public ACharacter, public IHitInterface
{
	GENERATED_BODY()

public:
	ABaseCharacter();
	virtual void Tick(float DeltaTime) override;
	
	UFUNCTION(BlueprintCallable) void SetWeaponCollisionEnabled(ECollisionEnabled::Type CollisionEnabled);
	
protected:
	float DeltaSeconds = 0.f;
	
	/* Components */
	UPROPERTY(VisibleAnywhere) UAttributeComponent* Attributes;
	
	/* Animation Montages */
	/* Set in editor */
	UPROPERTY(EditDefaultsOnly, Category = Montage) UAnimMontage* AttackMontage;
	UPROPERTY(EditDefaultsOnly, Category = Montage) UAnimMontage* DeathMontage;
	UPROPERTY(EditDefaultsOnly, Category = Montage) UAnimMontage* HitReactMontage;
	UPROPERTY(EditDefaultsOnly, Category = Montage) UAnimMontage* DodgeMontage;
	UPROPERTY(EditDefaultsOnly, Category = Montage) UAnimMontage* GuardMontage;
	
	/* Weapon Components */
	UPROPERTY(VisibleAnywhere, Category = Combat) AWeapon* EquippedWeapon; // Make array later? Hold multiple weapons

	/* Hit Components */
	UPROPERTY(EditAnywhere, Category = Combat) USoundBase* HitSound;
	UPROPERTY(EditAnywhere, Category = Combat) UParticleSystem* HitParticles;

	
	virtual void BeginPlay() override;
	virtual void AttackInput(const FInputActionValue& Value);
	virtual void Attack();
	virtual void Dodge();
	virtual void DodgeInput(const FInputActionValue& Value);
	virtual void Heal(const int32 HealAmount);
	virtual void Die();
	
	/* Notifies */
	UFUNCTION(BlueprintCallable) virtual void OnAttackEnd();
	UFUNCTION(BlueprintCallable) virtual void OnDodgeEnd();
	
	/* Montages */
	virtual void PlayAttackMontage();
	virtual void PlayDeathMontage();
	virtual void PlayDodgeMontage();
	virtual void PlayGuardMontage();
	void PlayMontageSection(UAnimMontage* Montage, const FName& SectionName) const;
	void PlayMontageSection(UAnimMontage* Montage, const int32 Index) const;
	void PlayRandomMontageSection(UAnimMontage* Montage) const;
	void PlayRandomMontageSection(UAnimMontage* Montage, const int32 MinIndex, const int32 MaxIndex) const;
	void StopMontage(UAnimMontage* Montage) const;
	void PlayHitReactMontage(const FName& SectionName) const;

	/* Hit */
	void DirectionalHitReact(const FVector& ImpactPoint);
	void PlayHitSound(const FVector& ImpactPoint) const;
	void SpawnHitParticles(const FVector& ImpactPoint) const;
	virtual void HandleDamage(float DamageAmount);
	
	bool IsAlive() const;
	
};











/* Functions in Tutorial left out
 * CanAttack()
 */

