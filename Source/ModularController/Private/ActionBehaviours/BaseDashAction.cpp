// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/BaseDashAction.h"



bool UBaseDashAction::CheckDash(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus,
	const float inDelta, UModularControllerComponent* controller)
{
	FVector currentPosition = inDatas.InitialTransform.GetLocation();
	FQuat currentRotation = inDatas.InitialTransform.GetRotation();
	FVector gravityDir = inDatas.Gravity.GetSafeNormal();

	if (!inputs)
		return false;

	if (!IsSimulated() && controller)
	{
		const auto actions = controller->GetCurrentControllerAction();
		if (actions == this && !bCanTransitionToSelf)
		{
			return false;
		}
	}

	if (inputs->ReadInput(DashInputCommand, bDebugAction, controller).Phase == EInputEntryPhase::InputEntryPhase_Pressed)
	{
		inputs->ConsumeInput(DashInputCommand, bDebugAction, controller);
		_dashToLocation = inDatas.InitialTransform.GetLocation() + (moveInput.Length() > 0 ? moveInput : inDatas.InitialTransform.GetRotation().GetForwardVector()) * DashDistance;
		if (!DashLocationInput.IsNone())
		{
			const FVector dashLocation = inputs->ConsumeInput(DashLocationInput).Axis;
			_dashToLocation = dashLocation;
		}
		
		controllerStatusParam.ActionsModifiers1 = _dashToLocation;

		currentStatus = controllerStatusParam;
		return true;
	}

	return false;
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


int UBaseDashAction::GetPriority_Implementation()
{
	return ActionPriority;
}

FName UBaseDashAction::GetDescriptionName_Implementation()
{
	return ActionName;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool UBaseDashAction::CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
	return CheckDash(inDatas, moveInput, inputs, controllerStatusParam, currentStatus, inDelta, controller);
}

FVelocity UBaseDashAction::OnActionProcessAnticipationPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = inDatas.InitialVelocities;
	move.InstantLinearVelocity = fromVelocity.InstantLinearVelocity;

	move.Rotation = FQuat::Slerp(move.Rotation, _initialRot, FMath::Clamp(inDelta * 50, 0, 1));
	move.ConstantLinearVelocity = UStructExtensions::AccelerateTo(move.ConstantLinearVelocity, FVector(0), 50, inDelta);
	return move;
}

FVelocity UBaseDashAction::OnActionProcessActivePhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = inDatas.InitialVelocities;
	move.InstantLinearVelocity = fromVelocity.InstantLinearVelocity;

	if (!_dashed)
	{
		_propulsionLocation = inDatas.InitialTransform.GetLocation();

		if (!IsSimulated())
		{
			if (inDatas.bUsePhysic && inDatas.FinalSurface.GetSurfacePrimitive() != nullptr)
			{
				//Push Objects
				if (inDatas.FinalSurface.GetSurfacePrimitive()->IsSimulatingPhysics())
				{
					inDatas.FinalSurface.GetSurfacePrimitive()->AddImpulseAtLocation(-(_dashToLocation - inDatas.InitialTransform.GetLocation()) * inDatas.GetMass(), inDatas.FinalSurface.GetHitResult().ImpactPoint, inDatas.FinalSurface.GetHitResult().BoneName);
				}
			}
		}
		_dashed = true;
	}

	//Handle rotation
	{
		move.Rotation = _initialRot;
	}

	//Movement
	const FVector initialLocation = inDatas.InitialTransform.GetLocation();
	const FVector nextLocation = FMath::Lerp(initialLocation, _dashToLocation, inDelta * (1 / ActivePhaseDuration));
	move.ConstantLinearVelocity = (nextLocation - initialLocation) / inDelta;

	if (bDebugAction)
	{
		UKismetSystemLibrary::DrawDebugPoint(this, _propulsionLocation, 500, FColor::Green, 5);
		UKismetSystemLibrary::DrawDebugPoint(this, _dashToLocation, 500, FColor::Red, 5);
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Dash Speed: (%f); Location: (%s), Distance: (%f)"), *GetDescriptionName().ToString(), move.ConstantLinearVelocity.Length(), *_dashToLocation.ToCompactString(), (_dashToLocation - _propulsionLocation).Length()), true, true, FColor::Yellow, 10, "DashProcess");
	}

	return move;
}

FVelocity UBaseDashAction::OnActionProcessRecoveryPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = inDatas.InitialVelocities;
	move.InstantLinearVelocity = fromVelocity.InstantLinearVelocity;

	move.ConstantLinearVelocity = UStructExtensions::AccelerateTo(move.ConstantLinearVelocity, FVector(0), 50, inDelta);
	return move;
}

void UBaseDashAction::OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{
	const FName stateName = newState != nullptr ? newState->GetDescriptionName() : "";
	if (ActionCompatibilityMode == ActionCompatibilityMode_OnCompatibleStateOnly || ActionCompatibilityMode == ActionCompatibilityMode_OnBothCompatiblesStateAndAction)
	{
		if (!CompatibleStates.Contains(stateName))
		{
			_remainingActivationTimer = 0;
		}
	}
}

void UBaseDashAction::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
	Super::OnActionEnds_Implementation(inDatas, moveInput, controller, controllerStatusParam, currentStatus, inDelta);
}

void UBaseDashAction::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
	FVector closestDir = inDatas.InitialTransform.GetRotation().Vector();
	
	if (controllerStatusParam.ActionsModifiers1.SquaredLength() > 0)
		_dashToLocation = controllerStatusParam.ActionsModifiers1;

	currentStatus = controllerStatusParam;

	const FVector moveDirection = (_dashToLocation - inDatas.InitialTransform.GetLocation()).GetSafeNormal();

	if (!IsSimulated() && controller)
	{
		//Bind montage needs call back
		if (bUseMontageDuration)
			_EndDelegate.BindUObject(this, &UBaseDashAction::OnAnimationEnded);

		//Select montage
		auto selectedMontage = FwdDashMontage;
		if (bUseFourDirectionnalDash && moveDirection.Length() > 0)
		{
			int directionID = 0;
			closestDir = GetFourDirectionnalVector(inDatas.InitialTransform, moveDirection, directionID);
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

	//Handle initial rotation
	const FVector up = -inDatas.Gravity.GetSafeNormal();
	FVector movefwd = FVector::VectorPlaneProject(moveDirection.GetSafeNormal(), up);
	FVector bodyfwd = FVector::VectorPlaneProject(closestDir.GetSafeNormal(), up);
	_initialRot = inDatas.InitialTransform.GetRotation();
	if (movefwd.Normalize() && bodyfwd.Normalize())
	{
		FQuat bodyFwdRot = UKismetMathLibrary::MakeRotationFromAxes(bodyfwd, FVector::CrossProduct(up, bodyfwd), up).Quaternion();
		const FQuat moveFwdRot = UKismetMathLibrary::MakeRotationFromAxes(movefwd, FVector::CrossProduct(up, movefwd), up).Quaternion();
		bodyFwdRot.EnforceShortestArcWith(moveFwdRot);
		const FQuat diff = bodyFwdRot.Inverse() * moveFwdRot;
		_initialRot *= diff;
	}
}

void UBaseDashAction::SaveActionSnapShot_Internal()
{
	_dashToLocation_saved = _dashToLocation;
	_propulsionLocation_saved = _propulsionLocation;
	_dashed_saved = _dashed;
	_initialRot_saved = _initialRot;
}

void UBaseDashAction::RestoreActionFromSnapShot_Internal()
{
	_dashToLocation = _dashToLocation_saved;
	_propulsionLocation = _propulsionLocation_saved;
	_dashed = _dashed_saved;
	_initialRot = _initialRot_saved;
}
