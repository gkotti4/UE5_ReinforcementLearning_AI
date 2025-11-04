// Fill out your copyright notice in the Description page of Project Settings.


#include "Items/Weapons/Weapon.h"
#include "Characters/SlashCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interfaces/HitInterface.h"
#include "NiagaraComponent.h"

/* Q Learning */
#include "Characters/KnightCharacter.h"
#include "QLearning/Enemy/QLearningEnemy.h"
#include "QLearning/QLearningTypes.h"


AWeapon::AWeapon()
{
	WeaponBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponBox"));
	WeaponBox->SetupAttachment(GetRootComponent());
	WeaponBox->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Query by default
	WeaponBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	WeaponBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);

	BoxTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("BoxTraceStart"));
	BoxTraceStart->SetupAttachment(GetRootComponent());

	BoxTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("BoxTraceEnd"));
	BoxTraceEnd->SetupAttachment(GetRootComponent());

}

void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	WeaponBox->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnBoxOverlap);

}

void AWeapon::PlayEquipSound()
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			EquipSound,
			GetActorLocation()
		);
	}
}

void AWeapon::DisableSphereCollision()
{
	if (Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AWeapon::DeactivateEmbers()
{
	if (EmbersEffect)
	{
		EmbersEffect->Deactivate();
	}
}

void AWeapon::Equip(USceneComponent* InParent, FName InSocketName, AActor* NewOwner, APawn* NewInstigator)
{
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);
	
	AttachMeshToSocket(InParent, InSocketName);
	ItemState = EItemState::EIS_Equipped;
	
	DisableSphereCollision();
	PlayEquipSound();
	DeactivateEmbers();
	
}

void AWeapon::AttachMeshToSocket(USceneComponent* InParent, const FName& InSocketName) const
{
	const FAttachmentTransformRules TransformRules(EAttachmentRule::SnapToTarget, true);
	ItemMesh->AttachToComponent(InParent, TransformRules, InSocketName);
}



void AWeapon::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) // Hit w Equipped Weapon
{
	if (IsActorSameTypeAs(OtherActor)) return;
	
	FHitResult BoxHit;
	BoxTrace(BoxHit);
	
	if (BoxHit.GetActor() && !bHitBlocked) // Successful Hit
	{
		if (IsActorSameTypeAs(BoxHit.GetActor())) return;

		UGameplayStatics::ApplyDamage(BoxHit.GetActor(),Damage,GetInstigator()->GetController(),this,UDamageType::StaticClass()// Create custom damage type later OR Implement custom damage system (we could still use things like EventInsigator since this is already set)
			);
		ExecuteGetHit(BoxHit);
		CreateFields(BoxHit.ImpactPoint);
	}
	else if (BoxHit.GetActor() && bHitBlocked) // Hit Blocked
	{
		/* Attack Blocked Damage Code Here */
		
		/* Q Learning */
		if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(GetOwner()))
		{
			QEnemy->AddQReward(QLearningRewards::AttackBlocked);
		}
		if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(BoxHit.GetActor()))
		{
			QEnemy->AddQReward(QLearningRewards::BlockTargetAttack);
		}
	}
	else // Hit Missed
	{
		/* Q Learning */
		if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(GetOwner()))
		{
			QEnemy->AddQReward(QLearningRewards::AttackMiss);
		}
		if (AKnightCharacter* Knight = Cast<AKnightCharacter>(GetOwner()))
		{
			Knight->AddQRewardToEnemies(QLearningRewards::TargetAttackMiss);
		}
	}
	
	// DEBUGGING
	//if (OtherActor) UE_LOG(LogTemp, Warning, TEXT("OnBoxOverlap -> OtherActor: %s"), *OtherActor->GetName());
	//if (OtherComp) UE_LOG(LogTemp, Warning, TEXT("OnBoxOverlap -> OtherComponent: %s"), *OtherComp->GetName());
	//UE_LOG(LogTemp, Warning, TEXT("OnBoxOverlap -> bHitBlocked: %d"), bHitBlocked);

}

void AWeapon::BoxTrace(FHitResult& BoxHit)
{
	const FVector Start = BoxTraceStart->GetComponentLocation(); // world space
	const FVector End = BoxTraceEnd->GetComponentLocation();

	bHitBlocked = false;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	ActorsToIgnore.Add(GetOwner()); // Added this (KnightCharacter was being hit by own weapon, Keep in mind when finishing)

	for (AActor* Actor : IgnoreActors) {
		ActorsToIgnore.AddUnique(Actor);
	}
	
	// box trace
	UKismetSystemLibrary::BoxTraceSingle( // use multi?
		this, 
		Start, 
		End, 
		BoxTraceExtent, 
		BoxTraceStart->GetComponentRotation(),
		ETraceTypeQuery::TraceTypeQuery1, // visibility?
		true, // expensive // better for Mesh (hits exact triangle) // CHECK
		ActorsToIgnore,
		bShowBoxDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		BoxHit, // out parameter
		true
		);



	if (BoxHit.GetActor())
	{
		if (UPrimitiveComponent* HitComp = BoxHit.GetComponent(); HitComp && HitComp->GetName().Contains("GuardBox")) // use tags
		{
			bHitBlocked = true;
			UE_LOG(LogTemp, Warning, TEXT("BoxTrace -> Hit Blocked at %s"), *BoxHit.GetActor()->GetName());
		}
		
		IgnoreActors.AddUnique(BoxHit.GetActor());
		for (AActor* Actor : IgnoreActors)
		{
			//UE_LOG(LogTemp, Warning, TEXT("BoxTrace END -> IgnoreActors: %s"), *Actor->GetName());
		}
		
	}

	// DEBUGGING
	//if (BoxHit.GetActor()) UE_LOG(LogTemp, Warning, TEXT("BoxTrace -> HitActor: %s"), *BoxHit.GetActor()->GetName());
	//if (HitComp) UE_LOG(LogTemp, Warning, TEXT("BoxTrace -> HitComponent: %s"), *HitComp->GetName());

}



void AWeapon::ExecuteGetHit(FHitResult BoxHit)
{
	if (const IHitInterface* HitInterface = Cast<IHitInterface>(BoxHit.GetActor())) 
	{
		//HitInterface->GetHit(BoxHit.ImpactPoint); // non-BPNativeEvent way
		HitInterface->Execute_GetHit(BoxHit.GetActor(), BoxHit.ImpactPoint); // BPNativeEvent format
	}
}





bool AWeapon::IsActorSameTypeAs(const AActor* OtherActor) const
{
	return GetOwner()->ActorHasTag(TEXT("Enemy")) && OtherActor->ActorHasTag(TEXT("Enemy")); // double check this
}