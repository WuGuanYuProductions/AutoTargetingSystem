#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class UCameraComponent;
class APlayerController;
class USpringArmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTargetStoppedSignature);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TSTPROJECTCPP_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool StartTargeting();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void StopTargeting();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SwitchToPrevEnemy();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SwitchToNextEnemy();

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetCurrentLockedEnemy() const { return IsValid(CurrentTarget) ? CurrentTarget : nullptr; }

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void CameraLockToEnemy(UCameraComponent* InCamera);

	// FAILSAFE: Instantly force the camera to reset to its default original state
	UFUNCTION(BlueprintCallable, Category = "Targeting|Failsafe")
	void ForceResetCamera();

	UPROPERTY(BlueprintAssignable, Category = "Targeting|Events")
	FOnTargetStoppedSignature OnTargetStopped;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config")
	float TargetingRadius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config")
	TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Scoring")
	float HealthWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Config|Scoring")
	float DistanceWeight = 0.5f;

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

	bool bIsTargeting;
	bool bIsCameraInputIgnored;
	bool bIsResettingCamera;
	bool bIsDynamicTargetingPaused;

	APlayerController* GetPlayerController() const;
	void FindAndSetBestTarget();
	float CalculateTargetCost(AActor* Enemy, float DistSquared);
	bool IsEnemyOnScreen(AActor* Enemy);
	float GetEnemyHealth(AActor* Enemy);
	void SwitchTargetByDistance(bool bNext);
};
