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
    // 【Bug修复】：销毁前确保镜头控制权归还
    if (bIsTargeting)
    {
        StopTargeting();
    }
    Super::EndPlay(EndPlayReason);
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // 【Bug修复】：使用 IsValid 替代简单的指针判空，防止敌人被Destroy后引发Crash
    if (bIsTargeting && IsValid(CurrentTarget))
    {
        AActor* Owner = GetOwner();
        if (!Owner) return;

        // 【性能优化】：先用不开方的 DistSquared 进行范围淘汰，极大地节约性能
        float DistSqToTarget = FVector::DistSquared(Owner->GetActorLocation(), CurrentTarget->GetActorLocation());
        float RadiusSq = TargetingRadius * TargetingRadius;

        if (DistSqToTarget > RadiusSq || GetEnemyHealth(CurrentTarget) <= 0.0f)
        {
            FindAndSetBestTarget();
            if (!IsValid(CurrentTarget))
            {
                StopTargeting();
                return;
            }
        }

        if (ActiveCamera && IsValid(CurrentTarget))
        {
            APlayerController* PC = GetPlayerController();
            if (PC)
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
                    // 确认在范围内后，再计算实际距离用于平滑映射
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
    // 【迭代1】：StopTargeting被调用后，执行弹簧臂回退的插值过渡逻辑
    else if (bIsResettingCamera && ActiveSpringArm)
    {
        ActiveSpringArm->TargetArmLength = FMath::FInterpTo(ActiveSpringArm->TargetArmLength, OriginalCameraDistance, DeltaTime, CameraInterpSpeed);
        ActiveSpringArm->SocketOffset = FMath::VInterpTo(ActiveSpringArm->SocketOffset, OriginalSocketOffset, DeltaTime, CameraInterpSpeed);

        // 如果已经足够接近原始值，则结束归位逻辑，彻底解绑
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
        if (APawn* Pawn = Cast<APawn>(Owner))
        {
            return Cast<APlayerController>(Pawn->GetController());
        }
    }
    return nullptr;
}

AActor* UTargetingComponent::StartTargeting()
{
    if (bIsTargeting) return CurrentTarget;

    bIsTargeting = true;
    bIsResettingCamera = false; // 取消可能正在进行的镜头重置
    FindAndSetBestTarget();

    if (!IsValid(CurrentTarget))
    {
        StopTargeting();
    }

    return CurrentTarget;
}

void UTargetingComponent::StopTargeting()
{
    if (!bIsTargeting) return;

    bIsTargeting = false;

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

    // 【迭代1】：如果是绑定了弹簧臂，不直接清空指针，而是开启平滑归位模式
    if (ActiveSpringArm)
    {
        bIsResettingCamera = true;
    }
    else
    {
        // 没弹簧臂就直接清空
        ActiveCamera = nullptr;
    }
}

void UTargetingComponent::CameraLockToEnemy(UCameraComponent* InCamera)
{
    if (bIsTargeting && IsValid(CurrentTarget) && InCamera)
    {
        ActiveCamera = InCamera;
        ActiveSpringArm = Cast<USpringArmComponent>(ActiveCamera->GetAttachParent());

        if (ActiveSpringArm)
        {
            OriginalCameraDistance = ActiveSpringArm->TargetArmLength;
            OriginalSocketOffset = ActiveSpringArm->SocketOffset;
        }

        APlayerController* PC = GetPlayerController();
        if (PC && !bIsCameraInputIgnored)
        {
            PC->SetIgnoreLookInput(true);
            bIsCameraInputIgnored = true;
        }
    }
}

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
        this, Owner->GetActorLocation(), TargetingRadius, TargetObjectTypes,
        AActor::StaticClass(), ActorsToIgnore, OverlappingActors);

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
    float Distance2D = FVector::Dist2D(Owner->GetActorLocation(), Enemy->GetActorLocation());
    float ZDiff = FMath::Abs(Owner->GetActorLocation().Z - Enemy->GetActorLocation().Z);

    return (Health * HealthWeight) + (Distance2D * DistanceWeight) + (ZDiff * ZAxisWeight);
}

bool UTargetingComponent::IsEnemyOnScreen(AActor* Enemy)
{
    APlayerController* PC = GetPlayerController();
    if (!PC || !IsValid(Enemy)) return false;

    FVector2D ScreenPosition;
    if (PC->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), ScreenPosition))
    {
        int32 ViewportSizeX, ViewportSizeY;
        PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

        // 增加一点视口容差，防止边缘敌人频繁切断
        float Margin = 50.0f;
        if (ScreenPosition.X >= -Margin && ScreenPosition.X <= (ViewportSizeX + Margin) &&
            ScreenPosition.Y >= -Margin && ScreenPosition.Y <= (ViewportSizeY + Margin))
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

void UTargetingComponent::SwitchToPrevEnemy()
{
    SwitchTargetByDistance(-1);
}

void UTargetingComponent::SwitchToNextEnemy()
{
    SwitchTargetByDistance(1);
}

void UTargetingComponent::SwitchTargetByDistance(int32 Direction)
{
    if (!bIsTargeting) return;

    TArray<AActor*> ValidEnemies = GetValidEnemiesOnScreen();
    if (ValidEnemies.Num() <= 1) return;

    AActor* Owner = GetOwner();

    // 【性能优化】：排序时使用开销极低的 DistSquared 代替原生 Dist
    ValidEnemies.Sort([Owner](const AActor& A, const AActor& B) {
        return FVector::DistSquared(Owner->GetActorLocation(), A.GetActorLocation()) <
            FVector::DistSquared(Owner->GetActorLocation(), B.GetActorLocation());
        });

    int32 CurrentIndex = ValidEnemies.Find(CurrentTarget);
    if (CurrentIndex != INDEX_NONE)
    {
        // 计算新的索引并处理循环溢出 (防止出现负数索引导致崩溃)
        int32 NewIndex = (CurrentIndex + Direction) % ValidEnemies.Num();
        if (NewIndex < 0)
        {
            NewIndex = ValidEnemies.Num() - 1;
        }

        if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
        {
            ITargetableInterface::Execute_OnUntargeted(CurrentTarget);
        }

        CurrentTarget = ValidEnemies[NewIndex];

        if (IsValid(CurrentTarget) && CurrentTarget->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
        {
            ITargetableInterface::Execute_OnTargeted(CurrentTarget);
        }
    }
}