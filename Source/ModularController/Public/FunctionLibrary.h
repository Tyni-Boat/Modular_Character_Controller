// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "CommonTypes.h"
#include "Engine/HitResult.h"
#include "FunctionLibrary.generated.h"


//Common types function library
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UFunctionLibrary: public UObject
{
	GENERATED_BODY()

public:

	UFunctionLibrary();


	// Get the relative 3D direction from a 2D vector.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Inputs")
	static FVector GetAxisRelativeDirection(FVector2D input, FTransform transformRelative, FVector planeNormal = FVector(0));


	// Get the linear velocity at the previously computed point on the surface.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface")
	static FVector GetSurfaceLinearVelocity(const FSurfaceInfos MyStructRef, bool linear = true, bool angular = true);

	// Get the angular velocity at the previously computed point on the surface.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface")
	static FQuat GetSurfaceAngularVelocity(FSurfaceInfos MyStructRef);

	// Get the hit result computed on the surface.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface")
	static FHitResult GetSurfaceHitInfos(const FSurfaceInfos MyStructRef);

	
	// Get surface friction (X), surface bounciness (Y)
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Physic")
	static FVector GetSurfacePhysicProperties(const FHitResult MyStructRef);


	// Draw a debug circle at the hit point on a surface.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Surface | Debug")
	static void DrawDebugCircleOnSurface(const FHitResult MyStructRef, bool useImpact = false, float radius = 40, FColor color = FColor::White, float duration = 0, float thickness = 1, bool showAxis = false);


	// Get the point to the object T from a soft object pointer.
	template <typename T>
	UFUNCTION(BlueprintCallable, Category = "Function Library | Common Objects")
	static std::enable_if_t<std::is_base_of_v<UObject, T>, T> GetObject(const TSoftObjectPtr<T> softObj);
	
	// Turn toward a direction form angular kinematics.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Transform Tools")
	static FAngularKinematicCondition LookAt(const FAngularKinematicCondition startCondition, const FVector direction, const float withSpeed, const float deltaTime);
	
	// Linear and spherical interpolation between two kinematics.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Kinematics")
	static FKinematicComponents LerpKinematic(const FKinematicComponents A, const FKinematicComponents B, const double delta);


	// Compute the final velocities of two colliding objects A and B, and return true if the operation succeeded.
	static bool ComputeCollisionVelocities(const FVector initialVelA, const FVector initialVelB, const FVector colNornal, const double massA, const double massB
		, const double bounceCoef, FVector& finalA, FVector& finalB);
};
