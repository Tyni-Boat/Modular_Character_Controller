// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/BaseControllerAction.h"
#include "CoreMinimal.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "Kismet/KismetSystemLibrary.h"



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FVector UBaseControllerAction::OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{
	return FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
}

void UBaseControllerAction::OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta) const
{

}

FControllerCheckResult UBaseControllerAction::CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction) const
{
	return FControllerCheckResult(false, startingConditions);
}


FControllerStatus UBaseControllerAction::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, FActionInfos& actionInfos, const float delta) const
{
	return startingConditions;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString UBaseControllerAction::DebugString()
{
	return GetDescriptionName().ToString();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FVector UBaseControllerAction::RemapDuration(float duration, bool tryDontMapAnticipation, bool tryDontMapRecovery) const
{
	const float anticipationScale = AnticipationPhaseDuration / (AnticipationPhaseDuration + ActivePhaseDuration + RecoveryPhaseDuration);
	const float recoveryScale = RecoveryPhaseDuration / (AnticipationPhaseDuration + ActivePhaseDuration + RecoveryPhaseDuration);
	//Anticipation
	float newAnticipation = duration * anticipationScale;
	if (tryDontMapAnticipation)
	{
		newAnticipation = FMath::Clamp(AnticipationPhaseDuration, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
	}

	//Recovery
	float newRecovery = duration * recoveryScale;
	if (tryDontMapRecovery)
	{
		newRecovery = FMath::Clamp(RecoveryPhaseDuration, 0, FMath::Clamp((duration * 0.5f) - 0.05, 0, TNumericLimits<float>().Max()));
	}

	const float newDuration = duration - (newAnticipation + newRecovery);

	if (bDebugAction)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Remap from (%s) to (%s)"), *FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration).ToCompactString(), *FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration).ToCompactString()), true, true, FColor::Orange, 5, "remapingDuration");
	}

	return FVector(newAnticipation, newDuration, newRecovery);
}
