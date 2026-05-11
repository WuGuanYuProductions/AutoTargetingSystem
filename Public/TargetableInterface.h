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
    // 获取当前血量
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    float GetCurrentHealth() const;

    // --- 新增：被锁定时的事件 ---
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    void OnTargeted();

    // --- 新增：取消锁定时的事件 ---
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    void OnUntargeted();
};