// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/BaseDashAction.h"

#include "FunctionLibrary.h"


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
	return FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
	//FVector closestDir = startingConditions.AngularKinematic.Orientation.Vector();
	//FVector moveDirection = moveInput.Length() > 0 ? moveInput.GetSafeNormal() : closestDir.GetSafeNormal();
	//const FVector currentLocation = startingConditions.LinearKinematic.Position;
	//const FQuat currentOrientation = startingConditions.AngularKinematic.Orientation;

	//_propulsionVector = moveDirection * (DashDistance > 0? DashDistance : 1);
	//if (bTurnTowardDashDirection)
	//	closestDir = moveDirection;
	//if (!DashLocationInput.IsNone() && controller)
	//{
	//	_propulsionVector = controller->ReadAxisInput(DashLocationInput) - currentLocation;
	//	moveDirection = _propulsionVector.GetSafeNormal();
	//}

	////Select montage
	//auto selectedMontage = FwdDashMontage;
	//if (!bTurnTowardDashDirection && bUseFourDirectionnalDash && moveDirection.Length() > 0)
	//{
	//	int directionID = 0;
	//	GetFourDirectionnalVector(FTransform(currentOrientation, currentLocation), moveDirection, directionID);
	//	switch (directionID)
	//	{
	//	default:
	//		break;
	//	case 2:
	//		selectedMontage = BackDashMontage;
	//		break;
	//	case 3:
	//		selectedMontage = LeftDashMontage;
	//		break;
	//	case 4:
	//		selectedMontage = RightDashMontage;
	//		break;
	//	}
	//}

	////Handle look direction
	//_lookDirection = closestDir.GetSafeNormal();

	//if (controller)
	//{
	//	//Bind montage needs call back
	//	if (bUseMontageDuration)
	//		_EndDelegate.BindUObject(this, &UBaseDashAction::OnAnimationEnded);


	//	//Play montage
	//	float montageDuration = 0;
	//	if (bMontageShouldBePlayerOnStateAnimGraph)
	//	{
	//		if (const auto currentState = controller->GetCurrentControllerState())
	//			montageDuration = controller->PlayAnimationMontageOnState_Internal(selectedMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration, _EndDelegate);
	//	}
	//	else
	//	{
	//		montageDuration = controller->PlayAnimationMontage_Internal(selectedMontage, -1, bUseMontageDuration, _EndDelegate);
	//	}


	//	if (bDebugAction)
	//	{
	//		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Montage duration: %f"), *GetDescriptionName().ToString(), montageDuration), true, true, FColor::Emerald, 5, "DashMontageDuration");
	//	}

	//	if (bUseMontageDuration && montageDuration > 0)
	//	{
	//		RemapDuration(montageDuration);
	//	}
	//}
}

void UBaseDashAction::OnActionEnds_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{

}

FControllerStatus UBaseDashAction::OnActionProcessAnticipationPhase_Implementation(
	UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.CustomPhysicProperties = FVector(0, 0, 0);

	if(controller)
	{
		controller->ReadRootMotion(result.Kinematics, RootMotionMode);
	}

	//result.Kinematics.AngularKinematic = FunctionLibrary::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 50, delta);
	//result.Kinematics.LinearKinematic.Velocity = FunctionLibrary::AccelerateTo(result.Kinematics.LinearKinematic.Velocity, FVector(0), 50, delta);
	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;

	//if (controller)
	//{
	//	if (controller->GetCurrentSurface().GetSurfacePrimitive() != nullptr)
	//	{
	//		//Push Objects
	//		if (controller->GetCurrentSurface().GetSurfacePrimitive()->IsSimulatingPhysics())
	//		{
	//			controller->GetCurrentSurface().GetSurfacePrimitive()->AddForceAtLocation(-_propulsionVector * controller->GetMass() * delta, controller->GetCurrentSurface().GetHitResult().ImpactPoint, controller->GetCurrentSurface().GetHitResult().BoneName);
	//		}
	//	}
	//}

	////Handle rotation
	//{
	//	result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 15, delta);
	//}

	////Movement
	//result.Kinematics.LinearKinematic.AddCompositeMovement(_propulsionVector, -1, 0);

	////Root motion
	//if (controller)
	//{
	//	controller->ReadRootMotion(result.Kinematics, RootMotionMode);
	//}

	//if (bDebugAction)
	//{
	//	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Dash Speed: (%f); vector: (%s), Distance: (%f)"), *GetDescriptionName().ToString(), result.Kinematics.LinearKinematic.Velocity.Length(), *_propulsionVector.GetSafeNormal().ToCompactString(), _propulsionVector.Length()), true, true, FColor::Yellow, 10, "DashProcess");
	//}

	//result.CustomPhysicProperties = FVector(0, 0, 0);

	return result;
}

FControllerStatus UBaseDashAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;

	////Handle rotation
	//{
	//	result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, _lookDirection, 15, delta);
	//}

	////Root motion
	//if (controller)
	//{
	//	controller->ReadRootMotion(result.Kinematics, RootMotionMode);
	//}

	//result.Kinematics.LinearKinematic.Velocity = FunctionLibrary::AccelerateTo(result.Kinematics.LinearKinematic.Velocity, FVector(0), 50, delta);
	return result;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

