#include "Characters/KnightCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
// Enhanced Input
#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Items/Item.h"
#include "Items/Weapons/Weapon.h"
#include "Components/AttributeComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

// Combat
#include "Enemy/Enemy.h"

// QLearning
#include "Kismet/GameplayStatics.h"
#include "QLearning/Enemy/QLearningEnemy.h"


AKnightCharacter::AKnightCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Capsule = GetCapsuleComponent();
	Capsule->SetCapsuleHalfHeight(88.f);
	Capsule->SetCapsuleRadius(34.f);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);

	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 300.f;
	SpringArm->bUsePawnControlRotation = true;

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SpringArm);
	

	GetMesh()->SetCollisionObjectType(ECC_WorldDynamic);
	GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

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

void AKnightCharacter::BeginPlay()
{
	Super::BeginPlay();
	Tags.Add(FName("Player"));

	// Bind Input Mapping Context
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(SlashMappingContext, 0);
		}
	}

	/* Q Learning */
	FindQEnemies(); // Can add 1-frame delay in case its 'too early' PRO TIP
	StartQEnemyUpdateTimer();
}


void AKnightCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	/* Check/Find Combat Enemies */
	// CheckCombatEnemies (expensive here all the time) use after Q Learning Project
}

void AKnightCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind Input Actions to C++ Function
		EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Triggered, this, &AKnightCharacter::MoveInput);
		EnhancedInputComponent->BindAction(LookingAction, ETriggerEvent::Triggered, this, &AKnightCharacter::LookInput);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AKnightCharacter::Jump);
		EnhancedInputComponent->BindAction(EKeyPressedAction, ETriggerEvent::Triggered, this, &AKnightCharacter::EKeyPressedInput);
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Triggered, this, &AKnightCharacter::DodgeInput);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AKnightCharacter::AttackInput);
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Triggered, this, &AKnightCharacter::GuardInput);
		EnhancedInputComponent->BindAction(GuardReleasedAction, ETriggerEvent::Triggered, this, &AKnightCharacter::GuardReleasedInput);
	}

}

void AKnightCharacter::GetHit_Implementation(const FVector& ImpactPoint)
{
	if (IsAlive())
	{
		if (!(IsAttacking() ||  IsGuarding() || IsDodging())) DirectionalHitReact(ImpactPoint); // CHECK

		PlayHitSound(ImpactPoint);
		SpawnHitParticles(ImpactPoint);
	}
	else
	{
		/* Death */
		//Die(); // in testing

		/* Q Learning */
		Attributes->SetHealthAmount(Attributes->GetMaxHealth()); // Exempt death, further trainin

		// Target Death Reward
		if (AWeapon* Weapon = Cast<AWeapon>(LastDamageCauser))
		{
			if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(Weapon->GetOwner()))
			{
				QEnemy->AddQReward(QLearningRewards::KillReward);
				//QEnemy->UpdateFunction(); // TESTING PLACEMENT - QENEMY UPDATE LOOP
			}
		}
		
	}
	/* Q Learning */
	// Successful Attack Reward
	if (AWeapon* Weapon = Cast<AWeapon>(LastDamageCauser))
	{
		if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(Weapon->GetOwner()))
		{
			QEnemy->AddQReward(QLearningRewards::AttackHit);
			//QEnemy->UpdateFunction(); // TESTING PLACEMENT - QENEMY UPDATE LOOP
		}
	}

	/* Q Learning */
	UpdateQEnemyData();
}


inline float AKnightCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	LastDamageCauser = DamageCauser;
	HandleDamage(DamageAmount);
	return DamageAmount;
}


void AKnightCharacter::Die() 
{
	// TODO 
}

void AKnightCharacter::MoveInput(const FInputActionValue& Value)
{
	if (const FVector2D MovementVector = Value.Get<FVector2D>();
		GetController() &&
		!( MovementVector.X == 0.f && MovementVector.Y == 0.f ) &&
		IsUnoccupied()) 
	{
		const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(ForwardDirection, MovementVector.Y);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(RightDirection, MovementVector.X);

	}

}

void AKnightCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddControllerPitchInput(LookAxisVector.Y);
	AddControllerYawInput(LookAxisVector.X);
}

void AKnightCharacter::EKeyPressedInput(const FInputActionValue& Value)
{
	// Figure out what type the Overlapping Item is, then Equip if necessary
	if (AWeapon* OverlappingWeapon = Cast<AWeapon>(OverlappingItem)) // TODO: && not equipped already or some functionality 
	{
		OverlappingWeapon->Equip(GetMesh(), FName("RightHandSocket"), this, this);
		
		CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon; // Will add two-handed later
		EquippedWeapon = OverlappingWeapon;
		OverlappingItem = nullptr; // Set overlapping weapon to null so we can't equip it twice
	}
	else
	{

		if (CanDisarm())
		{
			//PlayEquipMontage(FName("Unequip")); // TODO Create Equip Montage 
			CharacterState = ECharacterState::ECS_Unequipped;
			ActionState = EActionState::EAS_EquippingWeapon;
		}
		else if (CanArm()) // Equip or unequip first?
		{
			//PlayEquipMontage(FName("Equip")); // TODO Create Equip Montage 
			CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon;
			ActionState = EActionState::EAS_EquippingWeapon;
		}
	}

}

void AKnightCharacter::Jump()
{
	if (ActionState == EActionState::EAS_Attacking) return;

	Super::Jump();
}

void AKnightCharacter::DodgeInput(const FInputActionValue& Value)
{
	// TODO: Finish
	
	if (CanDodge())
	{
		FVector DodgeDirection = GetVelocity().GetSafeNormal();
		FRotator LookRotation = FRotationMatrix::MakeFromX(DodgeDirection).Rotator();
		SetActorRotation(FRotator(0.f, LookRotation.Yaw, 0.f));
		LaunchCharacter(DodgeDirection * DodgeStrength, true, true);
		PlayDodgeMontage();
		ActionState = EActionState::EAS_Dodging;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); // I-FRAMES // testing
		/* Q Learning */
		UpdateQEnemyData();
	}
	
}

void AKnightCharacter::AttackInput(const FInputActionValue& Value)
{
	// Finish?
	if (CanAttack()) 
	{
		PlayAttackMontage();
		ActionState = EActionState::EAS_Attacking;
		
		/* Q Learning */
		UpdateQEnemyData();
	}
}

void AKnightCharacter::GuardInput(const FInputActionValue& Value)
{
	if (CanGuard())
	{
		ActionState = EActionState::EAS_Guarding;
		PlayGuardMontage();
		if (GuardBoxComponent) GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // Usually Done in ABP i.e. for Weapon Collision

		/* Q Learning */
		UpdateQEnemyData();
	}
	
	// note: InputAction - could maybe use some sort of Pulse trigger for parry
}

void AKnightCharacter::GuardReleasedInput(const FInputActionValue& Value)
{
	StopMontage(GuardMontage);
	ActionState = EActionState::EAS_Unoccupied;
	if (GuardBoxComponent) GuardBoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // Usually Done in ABP i.e. for Weapon Collision

	/* Q Learning */
	UpdateQEnemyData();
}

void AKnightCharacter::HealInput(const FInputActionValue& Value) // Pulse Time of 1.0 Sec (Can't heal repeatedly for 1 sec intervals)
{
	// TODO: Heal Animation + Heal Potions  
	Attributes->Heal();
	/* Q Learning */
	UpdateQEnemyData();
}

void AKnightCharacter::OnAttackEnd() // Called in character blueprint on anim AttackEnd Notify
{
	ActionState = EActionState::EAS_Unoccupied;

	/* Q Learning */
	UpdateQEnemyData();
}

void AKnightCharacter::OnDodgeEnd()
{
	ActionState = EActionState::EAS_Unoccupied;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // I-FRAMES // testing
	UpdateQEnemyData();
}

void AKnightCharacter::OnDisarm()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("SpineSocket")); // transfers weapon attachment socket
	}
}

void AKnightCharacter::OnArm()
{
	if (EquippedWeapon) 
	{
		EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("RightHandSocket"));
	}
}

void AKnightCharacter::OnFinishedEquipping()
{
	ActionState = EActionState::EAS_Unoccupied;
}


bool AKnightCharacter::CanAttack() const // Look into timings
{
	return IsUnoccupied() && CharacterState != ECharacterState::ECS_Unequipped;
}

bool AKnightCharacter::CanDodge() const // Look into timings
{
	return IsUnoccupied();
}

bool AKnightCharacter::CanGuard() const
{
	return IsUnoccupied();
}

bool AKnightCharacter::CanDisarm() const
{
	return IsUnoccupied() &&
		CharacterState != ECharacterState::ECS_Unequipped &&
			EquippedWeapon;
}

bool AKnightCharacter::CanArm() const
{
	return IsUnoccupied() &&
		CharacterState == ECharacterState::ECS_Unequipped &&
			EquippedWeapon;
}

void AKnightCharacter::CheckCombatEnemies()
{
	CombatEnemies.Empty();

	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemy::StaticClass(), FoundEnemies);

	for (AActor* Actor : FoundEnemies)
	{
		AEnemy* Enemy = Cast<AEnemy>(Actor);
		if (!Enemy) continue;

		const float Distance = FVector::Dist(GetActorLocation(), Enemy->GetActorLocation());
		if (Distance < CombatRadius)
		{
			CombatEnemies.Add(Enemy);
			/* Q Learning */
			if (AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(Enemy); QEnemy)
			{
				QEnemies.Add(QEnemy);
			}
		}
		
	}
	
}


void AKnightCharacter::FindQEnemies()
{
	QEnemies.Empty();

	TArray<AActor*> FoundQEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AQLearningEnemy::StaticClass(), FoundQEnemies);

	for (AActor* Actor : FoundQEnemies)
	{
		AQLearningEnemy* QEnemy = Cast<AQLearningEnemy>(Actor);
		if (!QEnemy) continue;
		QEnemies.Add(QEnemy);
	}
}

void AKnightCharacter::StartQEnemyUpdateTimer() 
{
	//GetWorldTimerManager().SetTimer(QUpdateTimer, this, &AKnightCharacter::UpdateQEnemyData, UpdateTimeSecs, true);
}

void AKnightCharacter::UpdateQEnemyData() /* HealthPercent, Distance */
{
	if (QEnemies.IsEmpty()) return;

	for (AQLearningEnemy* QEnemy : QEnemies)
	{
		float HealthPercent = Attributes->GetHealthPercent();

		// TODO: Temporary (most likely called from anim notifies) 
		bool bIsAttacking = ActionState == EActionState::EAS_Attacking;
		bool bIsGuarding = ActionState == EActionState::EAS_Guarding;
		bool bIsDodging = ActionState == EActionState::EAS_Dodging;


		QEnemy->UpdateQState_Target(
			HealthPercent,
			bIsAttacking,
			bIsGuarding,
			bIsDodging
		);
		
	}
}

void AKnightCharacter::AddQRewardToEnemies(float Reward)
{
	if (QEnemies.IsEmpty()) return;
	
	for (AQLearningEnemy* QEnemy : QEnemies)
	{
		QEnemy->AddQReward(Reward);
	}
}


void AKnightCharacter::PlayAttackMontage() 
{
	if (!AttackMontage || AttackMontage->GetNumSections() <= 0) return;
	
	if (GetCharacterMovement()->IsFalling()) AttackMontageSelection = AttackMontageSelection; // TODO: Falling Attack Section 

	const int32 MaxSectionIndex = AttackMontage->GetNumSections() - 1;
	const FName SectionName = AttackMontage->GetSectionName(AttackMontageSelection);

	PlayMontageSection(AttackMontage, SectionName);

	if (++AttackMontageSelection >= MaxSectionIndex) AttackMontageSelection = 0;
	
}

void AKnightCharacter::PlayDodgeMontage()
{
	if (DodgeMontage)
	{
		DodgeMontage->RateScale = DodgeSpeed;
	}
	Super::PlayDodgeMontage();
}

void AKnightCharacter::PlayEquipMontage(const FName& SectionName)
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && EquipMontage) 
	{
		AnimInstance->Montage_Play(EquipMontage);
		AnimInstance->Montage_JumpToSection(SectionName, EquipMontage);

	}
}


