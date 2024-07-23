// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/BaseControllerAction.h"
#include "CoreMinimal.h"
#include "FunctionLibrary.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "Kismet/KismetSystemLibrary.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FVector4 UBaseControllerAction::OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
                                                             const float delta) const
{
	return FVector4(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration, 0);
}

void UBaseControllerAction::OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
                                                        const float delta) const
{
}

FControllerCheckResult UBaseControllerAction::CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta,
                                                                         bool asLastActiveAction) const
{
	return FControllerCheckResult(false, startingConditions);
}


FControllerStatus UBaseControllerAction::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                                                                         FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                                                                   FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                                                                     FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString UBaseControllerAction::DebugString()
{
	return GetDescriptionName().ToString();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FVector UBaseControllerAction::RemapDuration(float duration, FVector customTiming, bool tryDontMapAnticipation, bool tryDontMapRecovery) const
{
	FVector timings = FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
	if(!customTiming.IsZero())
		timings = customTiming;
	const float anticipationScale = timings.X / (timings.X + timings.Y + timings.Z);
	const float recoveryScale = timings.Z / (timings.X + timings.Y + timings.Z);
	//Anticipation
	float newAnticipation = duration * anticipationScale;
	if (tryDontMapAnticipation)
	{
		newAnticipation = FMath::Clamp(timings.X, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
	}

	//Recovery
	float newRecovery = duration * recoveryScale;
	if (tryDontMapRecovery)
	{
		newRecovery = FMath::Clamp(timings.Z, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
	}

	const float newDuration = duration - (newAnticipation + newRecovery);

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(
			this, FString::Printf(TEXT("Remap from (%s) to (%s)"), *FVector(timings.X, timings.Y, timings.Z).ToCompactString(),
			                      *FVector(newAnticipation, newDuration, newRecovery).ToCompactString()), true, true, FColor::Orange, 5, "reMapingDuration");
	}

	return FVector(newAnticipation, newDuration, newRecovery);
}


FVector UBaseControllerAction::RemapDurationByMontageSections(UAnimMontage* montage, FVector fallBackTimings) const
{
	if (!montage)
		return fallBackTimings;
	const int sectionCount = montage->GetNumSections();
	const int activePhaseIndex = sectionCount > 2 ? 1 : (sectionCount > 1 ? 1 : 0);
	const int recoveryPhaseIndex = activePhaseIndex + 1;
	const int anticipationPhaseIndex = activePhaseIndex - 1;
	FVector finalTimings = fallBackTimings;
	//Anticipation
	if (anticipationPhaseIndex >= 0)
	{
		finalTimings.X = montage->GetSectionLength(anticipationPhaseIndex);
	}
	//Active
	finalTimings.Y = montage->GetSectionLength(activePhaseIndex);
	//Recovery
	if (recoveryPhaseIndex < sectionCount)
	{
		finalTimings.Z = montage->GetSectionLength(recoveryPhaseIndex);
	}

	return finalTimings;
}





# pragma region Action montage


UActionMontage::UActionMontage()
{
	ActionName = "BuiltIn_ActionMontage";
	CoolDownDelay = 0;
	bShouldControllerStateCheckOverride = true;
	//bDebugAction = true;
	Reset();
}


bool UActionMontage::SetActionParams(UModularControllerComponent* controller, FActionMotionMontage montage, int priority)
{
	if (!controller)
		return false;
	if (!montage.Montage)
		return false;
	MontageToPlay = montage;
	MontageToPlay.bUseMontageLenght = true;
	MontageToPlay.bStopOnActionEnds = true;
	ActionPriority = priority;
	//
	bCanTransitionToSelf = false;
	FActionMontageLibrary library;
	library.Library = TArray<FActionMotionMontage>{MontageToPlay};
	if (controller->ActionMontageLibraryMap.Contains(GetDescriptionName()))
		controller->ActionMontageLibraryMap[GetDescriptionName()] = library;
	else
		controller->ActionMontageLibraryMap.Add(GetDescriptionName(), library);

	return true;
}

void UActionMontage::Reset()
{
	ActionPriority = -1;
	MontageToPlay = FActionMotionMontage();
	AnticipationPhaseDuration = 0.05;
	ActivePhaseDuration = 0.8;
	RecoveryPhaseDuration = 0.15;
}


FVector4 UActionMontage::OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput,
                                                       const float delta) const
{
	return FVector4(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration, 0);
}

void UActionMontage::OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
}

FControllerCheckResult UActionMontage::CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta,
                                                                  bool asLastActiveAction) const
{
	return FControllerCheckResult(MontageToPlay.Montage && GetPriority() >= 0, startingConditions);
}

FControllerStatus UActionMontage::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions,
                                                                                  FActionInfos& actionInfos, const float delta) const
{
	FControllerStatus result = startingConditions;
	if (!controller)
		return result;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	result.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	result.Kinematics.AngularKinematic.RotationSpeed = FVector(0);
	const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), MontageToPlay.Montage);
	controller->ReadRootMotion(result.Kinematics, result.Kinematics.LinearKinematic.Velocity, ERootMotionType::Override, 1, RMWeight);
	return result;
}

FControllerStatus UActionMontage::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
                                                                            const float delta) const
{
	FControllerStatus result = startingConditions;
	if (!controller)
		return result;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	result.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	result.Kinematics.AngularKinematic.RotationSpeed = FVector(0);
	const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), MontageToPlay.Montage);
	controller->ReadRootMotion(result.Kinematics, result.Kinematics.LinearKinematic.Velocity, ERootMotionType::Override, 1, RMWeight);
	return result;
}

FControllerStatus UActionMontage::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos,
                                                                              const float delta) const
{
	FControllerStatus result = startingConditions;
	if (!controller)
		return result;
	result.Kinematics.LinearKinematic.SnapDisplacement = FVector(0);
	result.Kinematics.AngularKinematic.AngularAcceleration = FVector(0);
	result.Kinematics.AngularKinematic.RotationSpeed = FVector(0);
	const double RMWeight = UFunctionLibrary::GetMontageCurrentWeight(controller->GetAnimInstance(), MontageToPlay.Montage);
	controller->ReadRootMotion(result.Kinematics, result.Kinematics.LinearKinematic.Velocity, ERootMotionType::Override, 1, RMWeight);
	return result;
}

#pragma endregion