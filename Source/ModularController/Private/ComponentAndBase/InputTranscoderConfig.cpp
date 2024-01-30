// Copyright © 2023 by Tyni Boat. All Rights Reserved.


#include "ComponentAndBase/InputTranscoderConfig.h"


bool UInputTranscoderConfig::CheckInputValid(FName inputKey, FInputEntry inputEntry) const
{
	return InputEntries.ContainsByPredicate([inputKey](FNetInputPair item)-> bool { return item.Key == inputKey; });
}

FTranscodedInput UInputTranscoderConfig::EncodeInputs(const FInputEntryPool& inputPool)
{
	InitializeTranscoder();

	FTranscodedInput encodedInputs = FTranscodedInput();

	if (inputPool._inputPool_last.Num() > 0)
	{
		for (auto item : inputPool._inputPool_last)
		{
			switch (item.Value.Nature)
			{
			case InputEntryNature_Axis:
				WriteEncodedAxis(item.Key, FVector2D(item.Value.Axis.X, item.Value.Axis.Y), encodedInputs);
				break;
			case InputEntryNature_Value:
				WriteEncodedValue(item.Key, item.Value.Axis.X, encodedInputs);
				break;
			default:
				WriteEncodedButton(item.Key, item.Value.Phase == InputEntryPhase_Pressed || item.Value.Phase == InputEntryPhase_Held, encodedInputs);
				break;
			}
		}
	}

	return encodedInputs;
}

bool UInputTranscoderConfig::DecodeInputs(FInputEntryPool& inputPool, FTranscodedInput encodedInputs)
{
	InitializeTranscoder();

	//Axis
	if (encodedInputs.axisCode >= 0)
	{
		for (int i = 0; i < _axisEntries.Num(); i++)
		{
			FName key = _axisEntries[i];
			int inputIndex = InputEntries.IndexOfByPredicate([key](FNetInputPair item) -> bool
				{
					return item.Key == key;
				});
			if (!InputEntries.IsValidIndex(inputIndex))
				continue;
			FInputEntry entry = InputEntries[inputIndex].Value;
			FVector2D axisVal = ReadEncodedAxis(key, encodedInputs);
			entry.Axis = FVector(axisVal.X, axisVal.Y, 0);
			inputPool.AddOrReplace(key, entry);
		}
	}

	//Values
	if (encodedInputs.valuesCode >= 0)
	{
		for (int i = 0; i < _valuesEntries.Num(); i++)
		{
			FName key = _valuesEntries[i];
			int inputIndex = InputEntries.IndexOfByPredicate([key](FNetInputPair item) -> bool
				{
					return item.Key == key;
				});
			if (!InputEntries.IsValidIndex(inputIndex))
				continue;
			FInputEntry entry = InputEntries[inputIndex].Value;
			float val = ReadEncodedValue(key, encodedInputs);
			entry.Axis = FVector(val, 0, 0);
			inputPool.AddOrReplace(key, entry);
		}
	}

	//Button
	if (encodedInputs.buttonsCode >= 0)
	{
		for (int i = 0; i < _buttonEntries.Num(); i++)
		{
			FName key = _buttonEntries[i];
			int inputIndex = InputEntries.IndexOfByPredicate([key](FNetInputPair item) -> bool
				{
					return item.Key == key;
				});
			if (!InputEntries.IsValidIndex(inputIndex))
				continue;
			FInputEntry entry = InputEntries[inputIndex].Value;
			bool state = ReadEncodedButton(key, encodedInputs);
			if (state)
				inputPool.AddOrReplace(key, entry);
		}
	}

	inputPool.UpdateInputs(0);
	return true;
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
			break;
		case InputEntryNature_Value:
			_valuesEntries.AddUnique(InputEntries[i].Key);
			break;
		default:
			_buttonEntries.AddUnique(InputEntries[i].Key);
			break;
		}
	}

	_transcoderInitialized = true;
}

double UInputTranscoderConfig::ToXDigitFloatingPoint(const double input, const double precision) const
{
	double rangedValue = input;
	if (input >= 0)
		rangedValue = FMath::Lerp(0.5, 1, input);
	else
		rangedValue = FMath::Lerp(0.5, 0, FMath::Abs(input));
	if (rangedValue == 1)
		rangedValue -= 0.0000000001;
	else if (rangedValue == -1)
		rangedValue += 0.0000000001;
	double scaledUpValue = rangedValue * FMathExtension::TenPowX(precision);
	int realPart = scaledUpValue;
	return realPart;
}

double UInputTranscoderConfig::FromXDigitFloatingPoint(const double input, const double precision) const
{
	double divider = FMathExtension::TenPowX(precision);
	double scaledDownValue = input / divider;
	int realPart = scaledDownValue;
	double alpha = scaledDownValue - (double)realPart;
	double remaped = (alpha - 0.5) * 2;
	return remaped;
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
	const double multiplierAtLastIndex = FMathExtension::TenPowX(lastIndex);
	double valueAtLastIndex = ((lastIndex >= DIGITS_DOUBLE_COUNT) ? 0 : serializedArray / multiplierAtLastIndex);
	valueAtLastIndex = (long)(valueAtLastIndex);
	valueAtLastIndex *= FMathExtension::TenPowX(digitCount);

	//this Index
	const double multiplierIndex = FMathExtension::TenPowX(serializedIndex);
	double valueAtIndex = serializedArray / multiplierIndex;
	valueAtIndex = (long)(valueAtIndex);

	double absoluteValue = valueAtIndex - valueAtLastIndex;
	if (absoluteValue < 0)
		absoluteValue *= -1;
	double result = FromXDigitFloatingPoint(absoluteValue, digitCount);
	return result;
}

bool UInputTranscoderConfig::SerializeValueAtIndex(double& serializedArray, int index, double val, int digitCount)
{
	if (val > 1 || val < -1)
		return false;
	if (digitCount <= 0)
		return false;
	if (index < 0 || index >= (DIGITS_DOUBLE_COUNT / digitCount))
		return false;
	const int serializedIndex = index * digitCount;
	//Last Index
	const int lastIndex = FMath::Clamp(serializedIndex + digitCount, 0, DIGITS_DOUBLE_COUNT);
	const double multiplierAtLastIndex = FMathExtension::TenPowX(lastIndex);
	unsigned long valueAtLastIndex = (lastIndex >= DIGITS_DOUBLE_COUNT ? 0 : serializedArray / multiplierAtLastIndex);
	valueAtLastIndex *= FMathExtension::TenPowX(lastIndex);
	//Next Index
	const int nextIndex = FMath::Clamp(serializedIndex - digitCount, 0, DIGITS_DOUBLE_COUNT);
	const double multiplierAtNextIndex = FMathExtension::TenPowX(serializedIndex);
	const double tempNextVal = serializedArray / multiplierAtNextIndex;
	const unsigned long valueAtNextIndex = (tempNextVal - ((int)tempNextVal)) * multiplierAtNextIndex;
	//this Index
	const double xDigitVal = ToXDigitFloatingPoint(val, digitCount);
	const double multiplierIndex = FMathExtension::TenPowX(serializedIndex);
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
	const bool writeRes = SerializeValueAtIndex(encodingInput.valuesCode, indexInList, val);
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
