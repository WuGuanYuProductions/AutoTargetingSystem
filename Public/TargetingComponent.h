#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class UCameraComponent;
class APlayerController;
class USpringArmComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TSTPROJECTCPP_API UTargetingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTargetingComponent();

protected:
    virtual void BeginPlay() override;

    // 【Bug修复】：防止组件被销毁时没有归还控制权
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    AActor* StartTargeting();

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void StopTargeting();

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void SwitchToPrevEnemy();

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void SwitchToNextEnemy();

    UFUNCTION(BlueprintPure, Category = "Targeting")
    AActor* GetCurrentLockedEnemy() const { return CurrentTarget; }

    UFUNCTION(BlueprintCallable, Category = "Targeting")
    void CameraLockToEnemy(UCameraComponent* InCamera);

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config")
    float TargetingRadius = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config")
    TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Scoring")
    float HealthWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Scoring")
    float DistanceWeight = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Scoring")
    float ZAxisWeight = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    FVector LockOnOffset = FVector(0.0f, 0.0f, 50.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float CameraInterpSpeed = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MinCameraDistance = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MaxCameraDistance = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MinCameraHeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MaxCameraHeight = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MinCameraPitch = -60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Camera")
    float MaxCameraPitch = 45.0f;

private:
    UPROPERTY()
    AActor* CurrentTarget;

    UPROPERTY()
    UCameraComponent* ActiveCamera;

    UPROPERTY()
    USpringArmComponent* ActiveSpringArm;

    float OriginalCameraDistance;
    FVector OriginalSocketOffset;

    APlayerController* GetPlayerController() const;

    bool bIsTargeting;
    bool bIsCameraInputIgnored;

    // 【迭代1】：用于标记当前是否处于镜头归位过渡期
    bool bIsResettingCamera;

    void FindAndSetBestTarget();
    TArray<AActor*> GetValidEnemiesOnScreen();
    float CalculateTargetScore(AActor* Enemy);
    bool IsEnemyOnScreen(AActor* Enemy);
    float GetEnemyHealth(AActor* Enemy);
    void SwitchTargetByDistance(int32 Direction);
};