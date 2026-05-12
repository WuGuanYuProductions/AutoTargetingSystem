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
    if (bIsTargeting) StopTargeting();
    Super::EndPlay(EndPlayReason);
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsTargeting && IsValid(CurrentTarget))
    {
        AActor* Owner = GetOwner();
        if (!Owner) return;

        float DistSqToTarget = FVector::DistSquared(Owner->GetActorLocation(), CurrentTarget->GetActorLocation());
        float RadiusSq = TargetingRadius * TargetingRadius;

        // When enemy is invalid(dead or over distance), find a new target
        if (DistSqToTarget > RadiusSq || GetEnemyHealth(CurrentTarget) <= 0.0f)
        {
            bIsDynamicTargetingPaused = false; // resume to dynamic targeting
            FindAndSetBestTarget();
            if (!IsValid(CurrentTarget))
            {
                StopTargeting();
                return;
            }
        }
        // if the target is valid and has higher scores, then switch target
        else if (!bIsDynamicTargetingPaused)
        {
            FindAndSetBestTarget();
            if (!IsValid(CurrentTarget))
            {
                StopTargeting();
                return;
            }
        }

        // camera control logic
        if (ActiveCamera && IsValid(CurrentTarget))
        {
            if (APlayerController* PC = GetPlayerController())
            {
                FVector CameraLoc = ActiveCamera->GetComponentLocation();
                FVector TargetLoc = CurrentTarget->GetActorLocation() + LockOnOffset;

                FRotator TargetRot = UKismetMathLibrary::FindLookAtRotation(CameraLoc, TargetLoc);
                TargetRot.Pitch = FMath::Clamp(TargetRot.Pitch, MinCameraPitch, MaxCameraPitch);

                FRotator CurrentRot = PC->GetControlRotation();
                FRotator InterpRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaTime, CameraInterpSpeed);
                PC->SetControlRotation(InterpRot);

                if (ActiveSpringArm)
                {
                    float ActualDist = FMath::Sqrt(DistSqToTarget);

                    float TargetCamDist = FMath::GetMappedRangeValueClamped(
                        FVector2D(0.0f, TargetingRadius), FVector2D(MinCameraDistance, MaxCameraDistance), ActualDist);

                    float TargetCamHeight = FMath::GetMappedRangeValueClamped(
                        FVector2D(0.0f, TargetingRadius), FVector2D(MinCameraHeight, MaxCameraHeight), ActualDist);

                    ActiveSpringArm->TargetArmLength = FMath::FInterpTo(ActiveSpringArm->TargetArmLength, TargetCamDist, DeltaTime, CameraInterpSpeed);

                    FVector NewSocketOffset = ActiveSpringArm->SocketOffset;
                    NewSocketOffset.Z = FMath::FInterpTo(NewSocketOffset.Z, TargetCamHeight, DeltaTime, CameraInterpSpeed);
                    ActiveSpringArm->SocketOffset = NewSocketOffset;
                }
            }
        }
    }
    // camera back to default
    else if (bIsResettingCamera && ActiveSpringArm)
    {
        ActiveSpringArm->TargetArmLength = FMath::FInterpTo(ActiveSpringArm->TargetArmLength, OriginalCameraDistance, DeltaTime, CameraInterpSpeed);
        ActiveSpringArm->SocketOffset = FMath::VInterpTo(ActiveSpringArm->SocketOffset, OriginalSocketOffset, DeltaTime, CameraInterpSpeed);

        if (FMath::IsNearlyEqual(ActiveSpringArm->TargetArmLength, OriginalCameraDistance, 2.0f) &&
            ActiveSpringArm->SocketOffset.Equals(OriginalSocketOffset, 2.0f))
        {
            ActiveSpringArm->TargetArmLength = OriginalCameraDistance;
            ActiveSpringArm->SocketOffset = OriginalSocketOffset;

            bIsResettingCamera = false;
            ActiveCamera = nullptr;
            ActiveSpringArm = nullptr;
        }
    }
}

APlayerController* UTargetingComponent::GetPlayerController() const
{
    if (AActor* Owner = GetOwner())
    {
        if (APawn* Pawn = Cast<APawn>(Owner)) return Cast<APlayerController>(Pawn->GetController());
    }
    return nullptr;
}

AActor* UTargetingComponent::StartTargeting()
{
    if (bIsTargeting) return CurrentTarget;

    bIsTargeting = true;
    bIsResettingCamera = false;
    bIsDynamicTargetingPaused = false; // Enable dynamic targeting as default

    FindAndSetBestTarget();

    if (!IsValid(CurrentTarget)) StopTargeting();

    return CurrentTarget;
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

    if (ActiveSpringArm)
    {
        bIsResettingCamera = true;
    }
    else
    {
        ActiveCamera = nullptr;
    }
}

void UTargetingComponent::CameraLockToEnemy(UCameraComponent* InCamera)
{
    if (bIsTargeting && CurrentTarget && InCamera)
    {
        ActiveCamera = InCamera;
        ActiveSpringArm = Cast<USpringArmComponent>(ActiveCamera->GetAttachParent());

        if (ActiveSpringArm)
        {
            OriginalCameraDistance = ActiveSpringArm->TargetArmLength;
            OriginalSocketOffset = ActiveSpringArm->SocketOffset;
        }

        if (APlayerController* PC = GetPlayerController())
        {
            if (!bIsCameraInputIgnored)
            {
                PC->SetIgnoreLookInput(true);
                bIsCameraInputIgnored = true;
            }
        }
    }
}

void UTargetingComponent::SwitchToPrevEnemy() { SwitchTargetByDistance(-1); }
void UTargetingComponent::SwitchToNextEnemy() { SwitchTargetByDistance(1); }

// Switch Enemy by distance,if this enabled, then pause dynamic targeting【补全代码与迭代】：按距离切换敌人，并触发动态锁敌的暂停
void UTargetingComponent::SwitchTargetByDistance(int32 Direction)
{
    if (!bIsTargeting || !IsValid(CurrentTarget)) return;

    TArray<AActor*> ValidEnemies = GetValidEnemiesOnScreen();
    if (ValidEnemies.Num() <= 1) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    // Only depends on actual distance between player and enemy
    ValidEnemies.Sort([Owner](const AActor& A, const AActor& B) {
        float DistA = FVector::DistSquared(Owner->GetActorLocation(), A.GetActorLocation());
        float DistB = FVector::DistSquared(Owner->GetActorLocation(), B.GetActorLocation());
        return DistA < DistB;
        });

    int32 CurrentIndex = ValidEnemies.Find(CurrentTarget);
    if (CurrentIndex != INDEX_NONE)
    {
        int32 NextIndex = CurrentIndex + Direction;
        if (NextIndex >= ValidEnemies.Num()) NextIndex = 0; // do loop to start
        if (NextIndex < 0) NextIndex = ValidEnemies.Num() - 1; // do loop to end

        AActor* NewTarget = ValidEnemies[NextIndex];

        if (NewTarget != CurrentTarget)
        {
            if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
                ITargetableInterface::Execute_OnUntargeted(CurrentTarget);

            CurrentTarget = NewTarget;

            if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
                ITargetableInterface::Execute_OnTargeted(CurrentTarget);

            // If player switch target manually then pause dynamic targeting
            bIsDynamicTargetingPaused = true;
        }
    }
}

// ---------------- 以下为底层过滤与评分核心逻辑 ----------------

void UTargetingComponent::FindAndSetBestTarget()
{
    TArray<AActor*> ValidEnemies = GetValidEnemiesOnScreen();
    AActor* BestTarget = nullptr;
    float BestScore = MAX_FLT;

    for (AActor* Enemy : ValidEnemies)
    {
        float Score = CalculateTargetScore(Enemy);
        if (Score < BestScore)
        {
            BestScore = Score;
            BestTarget = Enemy;
        }
    }

    if (CurrentTarget != BestTarget)
    {
        if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
        {
            ITargetableInterface::Execute_OnUntargeted(CurrentTarget);
        }

        CurrentTarget = BestTarget;

        if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
        {
            ITargetableInterface::Execute_OnTargeted(CurrentTarget);
        }
    }
}

TArray<AActor*> UTargetingComponent::GetValidEnemiesOnScreen()
{
    TArray<AActor*> OverlappingActors;
    TArray<AActor*> ValidEnemies;
    AActor* Owner = GetOwner();

    if (!Owner) return ValidEnemies;

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(Owner);

    UKismetSystemLibrary::SphereOverlapActors(
        this, Owner->GetActorLocation(), TargetingRadius, TargetObjectTypes, AActor::StaticClass(), ActorsToIgnore, OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (IsValid(Actor) && Actor->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
        {
            if (GetEnemyHealth(Actor) > 0.0f && IsEnemyOnScreen(Actor))
            {
                ValidEnemies.Add(Actor);
            }
        }
    }
    return ValidEnemies;
}

float UTargetingComponent::CalculateTargetScore(AActor* Enemy)
{
    AActor* Owner = GetOwner();
    if (!Owner || !IsValid(Enemy)) return MAX_FLT;

    float Health = GetEnemyHealth(Enemy);
    FVector OwnerLoc = Owner->GetActorLocation();
    FVector EnemyLoc = Enemy->GetActorLocation();

    float Distance2D = FVector::Dist2D(OwnerLoc, EnemyLoc);
    float ZDiff = FMath::Abs(OwnerLoc.Z - EnemyLoc.Z);

    return (Health * HealthWeight) + (Distance2D * DistanceWeight) + (ZDiff * ZAxisWeight);
}

bool UTargetingComponent::IsEnemyOnScreen(AActor* Enemy)
{
    APlayerController* PC = GetPlayerController();
    if (!PC || !IsValid(Enemy)) return false;

    FVector2D ScreenPosition;
    bool bIsOnScreen = PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), ScreenPosition);

    if (bIsOnScreen)
    {
        int32 ViewportSizeX, ViewportSizeY;
        PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

        if (ScreenPosition.X > 0.0f && ScreenPosition.X < ViewportSizeX &&
            ScreenPosition.Y > 0.0f && ScreenPosition.Y < ViewportSizeY)
        {
            return true;
        }
    }
    return false;
}

float UTargetingComponent::GetEnemyHealth(AActor* Enemy)
{
    if (IsValid(Enemy) && Enemy->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
    {
        return ITargetableInterface::Execute_GetCurrentHealth(Enemy);
    }
    return 0.0f;
}
