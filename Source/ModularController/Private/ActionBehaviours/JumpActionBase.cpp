// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ActionBehaviours/JumpActionBase.h"
#include <Kismet/KismetMathLibrary.h>
#include "CoreMinimal.h"
#include "FunctionLibrary.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Pawn.h"




#pragma region Jump


UJumpActionBase::UJumpActionBase()
{
	JumpCurve = NewObject<UCurveFloat>();
}

FVector UJumpActionBase::VelocityJumpTo(const FControllerStatus startingConditions, const FVector gravity, const float inDelta, const FVector Location) const
{
	//Get the maximum jump height
	FVector currentPosition = startingConditions.Kinematics.LinearKinematic.Position;
	FVector gravityDir = gravity.GetSafeNormal();
	float gravityAcc = gravity.Length();
	float jumpHeight = 200;

	//Get the forward vector
	FVector forwardVector = startingConditions.Kinematics.AngularKinematic.Orientation.Vector();
	FVector inputVector = startingConditions.MoveInput;
	if (inputVector.Length() > 0)
	{
		inputVector.Normalize();
		forwardVector = inputVector;
	}

	//Specified jump location
	float jumpLocationDist = 1000;
	if (!Location.ContainsNaN())
	{
		auto locationVector = (Location - currentPosition);
		jumpHeight += locationVector.ProjectOnToNormal(gravityDir).Length();
		forwardVector = FVector::VectorPlaneProject(locationVector, gravityDir);
		jumpLocationDist = forwardVector.Length();
		forwardVector.Normalize();
	}

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Custom JumpTo Location: Location(%s)  Distance(%f), Height(%f)"), *GetDescriptionName().ToString(), *Location.ToCompactString(), jumpLocationDist, jumpHeight), true, true, FColor::Black, 10, "Location");
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

	FVector jumpV0 = (-gravityDir * Voy) + (dir * Vox * 1);

	return jumpV0;
}


#pragma endregion

#pragma region Functions

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FControllerCheckResult UJumpActionBase::CheckAction_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const
{
	const auto kinematics = startingConditions.Kinematics;
	const bool pressedBtn = controller? controller->ReadButtonInput(JumpInputCommand) : false;
	return FControllerCheckResult(pressedBtn, startingConditions);
}


FVector UJumpActionBase::OnActionBegins_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	FVector defaultTimings = FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
	//const auto jumpLocation = JumpLocationInput.IsNone() ? FVector(NAN) : (controller? controller->ReadAxisInput(JumpLocationInput) : FVector(NAN));

	if (controller)
	{
		//Play montage
		float montageDuration = 0;
		if (bMontageShouldBePlayerOnStateAnimGraph)
		{
			if (const auto currentState = controller->GetCurrentControllerState())
				montageDuration = controller->PlayAnimationMontageOnState_Internal(JumpMontage, currentState->GetDescriptionName(), -1, bUseMontageDuration);
		}
		else
		{
			montageDuration = controller->PlayAnimationMontage_Internal(JumpMontage, -1, bUseMontageDuration);
		}

		if (bDebugAction)
		{
			UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("(%s) -> Montage duration: %f"), *GetDescriptionName().ToString(), montageDuration), true, true, FColor::Emerald, 5, "JumpMontageDuration");
		}

		if (bUseMontageDuration && montageDuration > 0)
		{
			defaultTimings = RemapDuration(montageDuration);
		}
	}

	return defaultTimings;
}


void UJumpActionBase::OnActionEnds_Implementation(UModularControllerComponent* controller,
	const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	if (controller)
	{
		if (bMontageShouldBePlayerOnStateAnimGraph)
		{
			if (const auto currentState = controller->GetCurrentControllerState())
			{
				if (auto* animInstance = controller->GetAnimInstance(currentState->GetDescriptionName()))
				{
					if (animInstance->Montage_IsPlaying(JumpMontage.Montage))
					{
						animInstance->Montage_Stop(0.2, JumpMontage.Montage);
					}
				}
			}
		}
		else
		{
			if (auto* animInstance = controller->GetAnimInstance())
			{
				if (animInstance->Montage_IsPlaying(JumpMontage.Montage))
				{
					animInstance->Montage_Stop(0.2, JumpMontage.Montage);
				}
			}
		}
	}
}


FControllerStatus UJumpActionBase::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.CustomPhysicProperties = FVector(-1);
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	return result;
}


FControllerStatus UJumpActionBase::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);

	if (!controller)
		return  result;

	FVector reactionForce = FVector(0);

	//if (!_jumpLocation.ContainsNaN())
	//{
	//	const auto jumpForce = VelocityJumpTo(startingConditions, controller->GetGravity(), delta, _jumpLocation);
	//	result.Kinematics.LinearKinematic.AddCompositeMovement(jumpForce);
	//	reactionForce = -(jumpForce / delta) * controller->GetMass() * 0.01;

	//	////instant rotation
	//	const FVector up = -controller->GetGravityDirection();
	//	FVector fwd = FVector::VectorPlaneProject(_jumpLocation - startingConditions.Kinematics.LinearKinematic.Position, up);
	//	if (fwd.Normalize())
	//	{
	//		const FQuat fwdRot = UKismetMathLibrary::MakeRotationFromAxes(fwd, FVector::CrossProduct(up, fwd), up).Quaternion();
	//		result.Kinematics.AngularKinematic.Orientation = fwdRot;
	//	}

	//	_remainingActivationTimer = RecoveryPhaseDuration;
	//}

	const float normalizedTime = actionInfos.GetNormalizedTime(ActionPhase_Active);
	float forceScale = 1;
	if (JumpCurve)
	{
		forceScale = JumpCurve->GetFloatValue(normalizedTime);
	}
	const bool pressedBtn = controller->ReadButtonInput(JumpInputCommand, true);

	if (pressedBtn || normalizedTime <= 0.1)
	{
		const FVector jumpAcceleration = -controller->GetGravityDirection() * JumpForce * forceScale;
		result.Kinematics.LinearKinematic.Acceleration += jumpAcceleration * (1 / ActivePhaseDuration);
		reactionForce = -jumpAcceleration * controller->GetMass() * 0.01;

		//Handle rotation
		{
			result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic, result.MoveInput, TurnTowardDirectionSpeed, delta);
		}
	}
	else
	{
		actionInfos.SkipTimeToPhase(ActionPhase_Recovery);
	}

	//Push Objects
	{
		if (startingConditions.ControllerSurface.GetSurfacePrimitive() != nullptr)
		{
			auto* surfaceComponent = startingConditions.ControllerSurface.GetSurfacePrimitive();
			const FVector impactPt = startingConditions.ControllerSurface.GetHitResult().ImpactPoint;
			const FName impactBone = startingConditions.ControllerSurface.GetHitResult().BoneName;

			if (reactionForce.SquaredLength() > 0 && surfaceComponent->IsSimulatingPhysics())
			{
				surfaceComponent->AddImpulseAtLocation(reactionForce, impactPt, impactBone);
			}
		}
	}

	result.CustomPhysicProperties = FVector(-1);

	return result;
}

FControllerStatus UJumpActionBase::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
	const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	result.CustomPhysicProperties = FVector(-1);
	return result;
}


#pragma endregion