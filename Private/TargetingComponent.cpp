#include "TargetingComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "TargetableInterface.h"

UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Optimization: Tick after physics to ensure the most accurate location is acquired
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	bIsTargeting = false;
	bIsCameraInputIgnored = false;
	bIsResettingCamera = false;
	bIsDynamicTargetingPaused = false;

	CurrentTarget = nullptr;
	ActiveCamera = nullptr;
	ActiveSpringArm = nullptr;

	OriginalCameraDistance = 0.0f;
	OriginalSocketOffset = FVector::ZeroVector;
}

void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsTargeting)
	{
		StopTargeting();
	}

	// Failsafe: Ensure camera is completely reset when destroying the component
	ForceResetCamera();

	Super::EndPlay(EndPlayReason);
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	// ================= Core Targeting Logic =================
	if (bIsTargeting)
	{
		bool bNeedsNewTarget = false;

		// 1. Safety check and state update
		if (!IsValid(CurrentTarget) || GetEnemyHealth(CurrentTarget) <= 0.0f)
		{
			bNeedsNewTarget = true;
		}
		else
		{
			float DistSq = FVector::DistSquared(Owner->GetActorLocation(), CurrentTarget->GetActorLocation());
			if (DistSq > FMath::Square(TargetingRadius))
			{
				bNeedsNewTarget = true; // Out of maximum distance
			}
			else if (!bIsDynamicTargetingPaused)
			{
				// If not manually paused, dynamically check for a better target in real-time
				FindAndSetBestTarget();
				if (!IsValid(CurrentTarget))
				{
					StopTargeting();
					return;
				}
			}
		}

		// 2. Retargeting mechanism after target loss
		if (bNeedsNewTarget)
		{
			bIsDynamicTargetingPaused = false;
			FindAndSetBestTarget();

			// [Crucial]: Completely stop only when no valid enemies are found both on and off-screen
			if (!IsValid(CurrentTarget))
			{
				StopTargeting();
				return;
			}
		}

		// 3. Smooth camera tracking logic
		if (IsValid(ActiveCamera) && IsValid(CurrentTarget))
		{
			if (APlayerController* PC = GetPlayerController())
			{
				FVector CameraLoc = ActiveCamera->GetComponentLocation();
				FVector TargetLoc = CurrentTarget->GetActorLocation() + LockOnOffset;

				FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(CameraLoc, TargetLoc);
				TargetRot.Pitch = FMath::Clamp(TargetRot.Pitch, MinCameraPitch, MaxCameraPitch);

				FRotator InterpRot = FMath::RInterpTo(PC->GetControlRotation(), TargetRot, DeltaTime, CameraInterpSpeed);
				PC->SetControlRotation(InterpRot);

				// Dynamic mapping of spring arm distance and height
				if (IsValid(ActiveSpringArm))
				{
					// Perform square root only once here to save performance
					float ActualDist = FVector::Dist(Owner->GetActorLocation(), CurrentTarget->GetActorLocation());

					float TargetCamDist = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, TargetingRadius), FVector2D(MinCameraDistance, MaxCameraDistance), ActualDist);
					float TargetCamHeight = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, TargetingRadius), FVector2D(MinCameraHeight, MaxCameraHeight), ActualDist);

					ActiveSpringArm->TargetArmLength = FMath::FInterpTo(ActiveSpringArm->TargetArmLength, TargetCamDist, DeltaTime, CameraInterpSpeed);

					FVector NewSocketOffset = ActiveSpringArm->SocketOffset;
					NewSocketOffset.Z = FMath::FInterpTo(NewSocketOffset.Z, TargetCamHeight, DeltaTime, CameraInterpSpeed);
					ActiveSpringArm->SocketOffset = NewSocketOffset;
				}
			}
		}
	}
	// ================= Camera Reset Transition Logic =================
	else if (bIsResettingCamera)
	{
		if (IsValid(ActiveSpringArm))
		{
			ActiveSpringArm->TargetArmLength = FMath::FInterpTo(ActiveSpringArm->TargetArmLength, OriginalCameraDistance, DeltaTime, CameraInterpSpeed);
			ActiveSpringArm->SocketOffset = FMath::VInterpTo(ActiveSpringArm->SocketOffset, OriginalSocketOffset, DeltaTime, CameraInterpSpeed);

			if (FMath::IsNearlyEqual(ActiveSpringArm->TargetArmLength, OriginalCameraDistance, 2.0f) &&
				ActiveSpringArm->SocketOffset.Equals(OriginalSocketOffset, 2.0f))
			{
				// Successfully reset, use the failsafe function to finalize and clean up safely
				ForceResetCamera();
			}
		}
		else
		{
			// If SpringArm became invalid during the reset process, clean up instantly
			ForceResetCamera();
		}
	}
	// ================= FAILSAFE: Final Confirmation =================
	// If we are not targeting, and not resetting, but the pointers are still lingering,
	// it means the camera state failed to return to default. Force it now.
	else if (!bIsTargeting && !bIsResettingCamera && (IsValid(ActiveSpringArm) || IsValid(ActiveCamera)))
	{
		ForceResetCamera();
	}
}

bool UTargetingComponent::StartTargeting()
{
	if (bIsTargeting && IsValid(CurrentTarget)) return true;

	bIsTargeting = true;
	bIsResettingCamera = false;
	bIsDynamicTargetingPaused = false;

	FindAndSetBestTarget();

	if (!IsValid(CurrentTarget))
	{
		StopTargeting();
		return false;
	}
	return true;
}

void UTargetingComponent::StopTargeting()
{
	if (!bIsTargeting) return;

	bIsTargeting = false;
	bIsDynamicTargetingPaused = false;

	if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
	{
		ITargetableInterface::Execute_OnUntargeted(CurrentTarget);
	}
	CurrentTarget = nullptr;

	if (APlayerController* PC = GetPlayerController())
	{
		if (bIsCameraInputIgnored)
		{
			PC->ResetIgnoreLookInput();
			bIsCameraInputIgnored = false;
		}
	}

	if (IsValid(ActiveSpringArm))
	{
		bIsResettingCamera = true;
	}
	else
	{
		ActiveCamera = nullptr;
	}

	// Trigger Blueprint event to notify UI or Sound
	OnTargetStopped.Broadcast();
}

void UTargetingComponent::ForceResetCamera()
{
	if (IsValid(ActiveSpringArm))
	{
		ActiveSpringArm->TargetArmLength = OriginalCameraDistance;
		ActiveSpringArm->SocketOffset = OriginalSocketOffset;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		if (bIsCameraInputIgnored)
		{
			PC->ResetIgnoreLookInput();
		}
	}

	// Clean up all states and pointers
	bIsCameraInputIgnored = false;
	bIsResettingCamera = false;
	ActiveCamera = nullptr;
	ActiveSpringArm = nullptr;
}

// Core optimization: Single pass loop, maintaining both on-screen and off-screen best candidates simultaneously
void UTargetingComponent::FindAndSetBestTarget()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	TArray<AActor*> OutActors;
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		Owner->GetActorLocation(),
		TargetingRadius,
		TargetObjectTypes,
		AActor::StaticClass(),
		TArray<AActor*> { Owner },
		OutActors
	);

	AActor* BestOnScreenTarget = nullptr;
	float BestOnScreenCost = MAX_flt;

	AActor* BestOffScreenTarget = nullptr;
	float BestOffScreenCost = MAX_flt;

	FVector OwnerLoc = Owner->GetActorLocation();

	for (AActor* Enemy : OutActors)
	{
		if (!IsValid(Enemy) || GetEnemyHealth(Enemy) <= 0.0f) continue;
		if (!Enemy->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())) continue;

		float DistSq = FVector::DistSquared(OwnerLoc, Enemy->GetActorLocation());
		float Cost = CalculateTargetCost(Enemy, DistSq);

		if (IsEnemyOnScreen(Enemy))
		{
			if (Cost < BestOnScreenCost)
			{
				BestOnScreenCost = Cost;
				BestOnScreenTarget = Enemy;
			}
		}
		else
		{
			if (Cost < BestOffScreenCost)
			{
				BestOffScreenCost = Cost;
				BestOffScreenTarget = Enemy;
			}
		}
	}

	// Prioritize on-screen targets if available; otherwise, fallback to off-screen targets
	AActor* NewTarget = IsValid(BestOnScreenTarget) ? BestOnScreenTarget : BestOffScreenTarget;

	if (NewTarget != CurrentTarget)
	{
		if (IsValid(CurrentTarget))
		{
			ITargetableInterface::Execute_OnUntargeted(CurrentTarget);
		}
		CurrentTarget = NewTarget;
		if (IsValid(CurrentTarget))
		{
			ITargetableInterface::Execute_OnTargeted(CurrentTarget);
		}
	}
}

float UTargetingComponent::CalculateTargetCost(AActor* Enemy, float DistSquared)
{
	// Use Cost calculation method (lower is higher priority). Use squared distance directly instead of square root for better performance
	float HealthCost = GetEnemyHealth(Enemy) * HealthWeight;
	float DistanceCost = (DistSquared / 10000.0f) * DistanceWeight; // Scale to prevent the value from being too large
	return HealthCost + DistanceCost;
}

bool UTargetingComponent::IsEnemyOnScreen(AActor* Enemy)
{
	if (!IsValid(Enemy)) return false;
	APlayerController* PC = GetPlayerController();
	if (!PC) return false;

	FVector2D ScreenPos;
	bool bIsOnScreen = PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), ScreenPos);

	if (bIsOnScreen)
	{
		int32 ViewportX, ViewportY;
		PC->GetViewportSize(ViewportX, ViewportY);
		// Ensure it is not only in front but also strictly within the screen viewport
		if (ScreenPos.X > 0 && ScreenPos.X < ViewportX && ScreenPos.Y > 0 && ScreenPos.Y < ViewportY)
		{
			return true;
		}
	}
	return false;
}

float UTargetingComponent::GetEnemyHealth(AActor* Enemy)
{
	if (IsValid(Enemy) && Enemy->Implements<UTargetableInterface>())
	{
		return ITargetableInterface::Execute_GetCurrentHealth(Enemy);
	}

	return 0.0f;
}

APlayerController* UTargetingComponent::GetPlayerController() const
{
	if (AActor* Owner = GetOwner())
	{
		if (APawn* Pawn = Cast<APawn>(Owner))
		{
			return Cast<APlayerController>(Pawn->GetController());
		}
	}
	return nullptr;
}

void UTargetingComponent::CameraLockToEnemy(UCameraComponent* InCamera)
{
	if (!IsValid(InCamera)) return;
	ActiveCamera = InCamera;

	if (USpringArmComponent* SpringArm = Cast<USpringArmComponent>(ActiveCamera->GetAttachParent()))
	{
		ActiveSpringArm = SpringArm;
		OriginalCameraDistance = ActiveSpringArm->TargetArmLength;
		OriginalSocketOffset = ActiveSpringArm->SocketOffset;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		PC->SetIgnoreLookInput(true);
		bIsCameraInputIgnored = true;
	}
}

void UTargetingComponent::SwitchToPrevEnemy()
{
	SwitchTargetByDistance(false);
}

void UTargetingComponent::SwitchToNextEnemy()
{
	SwitchTargetByDistance(true);
}

void UTargetingComponent::SwitchTargetByDistance(bool bNext)
{
	if (!bIsTargeting || !IsValid(CurrentTarget)) return;

	// Pause dynamic targeting when manually switching
	bIsDynamicTargetingPaused = true;

	AActor* Owner = GetOwner();
	if (!IsValid(Owner)) return;

	TArray<AActor*> OutActors;
	UKismetSystemLibrary::SphereOverlapActors(this, Owner->GetActorLocation(), TargetingRadius, TargetObjectTypes, AActor::StaticClass(), TArray<AActor*> { Owner, CurrentTarget }, OutActors);

	AActor* BestCandidate = nullptr;
	float MinDistSq = MAX_flt;

	for (AActor* Enemy : OutActors)
	{
		if (!IsValid(Enemy) || GetEnemyHealth(Enemy) <= 0.0f) continue;
		if (!Enemy->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())) continue;

		// Can be expanded to left/right filtering algorithm based on needs, currently using distance as an example
		float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Enemy->GetActorLocation());
		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			BestCandidate = Enemy;
		}
	}

	if (IsValid(BestCandidate))
	{
		ITargetableInterface::Execute_OnUntargeted(CurrentTarget);
		CurrentTarget = BestCandidate;
		ITargetableInterface::Execute_OnTargeted(CurrentTarget);
	}
}
