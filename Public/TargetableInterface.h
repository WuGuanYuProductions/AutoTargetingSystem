#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetableInterface.generated.h"

UINTERFACE(MinimalAPI)
class UTargetableInterface : public UInterface
{
    GENERATED_BODY()
};

class TSTPROJECTCPP_API ITargetableInterface
{
    GENERATED_BODY()

public:
    // Get enemy current health
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    float GetCurrentHealth() const;

    // Event On Locked
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    void OnTargeted();

    // Event On Unlocked
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    void OnUntargeted();
};
