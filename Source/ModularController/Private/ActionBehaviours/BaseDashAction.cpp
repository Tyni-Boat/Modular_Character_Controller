// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/BaseDashAction.h"

#include "FunctionLibrary.h"
#include "ToolsLibrary.h"


bool UBaseDashAction::CheckDash(UModularControllerComponent* controller) const
{
	if (!controller)
		return false;

	if (controller)
	{
		const auto actions = controller->GetCurrentControllerAction();
		if (actions == this && !bCanTransitionToSelf)
		{
			return false;
		}
	}

	return true;
}


FVector UBaseDashAction::GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, ESixAxisDirectionType& directionEnum) const
{
	directionEnum = ESixAxisDirectionType::Forward;
	if (!desiredDir.Normalize())
		return bodyTransform.GetRotation().GetForwardVector();
	const FVector desiredDirection = FVector::VectorPlaneProject(desiredDir, bodyTransform.GetRotation().GetUpVector());
	const float fwdDot = FVector::DotProduct(desiredDirection, bodyTransform.GetRotation().GetForwardVector());
	const float rhtDot = FVector::DotProduct(desiredDirection, bodyTransform.GetRotation().GetRightVector());
	if (FMath::Abs(fwdDot) > 0.5)
	{
		directionEnum = fwdDot > 0 ? ESixAxisDirectionType::Forward : ESixAxisDirectionType::Backward;
		return fwdDot > 0 ? bodyTransform.GetRotation().GetForwardVector() : -bodyTransform.GetRotation().GetForwardVector();
	}
	else
	{
		directionEnum = rhtDot > 0 ? ESixAxisDirectionType::Right : ESixAxisDirectionType::Left;
		return rhtDot > 0 ? bodyTransform.GetRotation().GetRightVector() : -bodyTransform.GetRotation().GetRightVector();
	}
}


FVector UBaseDashAction::GetFourDirectionnalVectorFromIndex(FTransform bodyTransform, const ESixAxisDirectionType directionEnum) const
{
	if (directionEnum == ESixAxisDirectionType::Forward || directionEnum == ESixAxisDirectionType::Backward)
		return directionEnum == ESixAxisDirectionType::Forward ? bodyTransform.GetRotation().GetForwardVector() : -bodyTransform.GetRotation().GetForwardVector();
	if (directionEnum == ESixAxisDirectionType::Left || directionEnum == ESixAxisDirectionType::Right)
		return directionEnum == ESixAxisDirectionType::Right ? bodyTransform.GetRotation().GetRightVector() : -bodyTransform.GetRotation().GetRightVector();
	return FVector(0);
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerCheckResult UBaseDashAction::CheckAction_Implementation(UModularControllerComponent* controller,
                                                                   const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const
{
	if (!controller)
		return FControllerCheckResult(false, startingConditions);
	const bool dashInput = controller->ReadButtonInput(DashInputCommand);
	const bool canDash = CheckDash(controller);
	return FControllerCheckResult(dashInput && canDash, startingConditions);
}

FVector4 UBaseDashAction::OnActionBegins_Implementation(UModularControllerComponent* controller,
                                                        const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	FVector4 timings = FVector4(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration, 0);
	const FQuat currentOrientation = controller->GetRotation();
	FVector closestDir = currentOrientation.Vector();
	FVector moveDirection = moveInput.Length() > 0 ? moveInput.GetSafeNormal() : closestDir.GetSafeNormal();
	const FVector currentLocation = startingConditions.LinearKinematic.Position;


	//Select montage
	ESixAxisDirectionType direction = ESixAxisDirectionType::Forward;

	if (bUseFourDirectionnalDash)
	{
		GetFourDirectionnalVector(FTransform(currentOrientation, currentLocation), moveDirection, direction);
	}

	timings.W = static_cast<int>(direction);
	return timings;
}

void UBaseDashAction::OnActionEnds_Implementation(UModularControllerComponent* controller,
                                                  const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}

FControllerStatus UBaseDashAction::OnActionProcessAnticipationPhase_Implementation(
	UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	const int surfaceIndex = indexes.Num() > 0 ? indexes[0] : -1;
	const auto surface = result.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex) ? result.Kinematics.SurfacesInContact[surfaceIndex] : FSurface();

	//Root motion
	if (controller)
	{
		const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), controller->GetActionCurrentMotionMontage(this).Montage);
		controller->ReadRootMotion(result.Kinematics, FVector(0), RootMotionMode, surface.SurfacePhysicProperties.X, RMWeight);
		const float normalizedTime = actionInfos.GetNormalizedTime(EActionPhase::Anticipation);
		const float trueTime = normalizedTime * actionInfos._startingDurations.X;

		const FQuat currentOrientation = controller->GetRotation();
		const FTransform compTransform = FTransform(currentOrientation, result.Kinematics.LinearKinematic.Position);
		FVector moveDirection = startingConditions.MoveInput.Length() > 0 ? startingConditions.MoveInput.GetSafeNormal() : currentOrientation.Vector();

		if (trueTime <= delta)
		{
			ESixAxisDirectionType direction = ESixAxisDirectionType::Forward;
			if (bUseFourDirectionnalDash)
			{
				GetFourDirectionnalVector(compTransform, moveDirection, direction);
				FVector dir = GetFourDirectionnalVectorFromIndex(compTransform, direction);

				//Handle rotation
				FQuat currentRot = dir.ToOrientationQuat();
				FQuat targetRot = moveDirection.ToOrientationQuat();
				targetRot.EnforceShortestArcWith(currentRot);
				FQuat diff = currentRot.Inverse() * targetRot;
				FVector axis;
				float angle;
				diff.ToAxisAndAngle(axis, angle);
				axis = axis.ProjectOnToNormal(controller->GetGravityDirection());
				axis.Normalize();
				FQuat headingRot = result.Kinematics.AngularKinematic.Orientation * FQuat(axis, angle);
				result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, headingRot.Vector(), TNumericLimits<float>::Max(), delta);
			}
			UFunctionLibrary::AddOrReplaceCosmeticVariable(result.StatusParams, "DashDir", static_cast<float>(direction));
		}
		else
		{
			//Handle rotation
			result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, currentOrientation.Vector(), 500, delta);
		}
	}

	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller,
                                                                             const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	if (!controller)
		return result;
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	const int surfaceIndex = indexes.Num() > 0 ? indexes[0] : -1;
	const auto surface = result.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex) ? result.Kinematics.SurfacesInContact[surfaceIndex] : FSurface();
	const float surfaceAngle = surface.TrackedComponent.IsValid()
		                           ? FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(surface.SurfaceImpactNormal, -controller->GetGravityDirection())))
		                           : -1;

	const FQuat currentOrientation = controller->GetRotation();
	const FTransform compTransform = FTransform(currentOrientation, result.Kinematics.LinearKinematic.Position);
	FVector moveDirection = startingConditions.MoveInput.Length() > 0 ? startingConditions.MoveInput.GetSafeNormal() : currentOrientation.Vector();

	const float normalizedTime = actionInfos.GetNormalizedTime(EActionPhase::Active);
	const float trueTime = normalizedTime * actionInfos._startingDurations.Y;

	float directionIndex = 1;

	//First enter
	if (trueTime <= delta)
	{
		ESixAxisDirectionType direction = ESixAxisDirectionType::Forward;
		if (bUseFourDirectionnalDash)
		{
			GetFourDirectionnalVector(compTransform, moveDirection, direction);
			FVector dir = GetFourDirectionnalVectorFromIndex(compTransform, direction);

			//Handle rotation
			FQuat currentRot = dir.ToOrientationQuat();
			FQuat targetRot = moveDirection.ToOrientationQuat();
			targetRot.EnforceShortestArcWith(currentRot);
			FQuat diff = currentRot.Inverse() * targetRot;
			FVector axis;
			float angle;
			diff.ToAxisAndAngle(axis, angle);
			axis = axis.ProjectOnToNormal(controller->GetGravityDirection());
			axis.Normalize();
			FQuat headingRot = result.Kinematics.AngularKinematic.Orientation * FQuat(axis, angle);
			result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, headingRot.Vector(), TNumericLimits<float>::Max(), delta);
		}
		UFunctionLibrary::AddOrReplaceCosmeticVariable(result.StatusParams, "DashDir", static_cast<float>(direction));
	}
	else
	{
		//Handle rotation
		result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, currentOrientation.Vector(), 500, delta);
	}

	directionIndex = UFunctionLibrary::GetCosmeticVariable(result.StatusParams, "DashDir", 1);
	FVector dashVector = GetFourDirectionnalVectorFromIndex(compTransform, static_cast<ESixAxisDirectionType>(directionIndex));

	//Movement
	const FVector moveVec = dashVector.GetSafeNormal() * DashSpeed * FAlphaBlend::AlphaToBlendOption(1 - normalizedTime, DashCurve.GetBlendOption(), DashCurve.GetCustomCurve());

	//Root motion
	if (controller && surfaceAngle < MaxSurfaceAngle)
	{
		const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), controller->GetActionCurrentMotionMontage(this).Montage);
		controller->ReadRootMotion(result.Kinematics, moveVec, RootMotionMode, surface.SurfacePhysicProperties.X, RMWeight);
	}

	//Check if not anymore on surface
	if (!controller->CheckActionCompatibility(this, result.StatusParams.StateIndex, result.StatusParams.ActionIndex))
		actionInfos.SkipTimeToPhase(EActionPhase::Recovery);

	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
                                                                               const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	const int surfaceIndex = indexes.Num() > 0 ? indexes[0] : -1;
	const auto surface = result.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex) ? result.Kinematics.SurfacesInContact[surfaceIndex] : FSurface();
	const float normalizedTime = actionInfos.GetNormalizedTime(EActionPhase::Recovery);

	//Handle rotation
	{
		const FVector direction = FQuat::Slerp(controller->GetRotation(), result.Kinematics.AngularKinematic.Orientation, normalizedTime).Vector();
		result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, direction, 100, delta);
	}

	//Root motion
	if (controller)
	{
		const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), controller->GetActionCurrentMotionMontage(this).Montage);
		const FVector move = FMath::Lerp(result.Kinematics.LinearKinematic.Velocity, FVector(0), 2 * delta);
		controller->ReadRootMotion(result.Kinematics, move,
		                           (controller->CheckActionCompatibility(this, result.StatusParams.StateIndex, result.StatusParams.ActionIndex)
			                            ? RootMotionMode
			                            : ERootMotionType::NoRootMotion), surface.SurfacePhysicProperties.X, RMWeight);
	}

	return result;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
