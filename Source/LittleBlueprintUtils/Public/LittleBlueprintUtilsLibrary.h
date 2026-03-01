// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "LittleBlueprintUtilsLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE(FFinishForEachSignature);

UCLASS()
class ULittleBlueprintUtilsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

    /**
     * Searches the specified array for an element that satisfies the given predicate function.
     *
     * Predicate signature:
     *
     * bool YourPredicate(ArrayType ElementToSearch);
     *
     * @param Array             The array to search through (wildcard type).
     * @param PredicateOwner    The UObject instance owning the predicate function.
     * @param PredicateName     The name of the predicate function to call on each element.
     * @param Result            The first matching element is copied here (wildcard type).
     * @param Index             Index of the matching element; -1 if not found.
     */
    UFUNCTION(BlueprintPure, CustomThunk, Category = "Utilities|Array",
        meta = (
            DisplayName = "Search by Predicate",
            KeyWords = "Find by Predicate",
            CompactNodeTitle = "SEARCH",
            ArrayParm = "Array",
            ArrayTypeDependentParams = "Result",
            AutoCreateRefTerm = "PredicateName",
            HidePin = "WorldContextObject",
            DefaultToSelf = "PredicateOwner"
        )
    )
    static void ArrayFindByPredicate(
        const TArray<int32>& Array,
        UObject* PredicateOwner,
        const FName& PredicateName,
        int32& Result,
        int32& Index)
    {
        checkNoEntry();
        return;
    }
    DECLARE_FUNCTION(execArrayFindByPredicate);

    UFUNCTION(BlueprintPure, CustomThunk, Category = "Utilities|Array",
        meta = (
            DisplayName = "Filter by Predicate",
            CompactNodeTitle = "FILTER",
            ArrayParm = "Array",
            ArrayTypeDependentParams = "Result",
            AutoCreateRefTerm = "PredicateName",
            HidePin = "WorldContextObject",
            DefaultToSelf = "PredicateOwner"
            )
    )
    static void ArrayFilterByPredicate(
        const TArray<int32>& Array,
        UObject* PredicateOwner,
        const FName& PredicateName,
        TArray<int32>& Result)
    {
        checkNoEntry();
        return;
    }
    DECLARE_FUNCTION(execArrayFilterByPredicate);

private:
    static int32 GenericArrayFindByPredicate(
        void* Array,
        const FArrayProperty* ArrayProperty,
        UObject* PredicateOwner,
        UFunction* Predicate,
        void* Result,
        FProperty* ResultProperty);

    static void GenericArrayFilterByPredicate(
        void* Array,
        const FArrayProperty* ArrayProperty,
        UObject* PredicateOwner,
        UFunction* Predicate,
        void* Result,
        FArrayProperty* ResultProperty);

    static UFunction* FindPredicate(
        const UObject* PredicateOwner,
        const FName& PredicateName);

    static bool GetPredicateReturnValue(
        const UFunction* Predicate,
        const void* PredicateArgs);
};
