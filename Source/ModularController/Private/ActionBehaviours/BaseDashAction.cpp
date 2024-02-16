// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/BaseDashAction.h"




UBaseDashAction::UBaseDashAction()
{
	if (Duration <= DashPropulsionDelay)
		Duration = DashPropulsionDelay;
}

bool UBaseDashAction::CheckDash(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs,
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
		if (actions == this)
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


void UBaseDashAction::OnPropulsionOccured_Implementation()
{
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int UBaseDashAction::GetPriority_Implementation()
{
	return BehaviourPriority;
}

FName UBaseDashAction::GetDescriptionName_Implementation()
{
	return BehaviourName;
}


//*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool UBaseDashAction::CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta)
{
	return CheckDash(inDatas, moveInput, inputs, inDelta, controller);
}

FVelocity UBaseDashAction::OnActionProcess_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = inDatas.InitialVelocities;
	move.InstantLinearVelocity = fromVelocity.InstantLinearVelocity;
	bool dashJustStarted = false;

	if (_dashDelayTimer >= 0)
	{
		_dashDelayTimer -= inDelta;
		if (_dashDelayTimer > 0)
		{
			move.Rotation = FQuat::Slerp(move.Rotation, _initialRot, FMath::Clamp(inDelta * 50, 0, 1));
			move.ConstantLinearVelocity = UStructExtensions::AccelerateTo(move.ConstantLinearVelocity, FVector(0), 50, inDelta);
			return move;
		}
		dashJustStarted = true;
	}

	//Handle rotation
	{
		move.Rotation = _initialRot;
	}

	//Propulsion
	if (dashJustStarted)
	{
		_propulsionLocation = inDatas.InitialTransform.GetLocation();
		OnPropulsionOccured();

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
	}

	//Movement
	const FVector initialLocation = inDatas.InitialTransform.GetLocation();
	const FVector nextLocation = FMath::Lerp(initialLocation, _dashToLocation, inDelta * (1 / Duration));
	move.ConstantLinearVelocity = (nextLocation - initialLocation) / inDelta;

	if (bDebugAction)
	{
		UKismetSystemLibrary::DrawDebugPoint(this, _propulsionLocation, 500, FColor::Green, 5);
		UKismetSystemLibrary::DrawDebugPoint(this, _dashToLocation, 500, FColor::Red, 5);
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Dash Speed: (%f); Location: (%s), Distance: (%f)"), *GetDescriptionName().ToString(), move.ConstantLinearVelocity.Length(), *_dashToLocation.ToCompactString(), (_dashToLocation - _propulsionLocation).Length()), true, true, FColor::Yellow, 10, "DashProcess");
	}

	return move;
}

void UBaseDashAction::OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{
	const FName stateName = newState != nullptr ? newState->GetDescriptionName() : "";
	if (ActionCompatibilityMode == OnCompatibleStateOnly || ActionCompatibilityMode == OnBothCompatiblesStateAndAction)
	{
		if (!CompatibleStates.Contains(stateName))
		{
			_remainingActivationTimer = 0;
		}
	}
}

void UBaseDashAction::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	Super::OnActionEnds_Implementation(inDatas, moveInput, controller, inDelta);
}

void UBaseDashAction::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVector closestDir = inDatas.InitialTransform.GetRotation().Vector();
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
			_remainingActivationTimer = montageDuration;
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

	_dashDelayTimer = DashPropulsionDelay;
}

void UBaseDashAction::SaveActionSnapShot_Internal()
{
	_dashToLocation_saved = _dashToLocation;
	_propulsionLocation_saved = _propulsionLocation;
	_dashDelayTimer_saved = _dashDelayTimer;
	_initialRot_saved = _initialRot;
}

void UBaseDashAction::RestoreActionFromSnapShot_Internal()
{
	_dashToLocation = _dashToLocation_saved;
	_propulsionLocation = _propulsionLocation_saved;
	_dashDelayTimer = _dashDelayTimer_saved;
	_initialRot = _initialRot_saved;
}
