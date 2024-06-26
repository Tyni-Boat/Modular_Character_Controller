
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

	// Convert from a bool array to an integer. useful to serialize indexes in an array.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static int BoolArrayToInt(const TArray<bool> array);

	// Convert from an in to an bool array. useful to deserialize indexes in an array.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<bool> IntToBoolArray(int integer);

	// Convert a bool array to an index array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<int> BoolToIndexesArray(const TArray<bool> array);

	// Convert an int array of indexes to an bool array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static TArray<bool> IndexesToBoolArray(const TArray<int> array);

	// Returns a power of ten for positive values only.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools", meta = (CompactNodeTitle = "10powX"))
	static double TenPowX(const int exponent);

	// Returns a power of two for positive values only.
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools", meta = (CompactNodeTitle = "2powX"))
	static double TwoPowX(const int exponent);

	// Debug a boolean array
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static FString DebugBoolArray(TArray<bool> array);

	// Get the number of frame per seconds from a delta time
	UFUNCTION(BlueprintCallable, Category = "Function Library | Tools")
	static double GetFPS(double deltaTime);


	// Match two arrays to the greatest.
	template<typename InElementType> static void MatchArraySizesToLargest(TArray<InElementType>& arrayA, TArray<InElementType>& arrayB);

};

