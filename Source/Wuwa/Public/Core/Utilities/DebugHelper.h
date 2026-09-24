#pragma once
#include "CoreMinimal.h"

namespace Debug
{
    // FColor、GEngine 属于Engine模块
    static void Print(const FString& Msg, const FColor& color = FColor::MakeRandomColor(), int32 InKey = -1)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(InKey, 3.f, color, Msg);
        }
    }
}