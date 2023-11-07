// Copyright © 2023 by Tyni Boat. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Structs.h"
#include "UObject/Object.h"
#include "InputTranscoderConfig.generated.h"


///<summary>
/// The input transcoder config file.
/// </summary>
UCLASS(BlueprintType, Blueprintable, ClassGroup = "Modular Action Behaviours", abstract)
class MODULARCONTROLLER_API UInputTranscoderConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Inputs")
	TArray<FNetInputPair> InputEntries;


	bool CheckInputValid(FName inputKey, FInputEntry inputEntry) const;

	UFUNCTION(BlueprintCallable, Category="Inputs")
	FTranscodedInput EncodeInputs(const FInputEntryPool& inputPool);

	UFUNCTION(BlueprintCallable, Category="Inputs")
	bool DecodeInputs(const FInputEntryPool& inputPool, FTranscodedInput encodedInputs);

private:
	const double DIGITS_DOUBLE_COUNT = 14;

	//Max 4 axis can be encoded
	TArray<FName> _axisEntries;
	//Max 8 values can be encoded
	TArray<FName> _valuesEntries;
	//Nearly an unlimited buttons can be encoded
	TArray<FName> _buttonEntries;
	bool _transcoderInitialized;

	void InitializeTranscoder();

	//Convert a number from 0-1 to a number from 0-10pow(precision)
	double ToXDigitFloatingPoint(double input, double precision);

	//Convert a number from 0-10pow(precision) to a number from 0-1
	double FromXDigitFloatingPoint(double input, double precision);

	//Get a value from a serialized array
	double DeserializeValueAtIndex(double serializedArray, int index, int digitCount = 2);

	//Set a value to a serialized array
	bool SerializeValueAtIndex(double& serializedArray, int index, double val, int digitCount = 2);

	FVector2D ReadEncodedAxis(FName axisName, FTranscodedInput encodedInput);
	float ReadEncodedValue(FName valueName, FTranscodedInput encodedInput);
	bool ReadEncodedButton(FName buttonName, FTranscodedInput encodedInput);

	bool WriteEncodedAxis(FName axisName, FVector2D axisVal, FTranscodedInput& encodingInput);
	bool WriteEncodedValue(FName valueName, float val, FTranscodedInput& encodingInput);
	bool WriteEncodedButton(FName buttonName, bool state, FTranscodedInput& encodingInput);
};
