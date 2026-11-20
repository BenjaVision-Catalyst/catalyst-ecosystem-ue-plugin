#include "Weather/CEPlanetWeather.h"
#include "Weather/CEWeatherTopology.h"

bool CELocateWeatherGlobeTriangle(const FCEWeatherGlobeMesh& Mesh, const FVector& BodyDirection,
    int32& TriangleHint, FCEWeatherGlobeStencil& Stencil)
{
    if (BodyDirection.IsNearlyZero() || BodyDirection.ContainsNaN())
        return false;

    const FVector Direction = BodyDirection.GetSafeNormal();
    const auto TryTriangle = [&](int32 Triangle)
    {
        const int32 A = Mesh.Indices[3 * Triangle], B = Mesh.Indices[3 * Triangle + 1], C = Mesh.Indices[3 * Triangle + 2];
        const FVector& X = Mesh.Directions[A];
        const FVector& Y = Mesh.Directions[B];
        const FVector& Z = Mesh.Directions[C];
        // Determinants give the ray/triangle barycentric weights up to a common factor.
        double WX = FVector::DotProduct(Direction, FVector::CrossProduct(Y, Z));
        double WY = FVector::DotProduct(Direction, FVector::CrossProduct(Z, X));
        double WZ = FVector::DotProduct(Direction, FVector::CrossProduct(X, Y));
        if (WX < -UE_DOUBLE_SMALL_NUMBER || WY < -UE_DOUBLE_SMALL_NUMBER || WZ < -UE_DOUBLE_SMALL_NUMBER)
            return false;
        WX = FMath::Max(WX, 0.);
        WY = FMath::Max(WY, 0.);
        WZ = FMath::Max(WZ, 0.);
        const double Sum = WX + WY + WZ;
        if (Sum < UE_DOUBLE_SMALL_NUMBER)
            return false;
        Stencil = {{A, B, C}, FVector(WX, WY, WZ) / Sum};
        TriangleHint = Triangle;
        return true;
    };
    const int32 TriangleCount = Mesh.Indices.Num() / 3;
    if (TriangleHint >= 0 && TriangleHint < TriangleCount && TryTriangle(TriangleHint))
        return true;
    for (int32 Triangle = 0; Triangle < TriangleCount; ++Triangle)
        if (TryTriangle(Triangle))
            return true;
    return false;
}


bool CEInterpolateGlobeWeather(const FCEWeatherGlobeMesh& Mesh, TConstArrayView<FCEWeatherSample> Samples,
    const FVector& BodyDirection, int32& TriangleHint, FCEWeatherSample& Result)
{
    FCEWeatherGlobeStencil Stencil;
    if (Samples.Num() != Mesh.Directions.Num() || !CELocateWeatherGlobeTriangle(Mesh, BodyDirection, TriangleHint, Stencil))
        return false;

    const FVector Direction = BodyDirection.GetSafeNormal();
    const auto Blend = [&](auto Member)
    {
        return Samples[Stencil.Vertices.X].*Member * Stencil.Weights.X +
            Samples[Stencil.Vertices.Y].*Member * Stencil.Weights.Y +
            Samples[Stencil.Vertices.Z].*Member * Stencil.Weights.Z;
    };
    Result.TemperatureC = Blend(&FCEWeatherSample::TemperatureC);
    Result.RainMmHour = Blend(&FCEWeatherSample::RainMmHour);
    Result.SnowMmHour = Blend(&FCEWeatherSample::SnowMmHour);
    Result.Dust = Blend(&FCEWeatherSample::Dust);
    Result.Cloud = Blend(&FCEWeatherSample::Cloud);
    Result.PressureHpa = Blend(&FCEWeatherSample::PressureHpa);
    Result.Wind = FVector::VectorPlaneProject(Blend(&FCEWeatherSample::Wind), Direction);
    Result.AirMassTemperatureC = Blend(&FCEWeatherSample::AirMassTemperatureC);
    Result.FrontStrength = Blend(&FCEWeatherSample::FrontStrength);
    Result.FrontalPrecipitationMmHour = Blend(&FCEWeatherSample::FrontalPrecipitationMmHour);
    Result.FrontAcross = Blend(&FCEWeatherSample::FrontAcross);
    Result.FrontCrossWind = Blend(&FCEWeatherSample::FrontCrossWind);
    Result.FrontWarmward = FVector::VectorPlaneProject(Blend(&FCEWeatherSample::FrontWarmward), Direction).GetSafeNormal();
    return true;
}
