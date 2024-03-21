// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/JumpActionBase.h"
#include <Kismet/KismetMathLibrary.h>
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"






#pragma region Check

/// <summary>
/// Check for a jump input or jump state
/// </summary>
/// <param name="controller"></param>
/// <returns></returns>
bool UJumpActionBase::CheckJump(const FKinematicInfos& inDatas, const FVector moveInput, UInputEntryPool* inputs, const float inDelta, UModularControllerComponent* controller)
{
	FVector currentPosition = inDatas.InitialTransform.GetLocation();
	FQuat currentRotation = inDatas.InitialTransform.GetRotation();
	FVector gravityDir = inDatas.Gravity.GetSafeNormal();

	FHitResult ceilHitRes;
	if (controller->ComponentTraceCastSingle(ceilHitRes, currentPosition - gravityDir, -gravityDir * MaxJumpHeight, currentRotation))
	{
		if ((ceilHitRes.Location - currentPosition).Length() < MinJumpHeight)
		{
			return false;
		}
	}

	if (!inputs)
		return false;

	if (inputs->ReadInput(JumpInputCommand, bDebugAction, controller).Phase == EInputEntryPhase::InputEntryPhase_Pressed)
	{
		inputs->ConsumeInput(JumpInputCommand, bDebugAction, controller);
		return true;
	}

	return false;
}

#pragma endregion

#pragma region Jump


/// <summary>
/// Execute a jump Move
/// </summary>
/// <param name="controller"></param>
/// <returns></returns>
FVector UJumpActionBase::Jump(const FKinematicInfos inDatas, FVector moveInput, const FVelocity momentum, const float inDelta, FVector customJumpLocation)
{
	//Get the maximum jump height
	FVector currentPosition = inDatas.InitialTransform.GetLocation();
	FQuat currentRotation = inDatas.InitialTransform.GetRotation();
	FVector gravityDir = inDatas.Gravity.GetSafeNormal();
	float gravityAcc = inDatas.Gravity.Length();
	float jumpHeight = MaxJumpHeight;

	//Get the forward vector
	FVector forwardVector = UseSurfaceNormalOnNoDirection ? _jumpSurfaceNormal : FVector(0);
	FVector inputVector = moveInput;
	if (inputVector.Length() > 0)
	{
		inputVector.Normalize();
		forwardVector = inputVector;
	}

	//Specified jump location
	float jumpLocationDist = MaxJumpDistance;
	if (!customJumpLocation.ContainsNaN())
	{
		auto locationVector = (customJumpLocation - currentPosition);
		jumpHeight += locationVector.ProjectOnToNormal(gravityDir).Length();
		forwardVector = FVector::VectorPlaneProject(locationVector, gravityDir);
		jumpLocationDist = forwardVector.Length();
		forwardVector.Normalize();
	}

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Custom Jump Location: Location(%s)  Distance(%f), Height(%f)"), *GetDescriptionName().ToString(), *customJumpLocation.ToCompactString(), jumpLocationDist, jumpHeight), true, true, FColor::Black, 10, "customJumpLocation");
	}

	//Get planar movement
	float n_Height = jumpHeight / 1;
	FVector dir = FVector::VectorPlaneProject((currentPosition + forwardVector * jumpLocationDist) - currentPosition, gravityDir);
	float X_distance = dir.Length() / 1;
	dir.Normalize();
	FVector destHeight = FVector(0);// FVector::VectorPlaneProject(point - _currentLocation, dir);
	float hSign = FMath::Sign(FVector::DotProduct(gravityDir, destHeight.GetSafeNormal()));
	float h = (destHeight.Length() / 100) * hSign;
	float Voy = FMath::Sqrt(FMath::Clamp(2 * gravityAcc * (n_Height - h), 0, TNumericLimits<float>().Max()));
	float vox_num = X_distance * gravityAcc;
	float vox_den = Voy + FMath::Sqrt(FMath::Pow(Voy, 2) + (2 * gravityAcc * FMath::Clamp(h, 0, FMath::Abs(h))));
	float Vox = vox_num / vox_den;
	float fTime = Voy / gravityAcc;
	float pTime = vox_den / gravityAcc;

	//Momentum splitting
	FVector vertMomentum = (momentum.InstantLinearVelocity / inDelta).ProjectOnToNormal(gravityDir);
	FVector horiMomentum = FVector::VectorPlaneProject(momentum.InstantLinearVelocity / inDelta, gravityDir);

	if (momentum.ConstantLinearVelocity.Length() > 0)
	{
		vertMomentum = momentum.ConstantLinearVelocity.ProjectOnToNormal(gravityDir);
		FVector planedMomentum = FVector::VectorPlaneProject(momentum.ConstantLinearVelocity, gravityDir);
		horiMomentum += planedMomentum;// : dir * FMath::Max(planedMomentum.Length() * FMath::Clamp(FVector::DotProduct(dir, planedMomentum), 0, 1), Vox);
	}

	FVector jumpForce = (-gravityDir * Voy) + (UseMomentum ? horiMomentum : (dir * Vox * 1));

	return jumpForce;
}


#pragma endregion

#pragma region Functions


bool UJumpActionBase::CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
	return CheckJump(inDatas, moveInput, inputs, inDelta, controller);
}



FVelocity UJumpActionBase::OnActionProcessAnticipationPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = fromVelocity;
	move.InstantLinearVelocity = FVector(0);
	return move;
}

FVelocity UJumpActionBase::OnActionProcessActivePhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = fromVelocity;
	move.InstantLinearVelocity = FVector(0);	

	if (_jumped)
	{
		return move;
	}

	//Handle rotation
	{
		const FVector up = -inDatas.Gravity.GetSafeNormal();
		const FVector inputVector = moveInput;
		FVector fwd = FVector::VectorPlaneProject(inputVector.GetSafeNormal(), up);

		if (fwd.Normalize())
		{
			const FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
			const FQuat rotation = fwdRot;
			move.Rotation = rotation;
		}
	}

	const FVector jumpLocation = IsSimulated() ? FVector(NAN) :
		(JumpLocationInput.IsNone() ? FVector(NAN) : controller->ReadAxisInput(JumpLocationInput, true, bDebugAction, this));
	if (!IsSimulated() && controller) 
	{
		_startMomentum.InstantLinearVelocity = controller->GetCurrentSurface().GetSurfaceLinearVelocity();
	}
	const auto jumpForce = Jump(inDatas, moveInput, _startMomentum, inDelta, jumpLocation);
	move.ConstantLinearVelocity = jumpForce;

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Propulsion Vector: (%s); Velocity: (%s)"), *GetDescriptionName().ToString(), *jumpForce.ToCompactString(), *move.ConstantLinearVelocity.ToCompactString()), true, true, FColor::Yellow, 10, "");
	}

	//instant rotation if custom location
	if (!jumpLocation.ContainsNaN())
	{
		const FVector up = -inDatas.Gravity.GetSafeNormal();
		FVector fwd = FVector::VectorPlaneProject(jumpForce, up);
		if (fwd.Normalize())
		{
			const FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
			move.Rotation = fwdRot;
		}
	}

	if (!IsSimulated())
	{
		if (inDatas.bUsePhysic && controller->GetCurrentSurface().GetSurfacePrimitive() != nullptr)
		{
			//Push Objects
			if (controller->GetCurrentSurface().GetSurfacePrimitive()->IsSimulatingPhysics())
			{
				controller->GetCurrentSurface().GetSurfacePrimitive()->AddImpulseAtLocation(-jumpForce * inDatas.GetMass() * inDelta, controller->GetCurrentSurface().GetHitResult().ImpactPoint, controller->GetCurrentSurface().GetHitResult().BoneName);
			}
		}
	}

	_jumped = true;

	return move;
}

FVelocity UJumpActionBase::OnActionProcessRecoveryPhase_Implementation(FStatusParameters controllerStatusParam, FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = fromVelocity;
	move.InstantLinearVelocity = FVector(0);
	_jumped = false;
	return move;
}


void UJumpActionBase::OnAnimationEnded(UAnimMontage* Montage, bool bInterrupted)
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



void UJumpActionBase::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
	if (!IsSimulated() && controller)
	{
		//Bind montage neds call back
		if (bUseMontageDuration)
			_EndDelegate.BindUObject(this, &UJumpActionBase::OnAnimationEnded);

		//Play montage
		float montageDuration = 0;
		if (bMontageShouldBePlayerOnStateAnimGraph)
		{
			if (const auto currentState = controller->GetCurrentControllerState())
				montageDuration = controller->PlayAnimationMontageOnState_Internal(JumpMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration, _EndDelegate);
		}
		else
		{
			montageDuration = controller->PlayAnimationMontage_Internal(JumpMontage, -1, bUseMontageDuration, _EndDelegate);
		}


		if (bDebugAction)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Montage duration: %f"), *GetDescriptionName().ToString(), montageDuration), true, true, FColor::Emerald, 5, "JumpMontageDuration");
		}

		if (bUseMontageDuration && montageDuration > 0)
		{
			RemapDuration(montageDuration);
		}
	}

	_jumped = false;
	_startMomentum = inDatas.InitialVelocities;
}


void UJumpActionBase::SaveActionSnapShot_Internal()
{
	_jumped_saved = _jumped;
	_jumpSurfaceNormal_saved = _jumpSurfaceNormal;
	_startMomentum_saved = _startMomentum;
}

void UJumpActionBase::RestoreActionFromSnapShot_Internal()
{
	_jumped = _jumped_saved;
	_jumpSurfaceNormal = _jumpSurfaceNormal_saved;
	_startMomentum = _startMomentum_saved;
}


void UJumpActionBase::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, FStatusParameters controllerStatusParam, FStatusParameters& currentStatus, const float inDelta)
{
}


void UJumpActionBase::OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{
	FName stateName = newState != nullptr ? newState->GetDescriptionName() : "";
	if (CompatibleStates.Contains(stateName))
	{
	}
}


#pragma endregion