// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/BaseControllerAction.h"
#include "Animation/AnimMontage.h"
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ComponentAndBase/BaseControllerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"



void UBaseControllerAction::InitializeAction()
{
	_startingDurations = FVector(AnticipationPhaseDuration, ActivePhaseDuration, RecoveryPhaseDuration);
}


int UBaseControllerAction::GetPriority_Implementation()
{
	return 0;
}

FName UBaseControllerAction::GetDescriptionName_Implementation()
{
	return "";
}



void UBaseControllerAction::OnStateChanged_Implementation(UBaseControllerState* newState, UBaseControllerState* oldState)
{

}



void UBaseControllerAction::SaveActionSnapShot()
{
	if (_snapShotSaved)
		return;
	SaveActionSnapShot_Internal();
	_remainingActivationTimer_saved = _remainingActivationTimer;
	_repeatCount_saved = _repeatCount;
	_wasActiveFrame_saved = _wasActiveFrame;
	_cooldownTimer_saved = _cooldownTimer;
	_snapShotSaved = true;
}

void UBaseControllerAction::RestoreActionFromSnapShot()
{
	if (!_snapShotSaved)
		return;
	_remainingActivationTimer = _remainingActivationTimer_saved;
	_cooldownTimer = _cooldownTimer_saved;
	_repeatCount = _repeatCount_saved;
	_wasActiveFrame = _wasActiveFrame_saved;
	CurrentPhase = (_remainingActivationTimer <= AnticipationPhaseDuration) ? ActionPhase_Anticipation : ((_remainingActivationTimer > (ActivePhaseDuration + AnticipationPhaseDuration)) ? ActionPhase_Recovery : ActionPhase_Active);
	RestoreActionFromSnapShot_Internal();
	_snapShotSaved = false;
}

void UBaseControllerAction::SaveActionSnapShot_Internal()
{
}

void UBaseControllerAction::RestoreActionFromSnapShot_Internal()
{
}


void UBaseControllerAction::OnActionChanged_Implementation(UBaseControllerAction* newAction,
	UBaseControllerAction* lastAction)
{
}

void UBaseControllerAction::OnActionPhaseChanged_Implementation(EActionPhase newPhase,
	EActionPhase lastPhase)
{
}

bool UBaseControllerAction::GetActivatedLastFrame()
{
	return _wasActiveFrame;
}

void UBaseControllerAction::SetActivatedLastFrame(bool value)
{
	_wasActiveFrame = value;
}

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



void UBaseControllerAction::OnActionBegins_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
                                                          UModularControllerComponent* controller, const float inDelta)
{
}

void UBaseControllerAction::OnActionEnds_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
}

bool UBaseControllerAction::CheckAction_Implementation(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta)
{
	return false;
}



FVelocity UBaseControllerAction::OnActionProcessAnticipationPhase_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	return {};
}

FVelocity UBaseControllerAction::OnActionProcessActivePhase_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput, UModularControllerComponent* controller,
	const float inDelta)
{
	return {};
}

FVelocity UBaseControllerAction::OnActionProcessRecoveryPhase_Implementation(FStatusParameters& controllerStatus,
	const FKinematicInfos& inDatas, const FVelocity fromVelocity, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	return {};
}




void UBaseControllerAction::OnActionBegins_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	OnActionBegins(inDatas, moveInput, controller, inDelta);

	//Set timers
	_remainingActivationTimer = AnticipationPhaseDuration + ActivePhaseDuration + RecoveryPhaseDuration;
	_cooldownTimer = 0;
}

void UBaseControllerAction::OnActionEnds_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UModularControllerComponent* controller, const float inDelta)
{
	OnActionEnds(inDatas, moveInput, controller, inDelta);

	//Reset timers
	_remainingActivationTimer = 0;
	_cooldownTimer = CoolDownDelay;
	CurrentPhase = ActionPhase_Undetermined;
}

bool UBaseControllerAction::CheckAction_Internal(const FKinematicInfos& inDatas, const FVector moveInput,
	UInputEntryPool* inputs, UModularControllerComponent* controller, const float inDelta)
{
	//Update cooldown timer
	if (_cooldownTimer > 0)
	{
		_cooldownTimer -= inDelta;
		return false;
	}
	
	if (CurrentPhase == ActionPhase_Anticipation || CurrentPhase == ActionPhase_Active)
		return false;
	
	if (CurrentPhase == ActionPhase_Recovery && !bCanTransitionToSelf)
		return false;

	return CheckAction(inDatas, moveInput, inputs, controller, inDelta);
}

FVelocity UBaseControllerAction::OnActionProcess_Internal(FStatusParameters& controllerStatus, const FKinematicInfos& inDatas, const FVelocity fromVelocity,
	const FVector moveInput, UModularControllerComponent* controller, const float inDelta)
{
	//Update activation timer
	if (_remainingActivationTimer > 0)
	{
		_remainingActivationTimer -= inDelta;

		if (_remainingActivationTimer > (ActivePhaseDuration + RecoveryPhaseDuration))
		{
			if (CurrentPhase != ActionPhase_Anticipation)
			{
				OnActionPhaseChanged(ActionPhase_Anticipation, CurrentPhase);
				CurrentPhase = ActionPhase_Anticipation;
			}
			return OnActionProcessAnticipationPhase(controllerStatus, inDatas, fromVelocity, moveInput, controller, inDelta);
		}
		else if (_remainingActivationTimer > RecoveryPhaseDuration && _remainingActivationTimer <= (ActivePhaseDuration + RecoveryPhaseDuration))
		{
			if (CurrentPhase != ActionPhase_Active)
			{
				OnActionPhaseChanged(ActionPhase_Active, CurrentPhase);
				CurrentPhase = ActionPhase_Active;
			}
			return OnActionProcessActivePhase(controllerStatus, inDatas, fromVelocity, moveInput, controller, inDelta);
		}
		else
		{
			if (CurrentPhase != ActionPhase_Recovery)
			{
				OnActionPhaseChanged(ActionPhase_Recovery, CurrentPhase);
				CurrentPhase = ActionPhase_Recovery;
			}
			return OnActionProcessRecoveryPhase(controllerStatus, inDatas, fromVelocity, moveInput, controller, inDelta);
		}
	}

	return fromVelocity;
}




FString UBaseControllerAction::DebugString()
{
	return GetDescriptionName().ToString();
}

