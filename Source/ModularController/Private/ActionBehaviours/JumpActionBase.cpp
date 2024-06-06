// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/JumpActionBase.h"
#include <Kismet/KismetMathLibrary.h>
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"






#pragma region Check


bool UJumpActionBase::CheckJump(UModularControllerComponent* controller, const FVector currentPosition, const FQuat currentRotation, const FVector gravityDir) const
{
	if(!controller)
		return false;

	//FHitResult ceilHitRes;
	//if (controller->ComponentTraceCastSingle(ceilHitRes, currentPosition - gravityDir, -gravityDir * MaxJumpHeight, currentRotation, -1, controller->bUseComplexCollision))
	//{
	//	if ((ceilHitRes.Location - currentPosition).Length() < MinJumpHeight)
	//	{
	//		return false;
	//	}
	//}

	return true;
}

#pragma endregion

#pragma region Jump


FVector UJumpActionBase::Jump(const FControllerStatus startingConditions, const FVector gravity, const float inDelta, const FVector customJumpLocation) const
{
	//Get the maximum jump height
	FVector currentPosition = startingConditions.Kinematics.LinearKinematic.Position;
	FQuat currentRotation = startingConditions.Kinematics.AngularKinematic.Orientation;
	FVector gravityDir = gravity.GetSafeNormal();
	float gravityAcc = gravity.Length();
	float jumpHeight = MaxJumpHeight;

	//Get the forward vector
	FVector forwardVector = UseSurfaceNormalOnNoDirection ? _jumpSurfaceNormal : FVector(0);
	FVector inputVector = startingConditions.MoveInput;
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
	FVector vertMomentum = (startingConditions.Kinematics.LinearKinematic.Velocity / inDelta).ProjectOnToNormal(gravityDir);
	FVector horiMomentum = FVector::VectorPlaneProject(startingConditions.Kinematics.LinearKinematic.Velocity / inDelta, gravityDir);

	if (startingConditions.Kinematics.LinearKinematic.Velocity.Length() > 0)
	{
		vertMomentum = startingConditions.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(gravityDir);
		FVector planedMomentum = FVector::VectorPlaneProject(startingConditions.Kinematics.LinearKinematic.Velocity, gravityDir);
		horiMomentum += planedMomentum;// : dir * FMath::Max(planedMomentum.Length() * FMath::Clamp(FVector::DotProduct(dir, planedMomentum), 0, 1), Vox);
	}

	FVector jumpForce = (-gravityDir * Voy) + (UseMomentum ? horiMomentum : (dir * Vox * 1));

	return jumpForce;
}


#pragma endregion

#pragma region Functions


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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerCheckResult UJumpActionBase::CheckAction_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta, bool asLastActiveAction)
{
	const auto kinematics = startingConditions.Kinematics;
	const bool pressedBtn = controller->ReadButtonInput(JumpInputCommand);
	const bool gotJumped = CheckJump(controller, kinematics.LinearKinematic.Position, kinematics.AngularKinematic.Orientation, controller->GetGravityDirection());
	return FControllerCheckResult(pressedBtn && gotJumped, startingConditions);
}

FKinematicComponents UJumpActionBase::OnActionBegins_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	//if (!IsSimulated() && controller)
	//{
	//	//Bind montage needs call back
	//	if (bUseMontageDuration)
	//		_EndDelegate.BindUObject(this, &UJumpActionBase::OnAnimationEnded);

	//	//Play montage
	//	float montageDuration = 0;
	//	if (bMontageShouldBePlayerOnStateAnimGraph)
	//	{
	//		if (const auto currentState = controller->GetCurrentControllerState())
	//			montageDuration = controller->PlayAnimationMontageOnState_Internal(JumpMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration, _EndDelegate);
	//	}
	//	else
	//	{
	//		montageDuration = controller->PlayAnimationMontage_Internal(JumpMontage, -1, bUseMontageDuration, _EndDelegate);
	//	}


	//	if (bDebugAction)
	//	{
	//		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Montage duration: %f"), *GetDescriptionName().ToString(), montageDuration), true, true, FColor::Emerald, 5, "JumpMontageDuration");
	//	}

	//	if (bUseMontageDuration && montageDuration > 0)
	//	{
	//		RemapDuration(montageDuration);
	//	}
	//}

	//_jumped = false;
	return startingConditions;
}

FKinematicComponents UJumpActionBase::OnActionEnds_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	_jumped = false;
	return startingConditions;
}

FControllerStatus UJumpActionBase::OnActionProcessAnticipationPhase_Implementation(
	UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;
	result.CustomPhysicProperties = FVector(-1);
	return result;
}

FControllerStatus UJumpActionBase::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;
	//result.Kinematics.LinearKinematic.RemoveCompositeMovement(1);

	//if (_jumped)
	//{
	//	return result;
	//}

	//result.CustomPhysicProperties = FVector(-1);

	////Handle rotation
	//{
	//	result.Kinematics.AngularKinematic = UStructExtensions::LookAt(result.Kinematics.AngularKinematic, result.MoveInput, TurnTowardDirectionSpeed, delta);
	//}

	//const FVector jumpLocation = IsSimulated() ? FVector(NAN) :
	//	(JumpLocationInput.IsNone() ? FVector(NAN) : controller->ReadAxisInput(JumpLocationInput, true, bDebugAction, this));

	//const auto jumpForce = Jump(startingConditions, controller->GetGravity(), delta, jumpLocation);
	////result.Kinematics.LinearKinematic.Velocity = jumpForce;
	//result.Kinematics.LinearKinematic.AddCompositeMovement(jumpForce);

	//if (bDebugAction)
	//{
	//	UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Propulsion Vector: (%s)"), *GetDescriptionName().ToString(), *jumpForce.ToCompactString()), true, true, FColor::Yellow, 10, "");
	//}

	////instant rotation if custom location
	//if (!jumpLocation.ContainsNaN())
	//{
	//	//const FVector up = -inDatas.Gravity.GetSafeNormal();
	//	//FVector fwd = FVector::VectorPlaneProject(jumpForce, up);
	//	//if (fwd.Normalize())
	//	//{
	//	//	const FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
	//	//	result.Rotation = fwdRot;
	//	//}
	//}

	//if (!IsSimulated())
	//{
	//	if (controller->GetCurrentSurface().GetSurfacePrimitive() != nullptr)
	//	{
	//		//Push Objects
	//		if (controller->GetCurrentSurface().GetSurfacePrimitive()->IsSimulatingPhysics())
	//		{
	//			controller->GetCurrentSurface().GetSurfacePrimitive()->AddImpulseAtLocation(-jumpForce * controller->GetMass() * delta, controller->GetCurrentSurface().GetHitResult().ImpactPoint, controller->GetCurrentSurface().GetHitResult().BoneName);
	//		}
	//	}
	//}

	//_jumped = true;

	const bool pressedBtn = controller->ReadButtonInput(JumpInputCommand);
	if(pressedBtn)
	{
		result.Kinematics.LinearKinematic.Acceleration += -controller->GetGravity() * 3;
	}else
	{
		_remainingActivationTimer = RecoveryPhaseDuration;
	}



	return result;
}

FControllerStatus UJumpActionBase::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta)
{
	FControllerStatus result = startingConditions;
	result.CustomPhysicProperties = FVector(-1);
	return result;
}


void UJumpActionBase::SaveActionSnapShot_Internal()
{
	_jumped_saved = _jumped;
	_jumpSurfaceNormal_saved = _jumpSurfaceNormal;
}

void UJumpActionBase::RestoreActionFromSnapShot_Internal()
{
	_jumped = _jumped_saved;
	_jumpSurfaceNormal = _jumpSurfaceNormal_saved;
}

#pragma endregion