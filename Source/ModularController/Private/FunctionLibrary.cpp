// Copyright � 2023 by Tyni Boat. All Rights Reserved.


#include "FunctionLibrary.h"

#include "ToolsLibrary.h"
#include "ComponentAndBase/ModularControllerComponent.h"
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
	return direction;
}


FVector UFunctionLibrary::GetSurfacePhysicProperties(const FHitResult MyStructRef)
{
	if (!MyStructRef.GetActor())
		return MyStructRef.GetComponent() ? FVector(1, 0, 0) : FVector(0);
	if (!MyStructRef.PhysMaterial.IsValid())
		return FVector(1, 0, 0);

	const FVector surfaceProps = FVector(MyStructRef.PhysMaterial->Friction, MyStructRef.PhysMaterial->Restitution, 0);
	return surfaceProps;
}

FVector UFunctionLibrary::GetMixedPhysicProperties(const FHitResult MyStructRef, const FVector Base)
{
	if (!MyStructRef.PhysMaterial.IsValid())
		return Base;
	const FVector A = FVector(MyStructRef.PhysMaterial->Friction, MyStructRef.PhysMaterial->Restitution, 0);
	float friction = 0;
	switch (MyStructRef.PhysMaterial->FrictionCombineMode)
	{
		default: friction = (A.X + Base.X) / 2;
			break;
		case EFrictionCombineMode::Max: friction = A.X > Base.X ? A.X : Base.X;
			break;
		case EFrictionCombineMode::Min: friction = A.X < Base.X ? A.X : Base.X;
			break;
		case EFrictionCombineMode::Multiply: friction = A.X * Base.X;
			break;
	}
	float restitution = 0;
	switch (MyStructRef.PhysMaterial->RestitutionCombineMode)
	{
		default: restitution = (A.Y + Base.Y) / 2;
			break;
		case EFrictionCombineMode::Max: restitution = A.Y > Base.Y ? A.Y : Base.Y;
			break;
		case EFrictionCombineMode::Min: restitution = A.Y < Base.Y ? A.Y : Base.Y;
			break;
		case EFrictionCombineMode::Multiply: restitution = A.Y * Base.Y;
			break;
	}

	return FVector(friction, restitution, 0);
}


void UFunctionLibrary::DrawDebugCircleOnHit(const FHitResult MyStructRef, bool useImpact, float radius, FColor color, float duration, float thickness, bool showAxis)
{
	if (!MyStructRef.Component.IsValid())
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

void UFunctionLibrary::DrawDebugCircleOnSurface(const FSurface MyStructRef, float radius, FColor color, float duration, float thickness, bool showAxis, bool useImpact)
{
	if (!MyStructRef.TrackedComponent.IsValid())
		return;
	FVector up = useImpact? MyStructRef.SurfaceImpactNormal : MyStructRef.SurfaceNormal;
	if (!up.Normalize())
		return;
	FVector right = up.Rotation().Quaternion().GetAxisY();
	FVector forward = FVector::CrossProduct(right, up);
	FVector::CreateOrthonormalBasis(forward, right, up);
	FVector hitPoint = MyStructRef.SurfacePoint + up * 0.01;
	if (showAxis)
	{
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.TrackedComponent.Get(), hitPoint, hitPoint + up * radius, (radius * 0.25), FColor::Blue, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.TrackedComponent.Get(), hitPoint, hitPoint + forward * (radius * 0.5), (radius * 0.25), FColor::Red, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.TrackedComponent.Get(), hitPoint, hitPoint + right * (radius * 0.5), (radius * 0.25), FColor::Green, duration, thickness);
	}
	UKismetSystemLibrary::DrawDebugCircle(MyStructRef.TrackedComponent.Get(), hitPoint, radius, 32,
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


FVector UFunctionLibrary::GetKineticEnergy(const FVector velocity, const float mass, const double distanceTraveled)
{
	const FVector momentum = velocity * mass;
	const FVector kinematicEnergy = momentum.GetSafeNormal() * (momentum.SquaredLength() / (2 * mass));
	const FVector force = kinematicEnergy / distanceTraveled;
	return force;
}


FVector UFunctionLibrary::GetSnapOnSurfaceVector(const FVector onShapeTargetSnapPoint, const FSurface Surface, const FVector SnapAxis)
{
	if (!Surface.TrackedComponent.IsValid())
		return FVector(0);
	const FVector snapDirection = (-SnapAxis).GetSafeNormal();
	const FVector hitPoint = Surface.SurfacePoint;
	const FVector elevationDiff = (hitPoint - onShapeTargetSnapPoint).ProjectOnToNormal(snapDirection);
	FVector snapVector = elevationDiff;
	
	return snapVector;
}

bool UFunctionLibrary::AddOrReplaceCheckVariable(FStatusParameters& statusParam, const FName key, const float value)
{
	if (statusParam.StatusAdditionalCheckVariables.Contains(key))
	{
		statusParam.StatusAdditionalCheckVariables[key] = value;
		return false;
	}

	statusParam.StatusAdditionalCheckVariables.Add(key, value);
	return true;
}

float UFunctionLibrary::GetCheckVariable(FStatusParameters statusParam, const FName key, const float notFoundValue)
{
	if (statusParam.StatusAdditionalCheckVariables.Contains(key))
	{
		return statusParam.StatusAdditionalCheckVariables[key];
	}
	return notFoundValue;
}

void UFunctionLibrary::SetReferentialMovement(FLinearKinematicCondition& linearKinematic, const FVector movement, const float delta, const float acceleration)
{
	const double acc = acceleration >= 0 ? acceleration : 1 / (delta * delta);
	if (acc <= 0)
	{
		linearKinematic.refAcceleration = FVector(0);
		linearKinematic.refVelocity = FVector(0);
		return;
	}
	const double t = FMath::Clamp(acc * delta, 0, 1 / delta);
	const FVector v = movement;
	const FVector v0 = linearKinematic.refVelocity;
	FVector a = FVector(0);
	a.X = (v.X - v0.X) * t;
	a.Y = (v.Y - v0.Y) * t;
	a.Z = (v.Z - v0.Z) * t;
	linearKinematic.refAcceleration = a;
	linearKinematic.refVelocity = a * delta + v0;
}

void UFunctionLibrary::AddCompositeMovement(FLinearKinematicCondition& linearKinematic, const FVector movement, const float acceleration, int index)
{
	if (index < 0)
	{
		bool replaced = false;
		for (int i = 0; i < linearKinematic.CompositeMovements.Num(); i++)
		{
			if (linearKinematic.CompositeMovements[i].W == 0)
			{
				linearKinematic.CompositeMovements[i] = FVector4d(movement.X, movement.Y, movement.Z, acceleration);
				replaced = true;
			}
		}
		if (!replaced)
		{
			linearKinematic.CompositeMovements.Add(FVector4d(movement.X, movement.Y, movement.Z, acceleration));
		}
	}
	else if (linearKinematic.CompositeMovements.IsValidIndex(index))
	{
		linearKinematic.CompositeMovements[index] = FVector4d(movement.X, movement.Y, movement.Z, acceleration);
	}
	else
	{
		for (int i =linearKinematic. CompositeMovements.Num(); i <= index; i++)
		{
			if (i == index)
				linearKinematic.CompositeMovements.Add(FVector4d(movement.X, movement.Y, movement.Z, acceleration));
			else
				linearKinematic.CompositeMovements.Add(FVector4d(0, 0, 0, 0));
		}
	}
}

bool UFunctionLibrary::RemoveCompositeMovement(FLinearKinematicCondition& linearKinematic, int index)
{
	if (linearKinematic.CompositeMovements.IsValidIndex(index))
	{
		linearKinematic.CompositeMovements.RemoveAt(index);
		return true;
	}
	else
		return false;
}

FVector UFunctionLibrary::GetRelativeVelocity(FKinematicComponents kinematicComponent, const float deltaTime, ECollisionResponse channelFilter)
{
	const FVector refVelocity = GetAverageSurfaceVelocityAt(kinematicComponent, kinematicComponent.LinearKinematic.Position, deltaTime, channelFilter);
	return kinematicComponent.LinearKinematic.Velocity - refVelocity;
}

void UFunctionLibrary::ApplyForceOnSurfaces(FKinematicComponents& kinematicComponent, const FVector point, const FVector force, bool reactionForce, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return;
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return;

	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(kinematicComponent.SurfacesInContact[i].SurfacePhysicProperties.Z) != channelFilter)
			continue;
		kinematicComponent.SurfacesInContact[i].ApplyForceAtOnSurface(point, force, reactionForce);
	}
}

FVector UFunctionLibrary::GetVelocityFromReaction(FKinematicComponents kinematicComponent, const FVector velocity, const bool useImpactNormal, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return velocity;
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return velocity;

	FVector subSequentVelocity = velocity;
	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(kinematicComponent.SurfacesInContact[i].SurfacePhysicProperties.Z) != channelFilter)
			continue;
		subSequentVelocity = kinematicComponent.SurfacesInContact[i].GetVelocityAlongNormal(subSequentVelocity, useImpactNormal, true);
	}

	return subSequentVelocity;
}

FVector UFunctionLibrary::GetAverageSurfaceVelocityAt(FKinematicComponents kinematicComponent, const FVector point, const float deltaTime, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return FVector(0);
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return FVector(0);

	FVector cumulated = FVector(0);
	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		const auto surface = kinematicComponent.SurfacesInContact[i];
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != channelFilter)
			continue;
		const FVector surVel = surface.GetVelocityAt(point, deltaTime);
		if (cumulated.IsZero())
		{
			cumulated = surVel;
			continue;
		}
		const FVector projected = surVel.ProjectOnToNormal(cumulated.GetSafeNormal());
		const FVector planed = FVector::VectorPlaneProject(surVel, cumulated.GetSafeNormal());
		if ((projected | cumulated) > 0)
		{
			if (projected.SquaredLength() > cumulated.SquaredLength())
				cumulated = projected;
		}
		else
		{
			cumulated += projected;
		}
		cumulated += planed;
	}

	return cumulated;
}

FVector UFunctionLibrary::GetAverageSurfaceAngularSpeed(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return FVector(0);
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return FVector(0);

	FVector cumulated = FVector(0);
	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		const auto surface = kinematicComponent.SurfacesInContact[i];
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != channelFilter)
			continue;
		const FVector surRotVel = surface.AngularVelocity;
		FVector axis = surRotVel;
		if (!axis.Normalize())
			continue;
		const double angle = surRotVel.Length();
		cumulated += (axis * angle);
	}

	return cumulated;
}

FVector UFunctionLibrary::GetMaxSurfacePhysicProperties(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return FVector(0);
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return FVector(0);

	float maxFriction = 0;
	float maxBounce = 0;
	ECollisionResponse blockiestResponse = ECR_Ignore;

	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		const auto surface = kinematicComponent.SurfacesInContact[i];
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != channelFilter)
			continue;
		if (surface.SurfacePhysicProperties.X > maxFriction)
			maxFriction = surface.SurfacePhysicProperties.X;
		if (surface.SurfacePhysicProperties.Y > maxBounce)
			maxBounce = surface.SurfacePhysicProperties.Y;
		if (static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) > blockiestResponse && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != ECR_MAX)
			blockiestResponse = static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z);
	}

	return FVector(maxFriction, maxBounce, blockiestResponse);
}

bool UFunctionLibrary::IsValidSurfaces(FKinematicComponents kinematicComponent, ECollisionResponse channelFilter)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return false;
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return false;

	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		const auto surface = kinematicComponent.SurfacesInContact[i];
		if (channelFilter != ECR_MAX && static_cast<ECollisionResponse>(surface.SurfacePhysicProperties.Z) != channelFilter)
			continue;
		if (surface.TrackedComponent.IsValid())
			return true;
	}

	return false;
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

double UFunctionLibrary::GetHitComponentMass(FHitResult hit)
{
	if (hit.Component.IsValid())
	{
		const UModularControllerComponent* otherModularComponent = nullptr;
		const auto component = hit.GetActor()->GetComponentByClass<UModularControllerComponent>();
		if (component != nullptr)
		{
			otherModularComponent = Cast<UModularControllerComponent>(component);
		}

		if (hit.Component->IsSimulatingPhysics())
		{
			return hit.Component->GetMass();
		}
		else if (otherModularComponent)
		{
			return otherModularComponent->GetMass();
		}
	}

	return TNumericLimits<double>().Max();
}
