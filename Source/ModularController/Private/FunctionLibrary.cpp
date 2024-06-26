// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "FunctionLibrary.h"

#include "Components/PrimitiveComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


UFunctionLibrary::UFunctionLibrary()
{
}


FVector UFunctionLibrary::GetAxisRelativeDirection(FVector2D input, FTransform transformRelative, FVector planeNormal)
{
	FVector direction = FVector(0);
	FVector fwd = transformRelative.GetRotation().GetForwardVector();
	FVector rht = transformRelative.GetRotation().GetRightVector();
	if (planeNormal.Length() > 0 && planeNormal.Normalize())
	{
		fwd = FVector::VectorPlaneProject(fwd, planeNormal).GetSafeNormal();
		rht = FVector::VectorPlaneProject(rht, planeNormal).GetSafeNormal();
	}
	const FVector compositeRhs = rht * input.X;
	const FVector compositeFwd = fwd * input.Y;
	direction = compositeFwd + compositeRhs;
	return  direction;
}



FVector UFunctionLibrary::GetSurfaceLinearVelocity(const FSurfaceInfos MyStructRef, bool linear, bool angular)
{
	return MyStructRef.GetSurfaceLinearVelocity(linear, angular);
}


FQuat UFunctionLibrary::GetSurfaceAngularVelocity(FSurfaceInfos MyStructRef)
{
	return MyStructRef.GetSurfaceAngularVelocity();
}


FHitResult UFunctionLibrary::GetSurfaceHitInfos(const FSurfaceInfos MyStructRef)
{
	return MyStructRef.GetHitResult();
}


FVector UFunctionLibrary::GetSurfacePhysicProperties(const FHitResult MyStructRef)
{
	if (!MyStructRef.GetActor())
		return MyStructRef.GetComponent() ? FVector(1, 0, 0) : FVector(0);
	if (!MyStructRef.PhysMaterial.IsValid())
		return FVector(1, 0, 0);

	return FVector(MyStructRef.PhysMaterial->Friction, MyStructRef.PhysMaterial->Restitution, 0);
}



void UFunctionLibrary::DrawDebugCircleOnSurface(const FHitResult MyStructRef, bool useImpact, float radius,	FColor color, float duration, float thickness, bool showAxis)
{
	if (!MyStructRef.GetComponent())
		return;
	FVector up = useImpact ? MyStructRef.ImpactNormal : MyStructRef.Normal;
	if (!up.Normalize())
		return;
	FVector right = up.Rotation().Quaternion().GetAxisY();
	FVector forward = FVector::CrossProduct(right, up);
	FVector::CreateOrthonormalBasis(forward, right, up);
	FVector hitPoint = MyStructRef.ImpactPoint + up * 0.01;
	if (showAxis)
	{
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + up * radius, (radius * 0.25), FColor::Blue, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + forward * (radius * 0.5), (radius * 0.25), FColor::Red, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + right * (radius * 0.5), (radius * 0.25), FColor::Green, duration, thickness);
	}
	UKismetSystemLibrary::DrawDebugCircle(MyStructRef.GetComponent(), hitPoint, radius, 32,
		color, duration, thickness, right, forward);
}



template <typename T>
std::enable_if_t<std::is_base_of_v<UObject, T>, T> UFunctionLibrary::GetObject(const TSoftObjectPtr<T> softObj)
{
	if (!softObj.IsValid())
		return nullptr;
	return softObj.Get();
}



FAngularKinematicCondition UFunctionLibrary::LookAt(const FAngularKinematicCondition startCondition, const FVector direction, const float withSpeed, const float deltaTime)
{
	FAngularKinematicCondition finalAngular = startCondition;
	FVector lookDir = direction;

	if (lookDir.Normalize())
	{
		FQuat orientTarget = lookDir.ToOrientationQuat();
		orientTarget.EnforceShortestArcWith(startCondition.Orientation);
		FQuat diff = startCondition.Orientation.Inverse() * orientTarget;
		float rotSpeed;
		FVector rotAxis;
		diff.ToAxisAndAngle(rotAxis, rotSpeed);
		const float limitedSpeed = FMath::Clamp(withSpeed, 0, 1 / deltaTime);
		finalAngular.RotationSpeed = rotAxis * FMath::RadiansToDegrees(rotSpeed) * limitedSpeed;
	}
	else if (startCondition.RotationSpeed.SquaredLength() > 0)
	{
		finalAngular.AngularAcceleration = -startCondition.RotationSpeed / (deltaTime * 4);
	}

	return finalAngular;
}


FKinematicComponents UFunctionLibrary::LerpKinematic(const FKinematicComponents A, const FKinematicComponents B, const double delta)
{
	FKinematicComponents result = A;
	result.LinearKinematic.Velocity = FMath::Lerp(A.LinearKinematic.Velocity, B.LinearKinematic.Velocity, delta);
	result.LinearKinematic.Position = FMath::Lerp(A.LinearKinematic.Position, B.LinearKinematic.Position, delta);
	result.AngularKinematic.Orientation = FQuat::Slerp(A.AngularKinematic.Orientation, B.AngularKinematic.Orientation, delta);

	return result;
}


bool UFunctionLibrary::ComputeCollisionVelocities(const FVector initialVelA, const FVector initialVelB,
                                                  const FVector colNornal, const double massA, const double massB, const double bounceCoef, FVector& finalA,
                                                  FVector& finalB)
{
	FVector n = colNornal;
	if (!n.Normalize())
		return false;
	finalA = FVector::VectorPlaneProject(initialVelA, n);
	finalB = FVector::VectorPlaneProject(initialVelB, n);
	const FVector Va1 = initialVelA.ProjectOnToNormal(n);
	const FVector Vb1 = initialVelB.ProjectOnToNormal(n);
	const double cfa = bounceCoef * massA;
	const double cfb = bounceCoef * massB;
	const double massSum = massA + massB;
	const FVector Va2 = ((massA - cfb) / massSum) * Va1 + ((massB + cfb) / massSum) * Vb1;
	const FVector Vb2 = ((massB - cfa) / massSum) * Vb1 + ((massA + cfa) / massSum) * Va1;
	finalA += Va2;
	finalB += Vb2;
	return true;
}
