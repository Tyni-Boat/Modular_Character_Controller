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
		UKismetSystemLibrary::PrintString(
			this, FString::Printf(TEXT("(%s) -> Custom JumpTo Location: Location(%s)  Distance(%f), Height(%f)"), *GetDescriptionName().ToString(), *Location.ToCompactString(),
			                      jumpLocationDist, jumpHeight), true, true, FColor::Black, 10, "Location");
	}

	//Get planar movement
	float n_Height = jumpHeight / 1;
	FVector dir = FVector::VectorPlaneProject((currentPosition + forwardVector * jumpLocationDist) - currentPosition, gravityDir);
	float X_distance = dir.Length() / 1;
	dir.Normalize();
	FVector destHeight = FVector(0); // FVector::VectorPlaneProject(point - _currentLocation, dir);
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
		horiMomentum += planedMomentum; // : dir * FMath::Max(planedMomentum.Length() * FMath::Clamp(FVector::DotProduct(dir, planedMomentum), 0, 1), Vox);
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
	const bool pressedBtn = controller ? controller->ReadButtonInput(JumpInputCommand) : false;
	return FControllerCheckResult(pressedBtn, startingConditions);
}


FVector4 UJumpActionBase::OnActionBegins_Implementation(UModularControllerComponent* controller,
                                                        const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	FVector4 defaultTimings = FVector4(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration, 0);
	//const auto jumpLocation = JumpLocationInput.IsNone() ? FVector(NAN) : (controller? controller->ReadAxisInput(JumpLocationInput) : FVector(NAN));

	return defaultTimings;
}


void UJumpActionBase::OnActionEnds_Implementation(UModularControllerComponent* controller,
                                                  const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}


FControllerStatus UJumpActionBase::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                                                                   FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	if (bStopOnAnticipation)
	{
		const float normalizedTime = actionInfos.GetNormalizedTime(EActionPhase::Anticipation);
		result.Kinematics = UFunctionLibrary::LerpKinematic(
			FKinematicComponents(FLinearKinematicCondition(result.Kinematics.LinearKinematic.Position, result.Kinematics.LinearKinematic.Velocity),
			                     result.Kinematics.AngularKinematic)
			, FKinematicComponents(FLinearKinematicCondition(result.Kinematics.LinearKinematic.Position, FVector(0)),
			                       result.Kinematics.AngularKinematic), normalizedTime);
	}
	return result;
}


FControllerStatus UJumpActionBase::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
                                                                             const float delta) const
{
	FControllerStatus result = startingConditions;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);

	if (!controller)
		return result;

	const float normalizedTime = actionInfos.GetNormalizedTime(EActionPhase::Active);
	const float forceScale = FAlphaBlend::AlphaToBlendOption(1 - normalizedTime, JumpCurve.GetBlendOption(), JumpCurve.GetCustomCurve());
	const bool pressedBtn = controller->ReadButtonInput(JumpInputCommand, true);

	if (pressedBtn || normalizedTime <= 0.1)
	{
		const FVector jumpAcceleration = -result.Kinematics.GetGravityDirection() * (JumpForce * (1 / actionInfos._startingDurations.Y)) * forceScale
			+ ((result.Kinematics.GetGravityDirection() | result.Kinematics.LinearKinematic.Velocity) > 0 && normalizedTime < 0.1
				   ? -result.Kinematics.LinearKinematic.Velocity.ProjectOnToNormal(result.Kinematics.GetGravityDirection()) / delta
				   : FVector(0));
		result.Kinematics.LinearKinematic.Acceleration += jumpAcceleration * (1 / ActivePhaseDuration);

		//Handle rotation
		{
			result.Kinematics.AngularKinematic = UFunctionLibrary::LookAt(result.Kinematics.AngularKinematic,
			                                                              FVector::VectorPlaneProject(result.MoveInput, result.Kinematics.GetGravityDirection()).GetSafeNormal() * result.
			                                                                                                                                                                       MoveInput.
			                                                                                                                                                                       Length(),
			                                                              TurnTowardDirectionSpeed * (1 - normalizedTime), delta);
		}
	}
	else
	{
		actionInfos.SkipTimeToPhase(EActionPhase::Recovery);
	}

	result.Kinematics.SurfaceBinaryFlag = 0;
	return result;
}

FControllerStatus UJumpActionBase::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller,
                                                                               const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	return result;
}


#pragma endregion
