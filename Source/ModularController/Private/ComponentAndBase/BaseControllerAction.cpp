// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/BaseControllerAction.h"
#include "CoreMinimal.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "Kismet/KismetSystemLibrary.h"



void UBaseControllerAction::InitializeAction()
{
	_startingDurations = FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FKinematicComponents UBaseControllerAction::OnActionBegins_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	return startingConditions;
}

FKinematicComponents UBaseControllerAction::OnActionEnds_Implementation(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	return startingConditions;
}

FControllerCheckResult UBaseControllerAction::CheckAction_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction)
{
	return FControllerCheckResult(false, startingConditions);
}


FControllerStatus UBaseControllerAction::OnActionProcessAnticipationPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessActivePhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	return startingConditions;
}

FControllerStatus UBaseControllerAction::OnActionProcessRecoveryPhase_Implementation(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	return startingConditions;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UBaseControllerAction::OnControllerStateChanged_Implementation(UModularControllerComponent* onController, FName newBehaviourDescName, int newPriority)
{
}

void UBaseControllerAction::OnControllerActionChanged_Implementation(UModularControllerComponent* onController,	UBaseControllerAction* newAction, UBaseControllerAction* lastAction)
{
}

void UBaseControllerAction::OnActionPhaseChanged_Implementation(EActionPhase newPhase, EActionPhase lastPhase)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


double UBaseControllerAction::GetRemainingActivationTime()
{
	if (_remainingActivationTimer < 0)
		_remainingActivationTimer = 0;
	return _remainingActivationTimer;
}

double UBaseControllerAction::GetRemainingCoolDownTime()
{
	if (_cooldownTimer < 0)
		_cooldownTimer = 0;
	return _cooldownTimer;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString UBaseControllerAction::DebugString()
{
	return GetDescriptionName().ToString();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UBaseControllerAction::SaveActionSnapShot()
{
	if (_snapShotSaved)
		return;
	SaveActionSnapShot_Internal();
	_remainingActivationTimer_saved = _remainingActivationTimer;
	_repeatCount_saved = _repeatCount;
	_cooldownTimer_saved = _cooldownTimer;
	_snapShotSaved = true;
}

void UBaseControllerAction::RestoreActionFromSnapShot()
{
	return;
	//if (!_snapShotSaved)
	//	return;
	//_remainingActivationTimer = _remainingActivationTimer_saved;
	//_cooldownTimer = _cooldownTimer_saved;
	//_repeatCount = _repeatCount_saved;
	//CurrentPhase = (_remainingActivationTimer <= AnticipationPhaseDuration) ? ActionPhase_Anticipation : ((_remainingActivationTimer > (ActivePhaseDuration + AnticipationPhaseDuration)) ? ActionPhase_Recovery : ActionPhase_Active);
	//RestoreActionFromSnapShot_Internal();
	//_snapShotSaved = false;
}

void UBaseControllerAction::SaveActionSnapShot_Internal()
{
}

void UBaseControllerAction::RestoreActionFromSnapShot_Internal()
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FKinematicComponents UBaseControllerAction::OnActionBegins_Internal(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	FKinematicComponents kinematic = OnActionBegins(controller, startingConditions, moveInput, delta);

	//Set timers
	_remainingActivationTimer = AnticipationPhaseDuration + ActivePhaseDuration + RecoveryPhaseDuration;
	_cooldownTimer = 0;

	return kinematic;
}

FKinematicComponents UBaseControllerAction::OnActionEnds_Internal(UModularControllerComponent* controller, const FKinematicComponents startingConditions, const FVector moveInput, const float delta)
{
	FKinematicComponents kinematic = OnActionEnds(controller, startingConditions, moveInput, delta);

	//Reset timers
	_remainingActivationTimer = 0;
	_cooldownTimer = CoolDownDelay;
	CurrentPhase = ActionPhase_Undetermined;

	return kinematic;
}

FControllerCheckResult UBaseControllerAction::CheckAction_Internal(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta, bool asLastActiveAction)
{
	//Update cooldown timer
	if (_cooldownTimer > 0)
	{
		_cooldownTimer -= delta;
		return FControllerCheckResult(false, startingConditions);
	}

	if (CurrentPhase == ActionPhase_Anticipation || CurrentPhase == ActionPhase_Active)
		return FControllerCheckResult(false, startingConditions);

	if (CurrentPhase == ActionPhase_Recovery && !bCanTransitionToSelf)
		return FControllerCheckResult(false, startingConditions);

	return CheckAction(controller, startingConditions, delta, asLastActiveAction);
}

FControllerStatus UBaseControllerAction::OnActionProcess_Internal(UModularControllerComponent* controller, const FControllerStatus startingConditions, const float delta)
{
	//Update activation timer
	if (_remainingActivationTimer > 0)
	{
		_remainingActivationTimer -= delta;

		if (_remainingActivationTimer > (ActivePhaseDuration + RecoveryPhaseDuration))
		{
			if (CurrentPhase != ActionPhase_Anticipation)
			{
				OnActionPhaseChanged(ActionPhase_Anticipation, CurrentPhase);
				CurrentPhase = ActionPhase_Anticipation;
			}
			return OnActionProcessAnticipationPhase(controller, startingConditions, delta);
		}
		else if (_remainingActivationTimer > RecoveryPhaseDuration && _remainingActivationTimer <= (ActivePhaseDuration + RecoveryPhaseDuration))
		{
			if (CurrentPhase != ActionPhase_Active)
			{
				OnActionPhaseChanged(ActionPhase_Active, CurrentPhase);
				CurrentPhase = ActionPhase_Active;
			}
			return OnActionProcessActivePhase(controller, startingConditions, delta);
		}
		else
		{
			if (CurrentPhase != ActionPhase_Recovery)
			{
				OnActionPhaseChanged(ActionPhase_Recovery, CurrentPhase);
				CurrentPhase = ActionPhase_Recovery;
			}
			return OnActionProcessRecoveryPhase(controller, startingConditions, delta);
		}
	}

	return startingConditions;
}
