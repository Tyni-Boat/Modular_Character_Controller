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


FVector UBaseDashAction::GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, int& directionIndex) const
{
	directionIndex = 0;
	if (!desiredDir.Normalize())
		return bodyTransform.GetRotation().GetForwardVector();
	const FVector desiredDirection = FVector::VectorPlaneProject(desiredDir, bodyTransform.GetRotation().GetUpVector());
	const float fwdDot = FVector::DotProduct(desiredDirection, bodyTransform.GetRotation().GetForwardVector());
	const float rhtDot = FVector::DotProduct(desiredDirection, bodyTransform.GetRotation().GetRightVector());
	if (FMath::Abs(fwdDot) > 0.5)
	{
		directionIndex = fwdDot > 0 ? 1 : 2;
		return fwdDot > 0 ? bodyTransform.GetRotation().GetForwardVector() : -bodyTransform.GetRotation().GetForwardVector();
	}
	else
	{
		directionIndex = rhtDot > 0 ? 4 : 3;
		return rhtDot > 0 ? bodyTransform.GetRotation().GetRightVector() : -bodyTransform.GetRotation().GetRightVector();
	}
}


FVector UBaseDashAction::GetFourDirectionnalVectorFromIndex(FTransform bodyTransform, const int directionIndex) const
{
	if (directionIndex == 1 || directionIndex == 2)
		return directionIndex == 1 ? bodyTransform.GetRotation().GetForwardVector() : -bodyTransform.GetRotation().GetForwardVector();
	if (directionIndex == 3 || directionIndex == 4)
		return directionIndex == 4 ? bodyTransform.GetRotation().GetRightVector() : -bodyTransform.GetRotation().GetRightVector();
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

FVector UBaseDashAction::OnActionBegins_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	FVector timings = FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
	const FQuat currentOrientation = controller->GetRotation();
	FVector closestDir = currentOrientation.Vector();
	FVector moveDirection = moveInput.Length() > 0 ? moveInput.GetSafeNormal() : closestDir.GetSafeNormal();
	const FVector currentLocation = startingConditions.LinearKinematic.Position;


	//Select montage
	auto selectedMontage = FwdDashMontage;
	int directionID = 0;

	if (bUseFourDirectionnalDash)
	{
		GetFourDirectionnalVector(FTransform(currentOrientation, currentLocation), moveDirection, directionID);
	}

	switch (directionID)
	{
		default:
			selectedMontage = FwdDashMontage;
			break;
		case 2:
			selectedMontage = BackDashMontage;
			break;
		case 3:
			selectedMontage = LeftDashMontage;
			break;
		case 4:
			selectedMontage = RightDashMontage;
			break;
	}

	if (controller)
	{
		//Play montage
		float montageDuration = 0;
		if (bMontageShouldBePlayerOnStateAnimGraph)
		{
			if (const auto currentState = controller->GetCurrentControllerState())
				montageDuration = controller->PlayAnimationMontageOnState_Internal(selectedMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration);
		}
		else
		{
			montageDuration = controller->PlayAnimationMontage_Internal(selectedMontage, -1, bUseMontageDuration);
		}

		if (bUseMontageDuration && montageDuration > 0)
		{
			timings = RemapDuration(montageDuration);
		}
	}

	return timings;
}

void UBaseDashAction::OnActionEnds_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	if (controller)
	{
		controller->StopMontage(FwdDashMontage, bMontageShouldBePlayerOnStateAnimGraph);
		controller->StopMontage(BackDashMontage, bMontageShouldBePlayerOnStateAnimGraph);
		controller->StopMontage(LeftDashMontage, bMontageShouldBePlayerOnStateAnimGraph);
		controller->StopMontage(RightDashMontage, bMontageShouldBePlayerOnStateAnimGraph);
	}
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
		controller->ReadRootMotion(result.Kinematics, FVector(0), RootMotionMode, surface.SurfacePhysicProperties.X);
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

	const FQuat currentOrientation = controller->GetRotation();
	const FTransform compTransform = FTransform(currentOrientation, result.Kinematics.LinearKinematic.Position);
	FVector moveDirection = startingConditions.MoveInput.Length() > 0 ? startingConditions.MoveInput.GetSafeNormal() : currentOrientation.Vector();

	const float normalizedTime = actionInfos.GetNormalizedTime(ActionPhase_Active);
	const float trueTime = normalizedTime * actionInfos._startingDurations.Y;

	float directionIndex = 1;

	//First enter
	if (trueTime <= delta)
	{
		int directionID = 1;
		if (bUseFourDirectionnalDash)
		{
			GetFourDirectionnalVector(compTransform, moveDirection, directionID);
			FVector dir = GetFourDirectionnalVectorFromIndex(compTransform, directionID);

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
		UFunctionLibrary::AddOrReplaceCosmeticVariable(result.StatusParams, "DashDir", directionID);
	}
	else
	{
		//Handle rotation
		result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, currentOrientation.Vector(), 500, delta);
	}

	directionIndex = UFunctionLibrary::GetCosmeticVariable(result.StatusParams, "DashDir", 1);
	FVector dashVector = GetFourDirectionnalVectorFromIndex(compTransform, directionIndex);

	//Movement
	const FVector moveVec = dashVector.GetSafeNormal() * DashSpeed * FAlphaBlend::AlphaToBlendOption(1 - normalizedTime, DashCurve.GetBlendOption(), DashCurve.GetCustomCurve());

	//Root motion
	if (controller)
	{
		controller->ReadRootMotion(result.Kinematics, moveVec, RootMotionMode, surface.SurfacePhysicProperties.X);
	}

	//Check if not anymore on surface
	if (!controller->CheckActionCompatibility(this, result.StatusParams.StateIndex, result.StatusParams.ActionIndex))
		actionInfos.SkipTimeToPhase(ActionPhase_Recovery);

	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	const TArray<int> indexes = UToolsLibrary::BoolToIndexesArray(UToolsLibrary::FlagToBoolArray(result.Kinematics.SurfaceBinaryFlag));
	const int surfaceIndex = indexes.Num() > 0 ? indexes[0] : -1;
	const auto surface = result.Kinematics.SurfacesInContact.IsValidIndex(surfaceIndex) ? result.Kinematics.SurfacesInContact[surfaceIndex] : FSurface();
	const float normalizedTime = actionInfos.GetNormalizedTime(ActionPhase_Recovery);

	//Handle rotation
	{
		const FVector direction = FQuat::Slerp(controller->GetRotation(), result.Kinematics.AngularKinematic.Orientation, normalizedTime).Vector();
		result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, direction, 100, delta);
	}

	//Root motion
	if (controller)
	{
		const FVector move = FMath::Lerp(result.Kinematics.LinearKinematic.Velocity, FVector(0), 2 * delta);
		controller->ReadRootMotion(result.Kinematics, move, (controller->CheckActionCompatibility(this, result.StatusParams.StateIndex, result.StatusParams.ActionIndex) ? RootMotionMode.GetValue() : RootMotionType_No_RootMotion), surface.SurfacePhysicProperties.X);
	}

	return result;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
