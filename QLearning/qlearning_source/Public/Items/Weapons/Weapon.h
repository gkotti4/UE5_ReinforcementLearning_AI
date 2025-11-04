// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/Item.h"
#include "Weapon.generated.h"

class UBoxComponent;
class USoundBase;

UCLASS()
class UDEMYACTIONRPG_API AWeapon : public AItem
{
	GENERATED_BODY()
	
public:
	AWeapon();

	void Equip(USceneComponent* InParent, FName InSocketName, AActor* NewOwner, APawn* NewInstigator);
	void AttachMeshToSocket(USceneComponent* InParent, const FName& InSocketName) const;
	bool IsActorSameTypeAs(const AActor* OtherActor) const;
	void ExecuteGetHit(FHitResult BoxHit);

	UPROPERTY() TArray<AActor*> IgnoreActors;


protected:
	virtual void BeginPlay() override;
	void PlayEquipSound();
	void DisableSphereCollision();
	void DeactivateEmbers();

	UFUNCTION()
	virtual void OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult); // personal: made virtual for future weapon classes (ep. 129)

	UFUNCTION(BlueprintImplementableEvent)
	void CreateFields(const FVector& FieldLocation);
	
	
private:
	UPROPERTY(EditAnywhere, Category = "Weapon Properties") USoundBase* EquipSound; // Set in BP
	UPROPERTY(VisibleAnywhere, Category = "Weapon Properties") UBoxComponent* WeaponBox;
	
	UPROPERTY(VisibleAnywhere) USceneComponent* BoxTraceStart;
	UPROPERTY(VisibleAnywhere) USceneComponent* BoxTraceEnd;

	UPROPERTY(EditAnywhere, Category = "Weapon Properties") float Damage = 20.f;
	UPROPERTY(EditAnywhere, Category = "Weapon Properties") FVector BoxTraceExtent = FVector(5.f);
	UPROPERTY(EditAnywhere, Category = "Weapon Properties") bool bShowBoxDebug = false;
	
	void BoxTrace(FHitResult& BoxHit);

	bool bHitBlocked = false;

public:
	FORCEINLINE UBoxComponent* GetWeaponBox() const { return WeaponBox; }

};
