#include "ToolsLibrary.h"


UToolsLibrary::UToolsLibrary()
{
}


int UToolsLibrary::BoolArrayToFlag(const TArray<bool> array)
{
	//Translate to int
	TArray<int> arr;
	for (int i = 0; i < array.Num(); i++)
		arr.Add(array[i] ? 1 : 0);

	int result = 0;
	for (int i = 0; i < arr.Num(); i++)
	{
		result += arr[i] * TwoPowX(i);
	}
	return result;
}


TArray<bool> UToolsLibrary::FlagToBoolArray(int flag)
{
	TArray<int> binary_array;
	TArray<bool> bool_array;
	int n = flag;

	while (n > 0)
	{
		binary_array.Add(n % 2);
		n /= 2;
	}
	for (int i = binary_array.Num() - 1; i >= 0; i--)
		bool_array.Add(binary_array[binary_array.Num() - (i + 1)] >= 1);

	return bool_array;
}


TArray<int> UToolsLibrary::BoolToIndexesArray(const TArray<bool> array)
{
	TArray<int> indexArray;
	//loop the array and add to the int[] i if array[i] == true
	for (int i = 0; i < array.Num(); i++)
		if (array[i])
			indexArray.Add(i);
	//return int[]
	return indexArray;
}


TArray<bool> UToolsLibrary::IndexesToBoolArray(const TArray<int> array)
{
	//Get the highest index
	int length = array.Max();
	//set the bool[] size
	TArray<bool> bools;
	for (int i = 0; i < length; i++)
		bools.Add(false);
	//loop the array and true at bool[x], where x = arr[i]
	for (int i = 0; i < array.Num(); i++)
	{
		if (!bools.IsValidIndex(array[i]))
			continue;
		bools[array[i]] = true;
	}

	//return bool[]
	return bools;
}


int UToolsLibrary::IndexToFlag(const int index)
{
	if(index < 0)
		return 0;
	return TwoPowX(index);
}


double UToolsLibrary::TenPowX(const int exponent)
{
	if (exponent <= 0)
		return 1;
	if (exponent == 1)
		return 10;
	double result = 10;
	for (int i = 1; i < exponent; i++)
		result *= 10;
	return result;
}

double UToolsLibrary::TwoPowX(const int exponent)
{
	if (exponent <= 0)
		return 1;
	if (exponent == 1)
		return 2;
	double result = 2;
	for (int i = 1; i < exponent; i++)
		result *= 2;
	return result;
}

FString UToolsLibrary::DebugBoolArray(TArray<bool> array)
{
	FString result = FString::Printf(TEXT("{"));

	for (int i = 0; i < array.Num(); i++)
	{
		if (i > 0)
			result.Append(FString::Printf(TEXT(",")));
		result.Append(FString::Printf(TEXT("%d"), array[i]));
	}

	result.Append(FString::Printf(TEXT("}")));
	return  result;
}

double UToolsLibrary::GetFPS(double deltaTime)
{
	return 1 / deltaTime;
}


template<typename InElementType> void UToolsLibrary::MatchArraySizesToLargest(TArray<InElementType>& arrayA, TArray<InElementType>& arrayB)
{
	if (arrayA.Num() == arrayB.Num())
		return;
	if (arrayA.Num() > arrayB.Num())
	{
		for (int i = arrayB.Num(); i < arrayA.Num(); i++)
		{
			arrayB.Add({});
		}
	}
	else
	{
		for (int i = arrayA.Num(); i < arrayB.Num(); i++)
		{
			arrayA.Add({});
		}
	}
}

