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
bool UJumpActionBase::CheckJump(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta)
{
	if (IsJUmping())
		return false;

	if (_landChrono > 0)
	{
		_landChrono -= inDelta;
		return false;
	}

	bool isCompatibleState = CompatibleStates.Contains(inDatas._currentStateName);
	if (isCompatibleState && _jumpCount > 0)
		return false;
	if (!isCompatibleState && _jumpCount <= 0)
		return false;

	if (CanJump() && inputs.ReadInput(JumpInputCommand).Phase == EInputEntryPhase::InputEntryPhase_Pressed)
	{
		_jumpSurfaceNormal = FVector::VectorPlaneProject(inDatas.InitialSurface.GetSurfaceNormal(), inDatas.Gravity.GetSafeNormal());
		_jumpForce = Jump(inDatas, inputs, inDelta);
		if (_jumpForce.Length() > 0)
		{
			OnJump(inDatas, _jumpForce);
			_lastJumpVector = _jumpForce;
			_haveSwitchedSurfaceDuringJump = _jumpCount > 1;
			return true;
		}
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
FVector UJumpActionBase::Jump(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, const float inDelta)
{
	//Get the maximum jump height
	//FHitResult ceilHitRes;
	FVector currentPosition = inDatas.InitialTransform.GetLocation();
	FQuat currentRotation = inDatas.InitialTransform.GetRotation();
	FVector gravityDir = inDatas.Gravity.GetSafeNormal();
	float gravityAcc = inDatas.Gravity.Length();
	float jumpHeight = MaxJumpHeight;
	//if (controller->ComponentTraceCastSingle(ceilHitRes, currentPosition - gravityDir, -gravityDir * MaxJumpHeight, currentRotation))
	//{
	//	if (ceilHitRes.Distance < jumpHeight)
	//	{
	//		jumpHeight = ceilHitRes.Distance;
	//	}
	//}

	//Get the forward vector
	FVector forwardVector = UseSurfaceNormalOnNoDirection ? _jumpSurfaceNormal : FVector(0);
	FVector inputVector = inputs.ReadInput(DirectionInputName).Axis;
	if (inputVector.Length() > 0)
	{
		inputVector.Normalize();
		forwardVector = inputVector;
	}

	//Get planar movement
	float n_Height = jumpHeight / 1;
	FVector dir = FVector::VectorPlaneProject((currentPosition + forwardVector * MaxJumpDistance) - currentPosition, gravityDir);
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
	FVector vertMomentum = (inDatas.FinalSurface.GetSurfaceLinearVelocity() / inDelta).ProjectOnToNormal(gravityDir);
	FVector horiMomentum = FVector::VectorPlaneProject(inDatas.FinalSurface.GetSurfaceLinearVelocity() / inDelta, gravityDir);
	
	if (inDatas.GetInitialMomentum().Length() > 0)
	{
		vertMomentum = inDatas.GetInitialMomentum().ProjectOnToNormal(gravityDir);
		FVector planedMomentum = FVector::VectorPlaneProject(inDatas.GetInitialMomentum(), gravityDir);
		horiMomentum += _jumpCount <= 0 ? planedMomentum : dir * FMath::Max(planedMomentum.Length() * FMath::Clamp(FVector::DotProduct(dir, planedMomentum), 0, 1), Vox);
	}

	FVector jumpForce = (-gravityDir * Voy) + (_jumpCount <= 0? vertMomentum : FVector(0)) + (UseMomentum ? horiMomentum : (dir * Vox * 1));
	if (jumpForce.Length() > 0)
	{
		JumpStart();
	}

	return jumpForce;
}


void UJumpActionBase::JumpStart()
{
	if (Montages.IsValidIndex(_jumpCount))
	{
		Montage = Montages[_jumpCount];
	}

	_jumpCount++;
	_jumpChrono = _actionTimer;
	_justJumps = true;
}


void UJumpActionBase::OnJump(const FKinematicInfos& inDatas, FVector jumpForce)
{
	if (inDatas.actor == nullptr)
		return;

	const APawn* pawn = Cast<APawn>(inDatas.actor.Get());
	if (pawn == nullptr)
		return;

	if (pawn->GetLocalRole() != ROLE_Authority || inDatas.FinalSurface.GetSurfacePrimitive() == nullptr)
		return;

	//Push Objects
	if (inDatas.bUsePhysic && inDatas.FinalSurface.GetSurfacePrimitive()->IsSimulatingPhysics())
	{
		inDatas.FinalSurface.GetSurfacePrimitive()->AddImpulseAtLocation(-jumpForce * inDatas.GetMass(), inDatas.FinalSurface.GetHitResult().ImpactPoint, inDatas.FinalSurface.GetHitResult().BoneName);
	}
}


void UJumpActionBase::JumpEnd(FName surfaceName)
{
	if (CompatibleStates.Contains(surfaceName))
	{
		_jumpCount = 0;
		_jumpChrono = 0;
		_landChrono = LandCoolDownTime;
	}
}

#pragma endregion

#pragma region Functions

int UJumpActionBase::GetPriority_Implementation() { return BehaviourPriority; }

FName UJumpActionBase::GetDescriptionName_Implementation() { return BehaviourName; }




void UJumpActionBase::ActionIdle_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
}

bool UJumpActionBase::CheckAction_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	return CheckJump(inDatas, inputs, inDelta);
}

FVelocity UJumpActionBase::OnActionProcess_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	FVelocity move = inDatas.InitialVelocities;
	move.InstantLinearVelocity = FVector(0);
	
	//Handle rotation
	{
		FVector up = -inDatas.Gravity.GetSafeNormal();
		FVector inputVector = inputs.ReadInput(DirectionInputName).Axis;
		FVector fwd = FVector::VectorPlaneProject(inputVector.GetSafeNormal(), up);

		if (fwd.Length() > 1 && fwd.Normalize())
		{
			FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
			FQuat rotation = _justJumps? fwdRot : FQuat::Slerp(inDatas.InitialTransform.GetRotation(), fwdRot, FMath::Clamp(inDelta * TurnTowardDirectionSpeed, 0, 1));
			move.Rotation = rotation;
		}
	}

	if (_jumpChrono > 0)
		_jumpChrono -= inDelta;

	if (_jumpForce.Length() > 0)
	{
		move.ConstantLinearVelocity = _jumpForce;
		_jumpForce += inDatas.Gravity * inDelta;
	}

	if (_justJumps)
		_justJumps = false;

	return move;
}

void UJumpActionBase::OnActionRepeat_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{

}

void UJumpActionBase::OnStateChanged_Implementation(UBaseState* newState, UBaseState* oldState)
{
	FName stateName = newState != nullptr ? newState->GetDescriptionName() : "";

	if (!CompatibleStates.Contains(stateName) && !IsJUmping() && _jumpCount <= 0)
	{
		_jumpCount++;
	}
	_haveSwitchedSurfaceDuringJump = true;
	_lastJumpVector = FVector(0);
	JumpEnd(stateName);
}

void UJumpActionBase::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FInputEntryPool& inputs, UModularControllerComponent* controller, const float inDelta)
{
	if (!_haveSwitchedSurfaceDuringJump)
	{
		if (CompatibleStates.Num() > 0)
		{
			JumpEnd(CompatibleStates[0]);
		}
	}
}


#pragma endregion