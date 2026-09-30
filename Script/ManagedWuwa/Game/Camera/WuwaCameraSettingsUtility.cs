using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.Camera;

/// <summary>在配置入口集中处理非法数值；后续控制器只处理已归一化的数据。</summary>
internal static class WuwaCameraSettingsUtility
{
    public static FWuwaCameraSettings Normalize(FWuwaCameraSettings settings)
    {
        settings.MinArmLength = Math.Max(0f, Finite(settings.MinArmLength, 150f));
        settings.MaxArmLength = Math.Max(settings.MinArmLength, Finite(settings.MaxArmLength, 800f));
        settings.ArmLength = Math.Clamp(Finite(settings.ArmLength, 400f), settings.MinArmLength, settings.MaxArmLength);
        settings.ZoomStep = Math.Max(0f, Finite(settings.ZoomStep, 50f));
        settings.ZoomInterpSpeed = Math.Max(0f, Finite(settings.ZoomInterpSpeed, 12f));
        settings.BlendTime = Math.Max(0f, Finite(settings.BlendTime, 0.2f));
        settings.FieldOfView = Math.Clamp(Finite(settings.FieldOfView, 90f), 5f, 170f);
        settings.PitchMin = Math.Clamp(Finite(settings.PitchMin, -89.9f), -89.9f, 89.9f);
        settings.PitchMax = Math.Clamp(Finite(settings.PitchMax, 89.9f), settings.PitchMin, 89.9f);
        settings.PivotOffset = FiniteVector(settings.PivotOffset);
        settings.RotationOffset = new FRotator(
            Angle(settings.RotationOffset.Pitch),
            Angle(settings.RotationOffset.Yaw),
            Angle(settings.RotationOffset.Roll));
        settings.ProbeRadius = Math.Max(0f, Finite(settings.ProbeRadius, 12f));
        return settings;
    }

    public static float Finite(float value, float fallback = 0f) => float.IsFinite(value) ? value : fallback;

    public static double Angle(double value) => double.IsFinite(value) ? Math.IEEERemainder(value, 360.0) : 0.0;

    public static bool IsFinite(FVector value) =>
        double.IsFinite(value.X) && double.IsFinite(value.Y) && double.IsFinite(value.Z);

    private static FVector FiniteVector(FVector value) => new(
        double.IsFinite(value.X) ? value.X : 0.0,
        double.IsFinite(value.Y) ? value.Y : 0.0,
        double.IsFinite(value.Z) ? value.Z : 0.0);
}
