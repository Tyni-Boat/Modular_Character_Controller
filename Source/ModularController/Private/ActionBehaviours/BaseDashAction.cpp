// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/BaseDashAction.h"



bool UBaseDashAction::CheckDash(UModularControllerComponent* controller)
{
	if (!controller)
		return false;

	if (!IsSimulated() && controller)
	{
		const auto actions = controller->GetCurrentControllerAction();
		if (actions == this && !bCanTransitionToSelf)
		{
			return false;
		}
	}

	return true;
}



FVector UBaseDashAction::GetFourDirectionnalVector(FTransform bodyTransform, FVector desiredDir, int& directionIndex)
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


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UBaseDashAction::OnAnimationEnded(UAnimMontage* Montage, bool bInterrupted)
{
	_EndDelegate.Unbind();

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Unbond Montage"), *GetDescriptionName().ToString()), true, true, FColor::Red, 5, FName(FString::Printf(TEXT("%s"), *GetDescriptionName().ToString())));
	}

	if (bUseMontageDuration)
	{
		//_remainingActivationTimer = 0;
	}
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerCheckResult UBaseDashAction::CheckAction_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta, bool asLastActiveAction)
{
	if (!controller)
		return FControllerCheckResult(false, startingConditions);
	const bool dashInput = controller->ReadButtonInput(DashInputCommand);
	const bool canDash = CheckDash(controller);
	return FControllerCheckResult(dashInput && canDash, startingConditions);
}

FKinematicComponents UBaseDashAction::OnActionBegins_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	FKinematicComponents result = startingConditions;

	result.AngularKinematic.AngularAcceleration = FVector(0);
	result.AngularKinematic.RotationSpeed = FVector(0);

	FVector closestDir = result.AngularKinematic.Orientation.Vector();
	FVector moveDirection = moveInput.Length() > 0 ? moveInput.GetSafeNormal() : closestDir.GetSafeNormal();
	const FVector currentLocation = result.LinearKinematic.Position;
	const FQuat currentOrientation = result.AngularKinematic.Orientation;

	_propulsionVector = moveDirection * (DashDistance > 0? DashDistance : 1);
	if (bTurnTowardDashDirection)
		closestDir = moveDirection;
	if (!DashLocationInput.IsNone())
	{
		_propulsionVector = controller->ReadAxisInput(DashLocationInput) - currentLocation;
		moveDirection = _propulsionVector.GetSafeNormal();
	}

	//Select montage
	auto selectedMontage = FwdDashMontage;
	if (!bTurnTowardDashDirection && bUseFourDirectionnalDash && moveDirection.Length() > 0)
	{
		int directionID = 0;
		GetFourDirectionnalVector(FTransform(currentOrientation, currentLocation), moveDirection, directionID);
		switch (directionID)
		{
		default:
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
	}

	//Handle look direction
	_lookDirection = closestDir.GetSafeNormal();

	if (!IsSimulated() && controller)
	{
		//Bind montage needs call back
		if (bUseMontageDuration)
			_EndDelegate.BindUObject(this, &UBaseDashAction::OnAnimationEnded);


		//Play montage
		float montageDuration = 0;
		if (bMontageShouldBePlayerOnStateAnimGraph)
		{
			if (const auto currentState = controller->GetCurrentControllerState())
				montageDuration = controller->PlayAnimationMontageOnState_Internal(selectedMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration, _EndDelegate);
		}
		else
		{
			montageDuration = controller->PlayAnimationMontage_Internal(selectedMontage, -1, bUseMontageDuration, _EndDelegate);
		}


		if (bDebugAction)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Montage duration: %f"), *GetDescriptionName().ToString(), montageDuration), true, true, FColor::Emerald, 5, "DashMontageDuration");
		}

		if (bUseMontageDuration && montageDuration > 0)
		{
			RemapDuration(montageDuration);
		}
	}

	return result;
}

FKinematicComponents UBaseDashAction::OnActionEnds_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	return Super::OnActionEnds_Implementation(controller, startingConditions, moveInput, delta);
}

FControllerStatus UBaseDashAction::OnActionProcessAnticipationPhase_Implementation(
	UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;
	result.CustomPhysicProperties = FVector(0, 0, 0);

	if(controller)
	{
		controller->ReadRootMotion(result.Kinematics, RootMotionMode, delta);
	}

	//result.Kinematics.AngularKinematic = UStructExtensions::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 50, delta);
	//result.Kinematics.LinearKinematic.Velocity = UStructExtensions::AccelerateTo(result.Kinematics.LinearKinematic.Velocity, FVector(0), 50, delta);
	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;

	if (!IsSimulated() && controller)
	{
		if (controller->GetCurrentSurface().GetSurfacePrimitive() != nullptr)
		{
			//Push Objects
			if (controller->GetCurrentSurface().GetSurfacePrimitive()->IsSimulatingPhysics())
			{
				controller->GetCurrentSurface().GetSurfacePrimitive()->AddForceAtLocation(-_propulsionVector * controller->GetMass() * delta, controller->GetCurrentSurface().GetHitResult().ImpactPoint, controller->GetCurrentSurface().GetHitResult().BoneName);
			}
		}
	}

	//Handle rotation
	{
		result.Kinematics.AngularKinematic = UStructExtensions::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 15, delta);
	}

	//Movement
	result.Kinematics.LinearKinematic.AddCompositeMovement(_propulsionVector, -1, 0);

	//Root motion
	if (controller)
	{
		controller->ReadRootMotion(result.Kinematics, RootMotionMode, delta);
	}

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Dash Speed: (%f); vector: (%s), Distance: (%f)"), *GetDescriptionName().ToString(), result.Kinematics.LinearKinematic.Velocity.Length(), *_propulsionVector.GetSafeNormal().ToCompactString(), _propulsionVector.Length()), true, true, FColor::Yellow, 10, "DashProcess");
	}

	result.CustomPhysicProperties = FVector(0, 0, 0);

	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;

	//Handle rotation
	{
		result.Kinematics.AngularKinematic = UStructExtensions::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 15, delta);
	}

	//Root motion
	if (controller)
	{
		controller->ReadRootMotion(result.Kinematics, RootMotionMode, delta);
	}

	//result.Kinematics.LinearKinematic.Velocity = UStructExtensions::AccelerateTo(result.Kinematics.LinearKinematic.Velocity, FVector(0), 50, delta);
	return result;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UBaseDashAction::OnControllerStateChanged_Implementation(UModularControllerComponent* onController,
	FName newBehaviourDescName, int newPriority)
{
	if (ActionCompatibilityMode == ActionCompatibilityMode_OnCompatibleStateOnly || ActionCompatibilityMode == ActionCompatibilityMode_OnBothCompatiblesStateAndAction)
	{
		if (!CompatibleStates.Contains(newBehaviourDescName))
		{
			_remainingActivationTimer = 0;
		}
	}
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UBaseDashAction::SaveActionSnapShot_Internal()
{
}

void UBaseDashAction::RestoreActionFromSnapShot_Internal()
{
}
