using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Input.Conditions;

/// <summary>示例派生条件：ASC 当前属性达到阈值。对象只有配置，不保存任何角色的运行数据。</summary>
[UClass]
public partial class UInputCondition_AttributeMinimum : UWuwaInputCondition
{
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial FGameplayAttribute Attribute { get; set; }

    [UProperty(PropertyFlags.EditAnywhere)]
    public partial float Minimum { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        return context.ASC.IsValid() && float.IsFinite(Minimum)
            && context.ASC.TryGetAttributeValue(Attribute, out float value) && value >= Minimum;
    }
}
