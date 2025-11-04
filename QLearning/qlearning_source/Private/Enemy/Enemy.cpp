#include "Enemy/Enemy.h"
#include "UdemyActionRPG/DebugMacros.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HUD/HealthBarComponent.h"
#include "Components/AttributeComponent.h"
#include "Components/BoxComponent.h"
#include "AIController.h"
#include "Characters/SlashCharacter.h"
#include "Items/Weapons/Weapon.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/PawnSensingComponent.h"
//#include "Components/WidgetComponent.h"


AEnemy::AEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	GetMesh()->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic); // BP_Weapon is set to ignore pawns, so we want this to be world dynamic so it is hittable
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block); // BP_Weapon (and other overlapping features) we have set to use the visibility channel, so we want to make sure the enemy blocks it to allow for overlap events
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore); // will not make character camera zoom in when blocking, etc (ep.133)
	GetMesh()->SetGenerateOverlapEvents(true); // must be set to allow/trigger in overlap events
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore); // ^

	HealthBarWidgetComponent = CreateDefaultSubobject<UHealthBarComponent>(TEXT("HealthBar"));
	HealthBarWidgetComponent->SetupAttachment(GetRootComponent());

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = MaxPatrolWalkSpeed;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));
	PawnSensing->SightRadius = 5000.f; //default
	PawnSensing->SetPeripheralVisionAngle(70.f);

	GuardBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("GuardBox"));
	GuardBoxComponent->SetupAttachment(GetMesh(), FName("LeftForeArmSocket"));
	GuardBoxComponent->ComponentTags.Add(FName("GuardBox"));
	GuardBoxComponent->SetBoxExtent(FVector(20.f, 50.f, 50.f)); // Adjust size
	GuardBoxComponent->SetRelativeRotation(FRotator(0.f, 90.f, 0.f)); // Rotate to shield-like position Pro Tip: SetRelativeRotation(FRotator::MakeFromEuler(FVector(0.f, 0.f, 90.f))); is the same
	GuardBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	GuardBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GuardBoxComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	GuardBoxComponent->SetGenerateOverlapEvents(true);
	GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Start disabled
	
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp,Warning, TEXT("Enemy: BeingPlay Start"));

	Tags.Add(FName("Enemy"));

	ShowHealthBar();

	EnemyController = Cast<AAIController>(GetController());
	
	MoveToTarget(PatrolTarget, 20.f);

	if (PawnSensing)
	{
		PawnSensing->OnSeePawn.AddDynamic(this, &AEnemy::PawnSeen);
	}

	if (UWorld* World = GetWorld(); World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		DefaultWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		EquippedWeapon = DefaultWeapon;
	}

	UE_LOG(LogTemp,Warning, TEXT("Enemy: BeingPlay End"));
}

void AEnemy::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (IsDead()) return;

	if (true) return; // DEBUGGING
	if (EnemyState > EEnemyState::EES_Patrolling)
		CheckCombatTarget();
	else
		CheckPatrolTarget();

}

void AEnemy::PlayAttackMontage()
{
	//PlayRandomMontageSection(AttackMontage, 0, 3);
	PlayMontageSection(AttackMontage, FName("Attack2"));
	/*
	 * Attack Montage Sections
	 * 1 & 2 - In-Place Single Attacks
	 * 3 & 4 - In-Place Combo Attacks
	 * 5 & 6 - RootMotion Single Attacks
	*/
}

void AEnemy::Attack()
{
	PlayAttackMontage();
}

void AEnemy::Dodge() // TODO: Testing Phase
{
	int32 Direction = FMath::RandRange(0,2);
	FVector DodgeDirection = GetVelocity().GetSafeNormal();
	switch (Direction)
	{
	case 0: if (CombatTarget) DodgeDirection = -GetDirectionToTarget(CombatTarget); // Backwards
		break;
	case 1: if (CombatTarget) DodgeDirection = FVector::CrossProduct(GetDirectionToTarget(CombatTarget), FVector::UpVector); // 90° left
		break;
	case 2: if (CombatTarget) DodgeDirection = FVector::CrossProduct(FVector::UpVector, GetDirectionToTarget(CombatTarget)); // 90° right
		break;
	default: ;
	}
	
	
	FRotator LookRotation = FRotationMatrix::MakeFromX(DodgeDirection).Rotator();
	SetActorRotation(FRotator(0.f, LookRotation.Yaw, 0.f));
	
	PlayDodgeMontage();
}

void AEnemy::SetToPatrolling()
{
	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = MaxPatrolWalkSpeed;
	MoveToTarget(PatrolTarget, 20.f);
}

void AEnemy::SetToChasing()
{
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxWalkSpeed = MaxWalkSpeed;
	MoveToTarget(CombatTarget, AttackRadius/2); // Change: 15.f to 90.f, Issue: Enemy attacking too close
}

void AEnemy::SetToAttacking()
{
	EnemyState = EEnemyState::EES_Attacking;
	StartAttackTimer();
}

void AEnemy::SetToGuarding() // TODO: In Testing Phase 
{
	EnemyState = EEnemyState::EES_Guarding; // test
	StartGuardTimer();
}

void AEnemy::SetToDodging() // TODO: In Testing Phase 
{
	EnemyState = EEnemyState::EES_Dodging; // test
	Dodge();
}

void AEnemy::SetToHealing()
{
	// No animation - No healing state or state change atm
	Attributes->Heal();
}

void AEnemy::LoseInterest()
{
	CombatTarget = nullptr;
	SetToPatrolling();
}



void AEnemy::CheckCombatTarget()
{
	if (!CombatTarget) return; // note: could take out to save some performance
	if (IsOutsideCombatRadius()) // Outside Combat Radius, patrol
	{
		ClearGuardTimer();
		ClearAttackTimer();
		LoseInterest();
		if (!IsEngaged()) SetToPatrolling();
	}
	else if (IsOutsideAttackRadius() && !IsChasing()) // Outside Attack Range, chase player
	{
		ClearGuardTimer();
		ClearAttackTimer();
		if (!IsEngaged()) SetToChasing();
	}
	else if (CanAttack()) // Inside Attack Range, attack player
	{
		SetToAttacking();
	}
	
	if (IsAttacking() || IsGuarding()) InterpRotationToTarget(CombatTarget, DeltaSeconds, 7.5f); // IN DEVELOPMENT
	
}

void AEnemy::CheckPatrolTarget()
{
	if (InTargetRange(PatrolTarget, PatrolRadius)) // Patrol
	{
		const float WatchTime = FMath::RandRange(WatchTimeMin, WatchTimeMax);
		PatrolTarget = ChoosePatrolTarget();
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::PatrolTimerFinished, WatchTime);
	}
}

inline bool AEnemy::CanAttack()
{
	return IsInsideAttackRadius() && !IsAttacking() && !IsEngaged() && !IsDead();
}

void AEnemy::Die()
{
	PlayDeathMontage();
	EnemyState = EEnemyState::EES_Dead;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HideHealthBar();

	SetLifeSpan(1.5f);
}


void AEnemy::GetHit_Implementation(const FVector& ImpactPoint)
{
	if (IsAlive())
	{
		DirectionalHitReact(ImpactPoint);
	}
	else
	{
		Die();
	}

	ShowHealthBar();
	PlayHitSound(ImpactPoint);
	SpawnHitParticles(ImpactPoint);
	
}


float AEnemy::TakeDamage(const float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator,
                         AActor* DamageCauser)
{
	HandleDamage(DamageAmount);
	CombatTarget = EventInstigator->GetPawn();
	SetToChasing();
	return DamageAmount;
}

void AEnemy::Destroyed()
{
	Super::Destroyed(); // don't think this is needed
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
		// TODO: weapon dropping code here
	}
}


void AEnemy::PatrolTimerFinished()
{
	MoveToTarget(PatrolTarget, 20.f);
}

AActor* AEnemy::ChoosePatrolTarget()
{
	TArray<AActor*> ValidTargets;
	for (auto Target : PatrolTargets)
	{
		if (Target != PatrolTarget)
		{
			ValidTargets.AddUnique(Target);
		}
	}
	if (const int32 NumPatrolTargets = ValidTargets.Num(); NumPatrolTargets > 0)
	{
		const int32 NextTargetSelection = FMath::RandRange(0, NumPatrolTargets - 1);
		return ValidTargets[NextTargetSelection];
	}
	return nullptr;
}

void AEnemy::MoveToTarget(const AActor* Target, const double AcceptanceRadius)
{
	if (!IsValid(Target) || !EnemyController) return;
	
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	FNavPathSharedPtr NavPath;
	
	EnemyController->MoveTo(MoveRequest, &NavPath);
	
}



bool AEnemy::InTargetRange(const AActor* Target, const double Radius) const
{
	if (!Target) return false;
	const double DistanceToTarget = (Target->GetActorLocation() - GetActorLocation()).Size();
	return DistanceToTarget <= Radius;
}

void AEnemy::PawnSeen(APawn* SeenPawn)
{
	const bool bShouldChaseTarget =
		EnemyState != EEnemyState::EES_Dead &&
			EnemyState != EEnemyState::EES_Chasing &&
						EnemyState < EEnemyState::EES_Attacking &&
							SeenPawn->ActorHasTag(FName("Player"));
	
	if (bShouldChaseTarget)
	{
		CombatTarget = SeenPawn;
		ClearPatrolTimer(); // stopped until set again
		SetToChasing();
	}
}

void AEnemy::HandleDamage(const float DamageAmount)
{
	Super::HandleDamage(DamageAmount);
	if (Attributes && HealthBarWidgetComponent) HealthBarWidgetComponent->SetHealthPercent(Attributes->GetHealthPercent());
}


inline void AEnemy::HideHealthBar()
{
	if (HealthBarWidgetComponent) HealthBarWidgetComponent->SetVisibility(false);
}

inline void AEnemy::ShowHealthBar()
{
	if (HealthBarWidgetComponent) HealthBarWidgetComponent->SetVisibility(true);
}

void AEnemy::OnAttackEnd()
{
	EnemyState = EEnemyState::EES_NoState;
	CheckCombatTarget();
}
void AEnemy::OnDodgeEnd()
{
	EnemyState = EEnemyState::EES_NoState;
	CheckCombatTarget();
}


void AEnemy::StartAttackTimer() 
{
	//EnemyState = EEnemyState::EES_Attacking; // TODO: remove after testing (already in SetToAttacking)
	const float AttackTime = FMath::RandRange(AttackTimeMin, AttackTimeMax);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
}

void AEnemy::StartGuardTimer()
{
	constexpr float GuardTimeMin = 1.f, GuardTimeMax = 3.f;
	const float GuardTime = FMath::RandRange(GuardTimeMin, GuardTimeMax);
	PlayGuardMontage();
	GetWorldTimerManager().SetTimer(GuardTimer, this, &AEnemy::ClearGuardTimer, GuardTime);
	
	if (GuardBoxComponent) GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Usually Done in ABP i.e. for Weapon Collision
}

void AEnemy::ClearGuardTimer()
{
	if (GuardBoxComponent) GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Usually Done in ABP i.e. for Weapon Collision
	GetWorldTimerManager().ClearTimer(GuardTimer);
	StopMontage(GuardMontage);
	EnemyState = EEnemyState::EES_NoState;
	CheckCombatTarget();
}

void AEnemy::InterpRotationToTarget(AActor* Target, float DeltaTime, const float RotationSpeed)
{
	if (!Target) return;
	
	const FVector Direction = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const FRotator TargetRotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	const FRotator CurrentRotation = GetActorRotation();

	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
	SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f)); // lock pitch/roll
}
