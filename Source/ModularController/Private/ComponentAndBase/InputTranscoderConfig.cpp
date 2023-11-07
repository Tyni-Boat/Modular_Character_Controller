// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/InputTranscoderConfig.h"

bool UInputTranscoderConfig::CheckInputValid(FName inputKey, FInputEntry inputEntry) const
{
	return InputEntries.ContainsByPredicate([inputKey](FNetInputPair item)-> bool { return item.Key == inputKey; });
}

FTranscodedInput UInputTranscoderConfig::EncodeInputs(const FInputEntryPool& inputPool)
{
	InitializeTranscoder();
	return FTranscodedInput();
}

bool UInputTranscoderConfig::DecodeInputs(const FInputEntryPool& inputPool, FTranscodedInput encodedInputs)
{
	InitializeTranscoder();
	return false;
}

void UInputTranscoderConfig::InitializeTranscoder()
{
	if (_transcoderInitialized)
		return;
	if (InputEntries.Num() <= 0)
		return;

	for (int i = 0; i < InputEntries.Num(); i++)
	{
		switch (InputEntries[i].Value.Nature)
		{
		case InputEntryNature_Axis:
			_axisEntries.AddUnique(InputEntries[i].Key);
		case InputEntryNature_Value:
			_valuesEntries.AddUnique(InputEntries[i].Key);
		default:
			_buttonEntries.AddUnique(InputEntries[i].Key);
		}
	}

	_transcoderInitialized = true;
}

double UInputTranscoderConfig::ToXDigitFloatingPoint(double input, double precision)
{
	if (input == 1)
		input -= 0.0000000001;
	const double scaledUpValue = input * FMath::Pow(10.0, precision);
	const int realPart = scaledUpValue;
	return realPart;
}

double UInputTranscoderConfig::FromXDigitFloatingPoint(double input, double precision)
{
	const double scaledDownValue = input / FMath::Pow(10.0, precision);
	const int realPart = scaledDownValue;
	return scaledDownValue - realPart;
}

double UInputTranscoderConfig::DeserializeValueAtIndex(double serializedArray, int index, int digitCount)
{
	if (digitCount <= 0)
		return 0;
	if (index < 0 || index >= (DIGITS_DOUBLE_COUNT / digitCount))
		return 0;
	const int serializedIndex = index * digitCount;
	//Last Index
	const int lastIndex = FMath::Clamp(serializedIndex + digitCount, 0, DIGITS_DOUBLE_COUNT);
	const double multiplierAtLastIndex = FMath::Pow(10.0, lastIndex);
	const unsigned long valueAtLastIndex = (lastIndex >= DIGITS_DOUBLE_COUNT
		                                        ? 0
		                                        : serializedArray / multiplierAtLastIndex) * FMath::Pow(
		10.0, digitCount);
	//this Index
	const double multiplierIndex = FMath::Pow(10.0, serializedIndex);
	const unsigned long valueAtIndex = serializedArray / multiplierIndex;

	return FromXDigitFloatingPoint(FMath::Abs(valueAtIndex - valueAtLastIndex), digitCount);
}

bool UInputTranscoderConfig::SerializeValueAtIndex(double& serializedArray, int index, double val, int digitCount)
{
	if (val > 1 || val < 0)
		return false;
	if (digitCount <= 0)
		return false;
	if (index < 0 || index >= (DIGITS_DOUBLE_COUNT / digitCount))
		return false;
	const int serializedIndex = index * digitCount;
	//Last Index
	const int lastIndex = FMath::Clamp(serializedIndex + digitCount, 0, DIGITS_DOUBLE_COUNT);
	const double multiplierAtLastIndex = FMath::Pow(10.0, lastIndex);
	unsigned long valueAtLastIndex = (lastIndex >= DIGITS_DOUBLE_COUNT ? 0 : serializedArray / multiplierAtLastIndex);
	valueAtLastIndex *= FMath::Pow(10.0, lastIndex);
	//Next Index
	const int nextIndex = FMath::Clamp(serializedIndex - digitCount, 0, DIGITS_DOUBLE_COUNT);
	const double multiplierAtNextIndex = FMath::Pow(10.0, serializedIndex);
	const double tempNextVal = serializedArray / multiplierAtNextIndex;
	const unsigned long valueAtNextIndex = (tempNextVal - ((int)tempNextVal)) * multiplierAtNextIndex;
	//this Index
	const double xDigitVal = ToXDigitFloatingPoint(val, digitCount);
	const double multiplierIndex = FMath::Pow(10.0, serializedIndex);
	const int valueAtIndex = xDigitVal * multiplierIndex;

	//Set
	serializedArray = valueAtLastIndex + valueAtIndex + valueAtNextIndex;

	return true;
}


//Read Operations ################################################################################################################

FVector2D UInputTranscoderConfig::ReadEncodedAxis(FName axisName, FTranscodedInput encodedInput)
{
	const int indexInList = _axisEntries.IndexOfByPredicate([axisName](FName item)-> bool
	{
		return item == axisName;
	});
	if (!_axisEntries.IsValidIndex(indexInList))
		return FVector2D();
	const double deserializedX = DeserializeValueAtIndex(encodedInput.axisCode, indexInList * 2);
	const double deserializedY = DeserializeValueAtIndex(encodedInput.axisCode, (indexInList * 2) + 1);
	return FVector2D(deserializedX, deserializedY);
}

float UInputTranscoderConfig::ReadEncodedValue(FName valueName, FTranscodedInput encodedInput)
{
	const int indexInList = _valuesEntries.IndexOfByPredicate([valueName](FName item)-> bool
	{
		return item == valueName;
	});
	if (!_valuesEntries.IsValidIndex(indexInList))
		return 0;
	const double deserializedValue = DeserializeValueAtIndex(encodedInput.valuesCode, indexInList);
	return deserializedValue;
}

bool UInputTranscoderConfig::ReadEncodedButton(FName buttonName, FTranscodedInput encodedInput)
{
	const int indexInList = _buttonEntries.IndexOfByPredicate([buttonName](FName item)-> bool
	{
		return item == buttonName;
	});
	if (!_buttonEntries.IsValidIndex(indexInList))
		return false;
	TArray<bool> buttonStates = FMathExtension::IntToBoolArray(encodedInput.buttonsCode);
	if (!buttonStates.IsValidIndex(indexInList))
		return false;
	return buttonStates[indexInList];
}

//Write Operations ################################################################################################################

bool UInputTranscoderConfig::WriteEncodedAxis(FName axisName, FVector2D axisVal, FTranscodedInput& encodingInput)
{
	const int indexInList = _axisEntries.IndexOfByPredicate([axisName](FName item)-> bool
	{
		return item == axisName;
	});
	if (!_axisEntries.IsValidIndex(indexInList))
		return false;
	const bool xRes = SerializeValueAtIndex(encodingInput.axisCode, indexInList * 2, axisVal.X);
	const bool yRes = SerializeValueAtIndex(encodingInput.axisCode, (indexInList * 2) + 1, axisVal.Y);
	return xRes && yRes;
}

bool UInputTranscoderConfig::WriteEncodedValue(FName valueName, float val, FTranscodedInput& encodingInput)
{
	const int indexInList = _valuesEntries.IndexOfByPredicate([valueName](FName item)-> bool
	{
		return item == valueName;
	});
	if (!_valuesEntries.IsValidIndex(indexInList))
		return false;
	const bool writeRes = DeserializeValueAtIndex(encodingInput.valuesCode, indexInList);
	return writeRes;
}

bool UInputTranscoderConfig::WriteEncodedButton(FName buttonName, bool state, FTranscodedInput& encodingInput)
{
	const int indexInList = _buttonEntries.IndexOfByPredicate([buttonName](FName item)-> bool
	{
		return item == buttonName;
	});
	if (!_buttonEntries.IsValidIndex(indexInList))
		return false;
	TArray<bool> buttonStates = FMathExtension::IntToBoolArray(encodingInput.buttonsCode);
	if (!buttonStates.IsValidIndex(indexInList))
	{
		for (int i = 0; i <= indexInList; i++)
		{
			if (buttonStates.IsValidIndex(i))
				continue;
			buttonStates.Add(false);
			if (i == indexInList)
			{
				buttonStates[i] = state;
				break;
			}
		}
	}
	else
	{
		buttonStates[indexInList] = state;
	}
	encodingInput.buttonsCode = FMathExtension::BoolArrayToInt(buttonStates);
	
	return true;
}
