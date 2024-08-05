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


// Animation Utilities ///////////////////////////////////////////////////////////////////////

void UFunctionLibrary::ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose)
{
	OutPose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(static_cast<double>(Time), bExtractRootMotion);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(OutPose, Curve, Attributes);
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		AnimSequence->GetBonePose(AnimationPoseData, Context);
	}
	else if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		const FAnimTrack& AnimTrack = AnimMontage->SlotAnimTracks[0].AnimTrack;
		AnimTrack.GetAnimationPose(AnimationPoseData, Context);
	}
}

void UFunctionLibrary::ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose)
{
	FCompactPose Pose;
	ExtractLocalSpacePose(Animation, BoneContainer, Time, bExtractRootMotion, Pose);
	OutPose.InitPose(MoveTemp(Pose));
}

FTransform UFunctionLibrary::ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime, TArray<FTransform>* stepArray)
{
	if (stepArray)
		stepArray->Empty();
	if (const UAnimMontage* Anim = Cast<UAnimMontage>(Animation))
	{
		// This is identical to UAnimMontage::ExtractRootMotionFromTrackRange and UAnimCompositeBase::ExtractRootMotionFromTrack but ignoring bEnableRootMotion
		// so we can extract root motion from the montage even if that flag is set to false in the AnimSequence(s)

		FRootMotionMovementParams AccumulatedRootMotionParams;

		if (Anim->SlotAnimTracks.Num() > 0)
		{
			const FAnimTrack& RootMotionAnimTrack = Anim->SlotAnimTracks[0].AnimTrack;

			TArray<FRootMotionExtractionStep> RootMotionExtractionSteps;
			RootMotionAnimTrack.GetRootMotionExtractionStepsForTrackRange(RootMotionExtractionSteps, StartTime, EndTime);

			for (const FRootMotionExtractionStep& CurStep : RootMotionExtractionSteps)
			{
				if (CurStep.AnimSequence)
				{
					if (stepArray)
					{
						FRootMotionMovementParams stepRM;
						stepRM.Accumulate(CurStep.AnimSequence->ExtractRootMotionFromRange(CurStep.StartPosition, CurStep.EndPosition));
						stepArray->Add(stepRM.GetRootMotionTransform());
					}
					AccumulatedRootMotionParams.Accumulate(CurStep.AnimSequence->ExtractRootMotionFromRange(CurStep.StartPosition, CurStep.EndPosition));
				}
			}
		}

		return AccumulatedRootMotionParams.GetRootMotionTransform();
	}

	if (const UAnimSequence* Anim = Cast<UAnimSequence>(Animation))
	{
		return Anim->ExtractRootMotionFromRange(StartTime, EndTime);
	}

	return FTransform::Identity;
}

FTransform UFunctionLibrary::ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time)
{
	if (const UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		if (const FAnimSegment* Segment = AnimMontage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Time))
		{
			if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Segment->GetAnimReference()))
			{
				const float AnimSequenceTime = Segment->ConvertTrackPosToAnimPos(Time);
				return AnimSequence->ExtractRootTrackTransform(AnimSequenceTime, nullptr);
			}
		}
	}
	else if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Animation))
	{
		return AnimSequence->ExtractRootTrackTransform(Time, nullptr);
	}

	return FTransform::Identity;
}

double UFunctionLibrary::GetMontageCurrentWeight(const UAnimInstance* AnimInstance, const UAnimMontage* Montage)
{
	if (!AnimInstance)
		return 0;
	const auto MontageInstance = Montage ? AnimInstance->GetInstanceForMontage(Montage) : AnimInstance->GetActiveMontageInstance();
	if (!MontageInstance)
		return 0;
	return MontageInstance->GetWeight();
}


//-/////////////////////////////////////////////////////////////////////////////////////////////////////////////


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


bool UFunctionLibrary::CollisionShapeEquals(const FCollisionShape shapeA, const FCollisionShape shapeB)
{
	if (shapeA.ShapeType != shapeB.ShapeType)
		return false;
	return shapeA.GetExtent() == shapeB.GetExtent();
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


void UFunctionLibrary::DrawDebugCircleOnHit(const FHitResult MyStructRef, float radius, FLinearColor color, float duration, float thickness, bool showImpactAxis)
{
	if (!MyStructRef.Component.IsValid())
		return;
	FVector up = MyStructRef.Normal;
	FVector axisUp = MyStructRef.ImpactNormal;
	if (!up.Normalize())
		return;
	const FVector hitPoint = MyStructRef.ImpactPoint + up * 0.01;
	FVector right = axisUp.Rotation().Quaternion().GetAxisY();
	FVector forward = FVector::CrossProduct(right, axisUp);
	if (showImpactAxis && axisUp.Normalize())
	{
		FVector::CreateOrthonormalBasis(forward, right, axisUp);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + axisUp * radius, (radius * 0.25), FColor::Blue, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + forward * (radius * 0.5), (radius * 0.25), FColor::Red, duration, thickness);
		UKismetSystemLibrary::DrawDebugArrow(MyStructRef.GetComponent(), hitPoint, hitPoint + right * (radius * 0.5), (radius * 0.25), FColor::Green, duration, thickness);
	}
	right = up.Rotation().Quaternion().GetAxisY();
	forward = FVector::CrossProduct(right, up);
	FVector::CreateOrthonormalBasis(forward, right, up);
	UKismetSystemLibrary::DrawDebugCircle(MyStructRef.GetComponent(), hitPoint, radius, 32,
	                                      color, duration, thickness, right, forward);
}

void UFunctionLibrary::DrawDebugCircleOnSurface(const FSurface MyStructRef, float radius, FLinearColor color, float duration, float thickness, bool showImpactAxis)
{
	FHitResult hit;
	hit.Component = MyStructRef.TrackedComponent;
	hit.Normal = MyStructRef.SurfaceNormal;
	hit.ImpactNormal = MyStructRef.SurfaceImpactNormal;
	hit.ImpactPoint = MyStructRef.SurfacePoint;
	DrawDebugCircleOnHit(hit, radius, color, duration, thickness, showImpactAxis);
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
	// result.AngularKinematic.Orientation = FQuat::Slerp(A.AngularKinematic.Orientation, B.AngularKinematic.Orientation, delta);
	result.AngularKinematic.Orientation = B.AngularKinematic.Orientation;

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

bool UFunctionLibrary::AddOrReplaceCosmeticVariable(FStatusParameters& statusParam, const FName key, const float value)
{
	if (statusParam.StatusCosmeticVariables.Contains(key))
	{
		statusParam.StatusCosmeticVariables[key] = value;
		return false;
	}

	statusParam.StatusCosmeticVariables.Add(key, value);
	return true;
}

bool UFunctionLibrary::AddOrReplaceCosmeticVector(FStatusParameters& statusParam, const FName key, const FVector value)
{
	const bool xRes = AddOrReplaceCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sX"), *key.ToString())), value.X);
	const bool yRes = AddOrReplaceCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sY"), *key.ToString())), value.Y);
	const bool zRes = AddOrReplaceCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sZ"), *key.ToString())), value.Z);
	return xRes | yRes | zRes;
}

FVector UFunctionLibrary::GetCosmeticVector(FStatusParameters statusParam, const FName key, const FVector notFoundValue)
{
	const float xRes = GetCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sX"), *key.ToString())), notFoundValue.X);
	const float yRes = GetCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sY"), *key.ToString())), notFoundValue.Y);
	const float zRes = GetCosmeticVariable(statusParam, FName(FString::Printf(TEXT("%sZ"), *key.ToString())), notFoundValue.Z);

	return FVector(xRes, yRes, zRes);
}

float UFunctionLibrary::GetCosmeticVariable(FStatusParameters statusParam, const FName key, const float notFoundValue)
{
	if (statusParam.StatusCosmeticVariables.Contains(key))
	{
		return statusParam.StatusCosmeticVariables[key];
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
		for (int i = linearKinematic.CompositeMovements.Num(); i <= index; i++)
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
		FVector surVel = kinematicComponent.SurfacesInContact[i].GetVelocityAlongNormal(subSequentVelocity, useImpactNormal, true);
		const FVector projected = surVel.ProjectOnToNormal(subSequentVelocity.GetSafeNormal());
		const FVector planed = FVector::VectorPlaneProject(surVel, subSequentVelocity.GetSafeNormal());
		if ((projected | subSequentVelocity) > 0)
		{
			if (projected.SquaredLength() > subSequentVelocity.SquaredLength())
				subSequentVelocity = projected;
		}
		else
		{
			subSequentVelocity += projected;
		}
		subSequentVelocity += planed;
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
		if (cumulated.IsNearlyZero())
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


TArray<FKinematicPredictionSample> UFunctionLibrary::MakeKinematicsTrajectory(const FKinematicComponents kinematics, const int SampleCount, const float TimeStep, const float maxSpeed,
                                                                              FVector constantAccelerationForce, float maxRotationRate, float maxRotation, float allowMovementMaxAngle)
{
	TArray<FKinematicPredictionSample> _result;
	FKinematicComponents _kinematics = kinematics;
	FQuat initialOrientation = _kinematics.AngularKinematic.Orientation;
	FVector accVector = _kinematics.LinearKinematic.Acceleration - _kinematics.LinearKinematic.refAcceleration;
	_kinematics.LinearKinematic.Velocity -= _kinematics.LinearKinematic.refVelocity;
	_kinematics.LinearKinematic.refAcceleration = FVector(0);
	_kinematics.LinearKinematic.refVelocity = FVector(0);
	_kinematics.AngularKinematic.AngularAcceleration = FVector(0);

	for (int i = 0; i < SampleCount; i++)
	{
		//handle orientation
		FQuat orientation = _kinematics.AngularKinematic.Orientation;
		FQuat diff = orientation * initialOrientation.Inverse();
		FVector axis;
		float radAngle;
		diff.ToAxisAndAngle(axis, radAngle);
		float angle = FMath::RadiansToDegrees(radAngle);

		//Linear part
		_kinematics.LinearKinematic.Acceleration = constantAccelerationForce + (_kinematics.LinearKinematic.Velocity.Length() < maxSpeed ? accVector : FVector(0));
		auto newLinear = _kinematics.LinearKinematic;
		if (UToolsLibrary::IsVectorCone(newLinear.Velocity, orientation.Vector(), allowMovementMaxAngle))
			newLinear = _kinematics.LinearKinematic.GetFinalCondition(TimeStep);

		//Handle Rotation
		float radRotRate = FMath::DegreesToRadians(maxRotationRate / TimeStep);
		_kinematics.AngularKinematic.RotationSpeed = _kinematics.AngularKinematic.RotationSpeed.GetClampedToMaxSize(radRotRate);
		auto newAngular = _kinematics.AngularKinematic;
		if (angle < maxRotation)
		{
			FQuat rotStep = FQuat::Identity;
			newAngular = _kinematics.AngularKinematic.GetFinalCondition(TimeStep, &rotStep);
			newLinear.Velocity = rotStep.RotateVector(newLinear.Velocity);
			accVector = rotStep.RotateVector(accVector);
		}

		//Make Sample
		FKinematicPredictionSample sample = FKinematicPredictionSample();
		sample.LinearKinematic = newLinear;
		sample.AngularKinematic = newAngular;
		sample.RelativeTime = (i + 1) * TimeStep;
		_result.Add(sample);

		//Set and go again
		_kinematics.AngularKinematic = newAngular;
		_kinematics.LinearKinematic = newLinear;
	}

	return _result;
}

FActionMotionMontage UFunctionLibrary::GetActionMontageAt(FActionMontageLibrary& structRef, int index, int fallbackIndex, bool tryZeroOnNoFallBack)
{
	FActionMotionMontage actionMontage;
	bool montageSet = false;

	if (structRef.Library.IsValidIndex(index))
	{
		actionMontage = structRef.Library[index];
	}
	else
	{
		if (!structRef.Library.IsValidIndex(fallbackIndex))
		{
			if (!tryZeroOnNoFallBack)
				return FActionMotionMontage();
			if (!structRef.Library.IsValidIndex(0))
				return FActionMotionMontage();
			actionMontage = structRef.Library[0];
			montageSet = true;
		}
		if (!montageSet)
			actionMontage = structRef.Library[fallbackIndex];
	}

	if (structRef.OverrideMontageSection != NAME_None)
		actionMontage.MontageSection = structRef.OverrideMontageSection;
	if (structRef.bOverrideUseMontageLenght != false)
		actionMontage.bUseMontageLenght = structRef.bOverrideUseMontageLenght;
	if (structRef.bOverrideUseMontageSectionsAsPhases != false)
		actionMontage.bUseMontageSectionsAsPhases = structRef.bOverrideUseMontageSectionsAsPhases;
	if (structRef.bOverridePlayOnState != false)
		actionMontage.bPlayOnState = structRef.bOverridePlayOnState;
	if (structRef.bOverrideStopOnActionEnds != false)
		actionMontage.bStopOnActionEnds = structRef.bOverrideStopOnActionEnds;

	return actionMontage;
}

FActionMotionMontage UFunctionLibrary::GetActionMontageInDirection(FActionMontageLibrary& structRef, ESixAxisDirectionType direction, ESixAxisDirectionType fallbackDirection)
{
	return GetActionMontageAt(structRef, static_cast<int>(direction), static_cast<int>(fallbackDirection), true);
}

double UFunctionLibrary::GetActionLibraryMontageMaxWeight(FActionMontageLibrary& structRef, const UAnimInstance* AnimInstance)
{
	double weight = 0;
	if (structRef.Library.Num() > 0)
	{
		for (int i = 0; i < structRef.Library.Num(); i++)
		{
			double w = GetMontageCurrentWeight(AnimInstance, structRef.Library[i].Montage);
			if (w > weight)
				weight = w;
		}
	}
	return weight;
}


int UFunctionLibrary::GetSurfaceIndexUnderCondition(FKinematicComponents kinematicComponent, std::function<bool(FSurface&)> condition)
{
	if (kinematicComponent.SurfacesInContact.Num() <= 0)
		return -1;
	const TArray<bool> surfaceCombination = UToolsLibrary::FlagToBoolArray(kinematicComponent.SurfaceBinaryFlag);
	if (surfaceCombination.Num() <= 0)
		return -1;

	for (int i = 0; i < kinematicComponent.SurfacesInContact.Num(); i++)
	{
		if (!surfaceCombination.IsValidIndex(i) || !surfaceCombination[i])
			continue;
		auto surface = kinematicComponent.SurfacesInContact[i];
		if (!surface.TrackedComponent.IsValid())
			continue;
		if (condition(surface))
			return i;
	}

	return -1;
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
