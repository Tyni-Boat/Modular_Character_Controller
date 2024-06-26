// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#include "ComponentAndBase/ModularControllerComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "FunctionLibrary.h"
#include "Net/UnrealNetwork.h"


#pragma region Input Handling XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX


void UModularControllerComponent::MovementInput(FVector movement)
{
	FVector normalisationTester = movement;
	if (normalisationTester.Normalize())
		_userMoveDirectionHistory.Add(movement.GetClampedToMaxSize(1));
	else
		_userMoveDirectionHistory.Add(FVector(0));
}

void UModularControllerComponent::ListenInput(const FName key, const FInputEntry entry, const bool hold) const
{
	if (_inputPool.IsValid())
		_inputPool->AddOrReplace(key, entry, hold);
}

void UModularControllerComponent::ListenButtonInput(const FName key, const float buttonBufferTime, const bool hold)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Button;
	entry.Type = buttonBufferTime > 0 ? EInputEntryType::Buffered : EInputEntryType::Simple;
	entry.InputBuffer = buttonBufferTime;
	ListenInput(key, entry, hold);
}

void UModularControllerComponent::ListenValueInput(const FName key, const float value)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Value;
	entry.Axis.X = value;
	ListenInput(key, entry);
}

void UModularControllerComponent::ListenAxisInput(const FName key, const FVector axis)
{
	if (!key.IsValid())
		return;
	FInputEntry entry;
	entry.Nature = EInputEntryNature::InputEntryNature_Axis;
	entry.Axis = axis;
	ListenInput(key, entry);
}



FVector UModularControllerComponent::ConsumeMovementInput()
{
	if (_userMoveDirectionHistory.Num() < 2)
		return FVector(0);
	const FVector move = _userMoveDirectionHistory[0];
	_userMoveDirectionHistory.RemoveAt(0);
	if (DebugType == ControllerDebugType_MovementDebug)
	{
		FVector lookDir = move;
		if (lookDir.Normalize())
		{
			UKismetSystemLibrary::DrawDebugArrow(this, GetLocation(), GetLocation() + lookDir * 100, 50, FColor::Silver, 0.017, 2);
		}
	}
	if (DebugType == ControllerDebugType_InputDebug)
	{
		UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Consumed Move Input: %s"), *move.ToCompactString()), true, true, FColor::Silver, 0, "MoveInput_");
	}
	return move;
}

FInputEntry UModularControllerComponent::ReadInput(const FName key, bool consume)
{
	if (!_inputPool.IsValid())
		return {};
	return _inputPool->ReadInput(key, consume);
}

bool UModularControllerComponent::ReadButtonInput(const FName key, bool consume)
{
	const FInputEntry entry = ReadInput(key, consume);
	return entry.Phase == EInputEntryPhase::InputEntryPhase_Held || entry.Phase == EInputEntryPhase::InputEntryPhase_Pressed;
}

float UModularControllerComponent::ReadValueInput(const FName key)
{
	const FInputEntry entry = ReadInput(key);
	return entry.Axis.X;
}

FVector UModularControllerComponent::ReadAxisInput(const FName key)
{
	const FInputEntry entry = ReadInput(key);
	return entry.Axis;
}

#pragma endregion

