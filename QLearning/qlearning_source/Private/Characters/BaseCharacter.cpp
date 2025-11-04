#include "Characters/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Items/Weapons/Weapon.h"
#include "Components/AttributeComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UniversalObjectLocators/AnimInstanceLocatorFragment.h"

ABaseCharacter::ABaseCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	Attributes = CreateDefaultSubobject<UAttributeComponent>(TEXT("Attributes"));

}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void ABaseCharacter::AttackInput(const FInputActionValue& Value) //Character
{
}

void ABaseCharacter::Attack() // Enemy
{
}

void ABaseCharacter::Dodge()
{
}
void ABaseCharacter::DodgeInput(const FInputActionValue& Value)
{
	// TODO
}
void ABaseCharacter::Heal(const int32 HealAmount)
{
	if (!Attributes) { UE_LOG(LogTemp, Warning, TEXT("Attributes not defined")); return; }
	Attributes->Heal(HealAmount);
}

void ABaseCharacter::PlayAttackMontage()
{
	PlayRandomMontageSection(AttackMontage);
}

void ABaseCharacter::PlayDeathMontage()
{
	PlayRandomMontageSection(DeathMontage);
}

void ABaseCharacter::PlayDodgeMontage()
{
	//PlayMontageSection(DodgeMontage, 0);
	PlayRandomMontageSection(DodgeMontage);
}

void ABaseCharacter::PlayGuardMontage()
{
	PlayRandomMontageSection(GuardMontage);
}

void ABaseCharacter::PlayHitReactMontage(const FName& SectionName) const
{
	PlayMontageSection(HitReactMontage, SectionName);
}

void ABaseCharacter::PlayMontageSection(UAnimMontage* Montage, const FName& SectionName) const
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && Montage)
	{
		AnimInstance->Montage_Play(Montage);
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}
	
}

void ABaseCharacter::PlayMontageSection(UAnimMontage* Montage, const int32 Index) const
{
	if (!Montage || Montage->GetNumSections() <= 0) return;
	const int32 MaxSectionIndex = Montage->GetNumSections() - 1;
	const int32 Selection = FMath::Clamp(Index, 0, MaxSectionIndex); // could add Log here if over/under max/min index
	const FName SectionName = Montage->GetSectionName(Selection);
	
	PlayMontageSection(Montage, SectionName);
}

void ABaseCharacter::PlayRandomMontageSection(UAnimMontage* Montage) const
{
	if (!Montage || Montage->GetNumSections() <= 0) return;
	const int32 MaxSectionIndex = Montage->GetNumSections() - 1;
	const int32 Selection = FMath::RandRange(0, MaxSectionIndex);
	const FName SectionName = Montage->GetSectionName(Selection);
	
	PlayMontageSection(Montage, SectionName);
}

void ABaseCharacter::PlayRandomMontageSection(UAnimMontage* Montage, const int32 MinIndex,
	const int32 MaxIndex) const
{
	if (!Montage || Montage->GetNumSections() <= 0) return;
	const int32 MaxSectionIndex = Montage->GetNumSections() - 1;
	int32 Selection = FMath::RandRange(0, MaxSectionIndex);
	Selection = FMath::Clamp(Selection, MinIndex, MaxIndex);
	const FName SectionName = Montage->GetSectionName(Selection);
	
	PlayMontageSection(Montage, SectionName);
}

void ABaseCharacter::StopMontage(UAnimMontage* Montage) const
{
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance(); AnimInstance && Montage)
	{
		AnimInstance->Montage_Stop(0.25f, Montage);
	}
}


void ABaseCharacter::OnAttackEnd()
{
}

void ABaseCharacter::OnDodgeEnd()
{
}

void ABaseCharacter::Die()
{
}


void ABaseCharacter::DirectionalHitReact(const FVector& ImpactPoint)
{
	const FVector Forward = GetActorForwardVector();
	const FVector ImpactLowered(ImpactPoint.X, ImpactPoint.Y, GetActorLocation().Z); 	// Lower Impact Point to the Enemy's Actor Location
	const FVector ToHit = (ImpactLowered - GetActorLocation()).GetSafeNormal();

	/* DOT PRODUCT */
	// Forward * ToHit = |Forward||ToHit| *  cos(theta) - returns a scalar
	// |Forward|=1, |ToHit|=1, so Forward * ToHit = cos(theta)
	const double CosTheta = FVector::DotProduct(Forward, ToHit);

	// Take the inverse cosine (arc-cosine) of cos(theta) to get theta
	double Theta = FMath::Acos(CosTheta);

	// Convert from radians to degrees
	Theta = FMath::RadiansToDegrees(Theta);


	/* CROSS PRODUCT */
	// A x B = |A||B|sin(theta)N
	// if CrossProduct points down, Theta should be negative
	const FVector CrossProduct = FVector::CrossProduct(Forward, ToHit);
	if (CrossProduct.Z < 0)
	{
		Theta *= -1.f;
	}


	FName Section("LargeFromBack");

	if (Theta >= -45.f && Theta < 45.f) {
		Section = FName("LargeFromFront");
	}
	else if (Theta >= -135.f && Theta < -45.f) {
		Section = FName("LargeFromLeft");
	}
	else if (Theta >= 45.f && Theta < 135.f) {
		Section = FName("LargeFromRight");
	}


	PlayHitReactMontage(Section);

	UE_LOG(LogTemp, Warning, TEXT("Hit React Called"));
	/*  DEBUGGING
	UKismetSystemLibrary::DrawDebugArrow(this, GetActorLocation(), GetActorLocation() + CrossProduct * 100.f, 5.f, FColor::Green, 5.f);

	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Green, FString::Printf(TEXT("Theta: %f"), Theta));
	}
	UKismetSystemLibrary::DrawDebugArrow(this, GetActorLocation(), GetActorLocation() + Forward * 60.f, 5.f, FColor::White, 5.f);
	UKismetSystemLibrary::DrawDebugArrow(this, GetActorLocation(), GetActorLocation() + ToHit * 60.f, 5.f, FColor::Red, 5.f);
	*/
}

void ABaseCharacter::PlayHitSound(const FVector& ImpactPoint) const
{
	if (HitSound) UGameplayStatics::PlaySoundAtLocation(this, HitSound, ImpactPoint); // HitFlesh sfx
}

void ABaseCharacter::SpawnHitParticles(const FVector& ImpactPoint) const
{
	if (HitParticles) UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitParticles, ImpactPoint);
}

void ABaseCharacter::HandleDamage(const float DamageAmount)
{
	if (Attributes)
	{
		Attributes->ReceiveDamage(DamageAmount);
	}
}

bool ABaseCharacter::IsAlive() const
{
	return Attributes && Attributes->IsAlive();
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DeltaSeconds = DeltaTime;
}

void ABaseCharacter::SetWeaponCollisionEnabled(const ECollisionEnabled::Type CollisionEnabled)
{
	if (EquippedWeapon && EquippedWeapon->GetWeaponBox())
	{
		EquippedWeapon->GetWeaponBox()->SetCollisionEnabled(CollisionEnabled);
		EquippedWeapon->IgnoreActors.Empty(); // Clears Actors to ignore on hit (prevents double hitting on single attack)
	}
}



