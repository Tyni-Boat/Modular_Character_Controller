
#pragma once
#include "CoreMinimal.h"
#include "ToolsLibrary.generated.h"



//Base types function library
UCLASS(BlueprintType)
class MODULARCONTROLLER_API UToolsLibrary : public UObject
{
	GENERATED_BODY()

public:

	UToolsLibrary();

	// Convert from a bool array to an integer flag. useful to serialize indexes in an array.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static int BoolArrayToFlag(const TArray<bool> array);

	// Convert from an integer flag to an bool array. useful to deserialize indexes in an array.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<bool> FlagToBoolArray(int flag);

	// Convert a bool array to an index array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<int> BoolToIndexesArray(const TArray<bool> array);

	// Convert an int array of indexes to an bool array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<bool> IndexesToBoolArray(const TArray<int> array);

	// Convert an index to integer flag.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static int IndexToFlag(const int index);

	// Returns a power of ten for positive values only.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools", meta = (CompactNodeTitle = "10powX"))
	static double TenPowX(const int exponent);

	// Returns a power of two for positive values only.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools", meta = (CompactNodeTitle = "2powX"))
	static double TwoPowX(const int exponent);

	// Restrict a vector inside a cone defined by normal and angle in degrees.
	UFUNCTION(BlueprintPure, Category = "Function Library | Tools")
	static FVector VectorCone(const FVector inVector, const FVector normal, const float alphaAngle = 90);

	// Check if a vector is inside a cone defined by normal and angle in degrees.
	UFUNCTION(BlueprintPure, Category = "Function Library | Tools")
	static bool IsVectorCone(const FVector inVector, const FVector normal, const float alphaAngle = 90);
	
	// Check a vector range against a value and return true if it's in range (Min <= Val < Max). nanIsTrue define the result when either the range or the value is NAN
	UFUNCTION(BlueprintPure, Category = "Function Library | Tools")
	static bool CheckInRange(const FVector2D range, const float value, bool nanIsTrue = false);

	// Debug a boolean array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static FString DebugBoolArray(TArray<bool> array);

	// Get the number of frame per seconds from a delta time
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static double GetFPS(double deltaTime);


	// Match two arrays to the greatest.
	template<typename InElementType> static void MatchArraySizesToLargest(TArray<InElementType>& arrayA, TArray<InElementType>& arrayB);

};

