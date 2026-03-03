// Copyright Epic Games, Inc. All Rights Reserved.


#include "LittleBlueprintUtilsLibrary.h"


DEFINE_FUNCTION(ULittleBlueprintUtilsLibrary::execArrayFindByPredicate)
{
    Stack.MostRecentProperty = nullptr;
    Stack.StepCompiledIn<FArrayProperty>(NULL);
    void* Array = Stack.MostRecentPropertyAddress;
    auto* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
    if (!ArrayProperty)
    {
        Stack.bArrayContextFailed = true;
        return;
    }

    PARAM_PASSED_BY_VAL(PredicateOwner, FObjectProperty, UObject*);
    PARAM_PASSED_BY_REF(PredicateName, FNameProperty, FName);

    Stack.MostRecentPropertyAddress = nullptr;
    Stack.MostRecentPropertyContainer = nullptr;
    Stack.StepCompiledIn<FProperty>(NULL);
    void* Result = Stack.MostRecentPropertyAddress;
    auto* ResultProperty = CastField<FProperty>(Stack.MostRecentProperty);

    PARAM_PASSED_BY_REF(Index, FIntProperty, int32);

    P_FINISH;

    P_NATIVE_BEGIN;
    auto* Predicate = FindPredicate(PredicateOwner, PredicateName);
    Index = GenericArrayFindByPredicate(
        Array,
        ArrayProperty,
        PredicateOwner,
        Predicate,
        Result,
        ResultProperty);
    P_NATIVE_END;
}

DEFINE_FUNCTION(ULittleBlueprintUtilsLibrary::execArrayFilterByPredicate)
{
    Stack.MostRecentProperty = nullptr;
    Stack.StepCompiledIn<FArrayProperty>(NULL);
    void* Array = Stack.MostRecentPropertyAddress;
    auto* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
    if (!ArrayProperty)
    {
        Stack.bArrayContextFailed = true;
        return;
    }

    PARAM_PASSED_BY_VAL(PredicateOwner, FObjectProperty, UObject*);
    PARAM_PASSED_BY_REF(PredicateName, FNameProperty, FName);

    Stack.MostRecentPropertyAddress = nullptr;
    Stack.MostRecentPropertyContainer = nullptr;
    Stack.StepCompiledIn<FArrayProperty>(NULL);
    void* Result = Stack.MostRecentPropertyAddress;
    auto* ResultProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
    if (!ResultProperty)
    {
        Stack.bArrayContextFailed = true;
        return;
    }

    PARAM_PASSED_BY_REF(Index, FIntProperty, int32);

    P_FINISH;

    P_NATIVE_BEGIN;
    auto* Predicate = FindPredicate(PredicateOwner, PredicateName);
    GenericArrayFilterByPredicate(
        Array,
        ArrayProperty,
        PredicateOwner,
        Predicate,
        Result,
        ResultProperty);
    P_NATIVE_END;
}

int32 ULittleBlueprintUtilsLibrary::GenericArrayFindByPredicate(
	void* Array,
	const FArrayProperty* ArrayProperty,
    UObject* PredicateOwner,
    UFunction* Predicate,
	void* Result,
    FProperty* ResultProperty)
{
	if (!Array || !PredicateOwner || !Predicate || !Result)
	{
        UE_LOG(LogTemp, Warning, TEXT("GenericArrayFindByPredicate: any args is nullptr"));
        return INDEX_NONE;
	}

	FScriptArrayHelper ArrayHelper{ArrayProperty, Array};
	const FProperty* InnerProp = ArrayProperty->Inner;

	for (int32 Idx = 0; Idx < ArrayHelper.Num(); Idx++)
    {
        // Allocate a parameter buffer. This is necessary for using ProcessEvent, as it clears the passed Params.
        void* Params = FMemory::Malloc(Predicate->ParmsSize);
        FMemory::Memzero(Params, Predicate->ParmsSize);

        // Find input param in Predicate
        FProperty* InputParam = nullptr;
        for (TFieldIterator<FProperty> It(Predicate); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_OutParm) && InnerProp->SameType(*It))
            {
                InputParam = *It;
                break;
            }
        }
        if (!InputParam)
        {
            UE_LOG(LogTemp, Error, TEXT("GenericArrayFindByPredicate: Can not find in Predicate '%s' input param type '%s'"),
                *Predicate->GetFName().ToString(),
                *InnerProp->GetCPPType());
            return INDEX_NONE;
        }

        void* DestPtr = (uint8*)Params + InputParam->GetOffset_ForUFunction();
        InputParam->CopyCompleteValue(DestPtr, ArrayHelper.GetRawPtr(Idx));
        check(DestPtr);
        check(Params);

        PredicateOwner->ProcessEvent(Predicate, Params);
        if (GetPredicateReturnValue(Predicate, Params))
        {
            ResultProperty->CopyCompleteValue(Result, ArrayHelper.GetRawPtr(Idx));
            FMemory::Free(Params);
			return Idx;
        }
        FMemory::Free(Params);
	}
	return INDEX_NONE;
}

void ULittleBlueprintUtilsLibrary::GenericArrayFilterByPredicate(
    void* Array,
    const FArrayProperty* ArrayProperty,
    UObject* PredicateOwner,
    UFunction* Predicate,
    void* Result,
    FArrayProperty* ResultProperty)
{
    if (!Array || !PredicateOwner || !Predicate || !Result)
    {
        UE_LOG(LogTemp, Warning, TEXT("GenericArrayFilterByPredicate: any args is nullptr"));
        return;
    }

    FScriptArrayHelper ArrayHelper(ArrayProperty, Array);
    FScriptArrayHelper ResultHelper(ResultProperty, Result);
    const FProperty* InnerProp = ArrayProperty->Inner;

    for (int32 Idx = 0; Idx < ArrayHelper.Num(); Idx++)
    {
        // Allocate a parameter buffer. This is necessary for using ProcessEvent, as it clears the passed Params.
        void* Params = FMemory::Malloc(Predicate->ParmsSize);
        FMemory::Memzero(Params, Predicate->ParmsSize);

        // Find input param in Predicate
        FProperty* InputParam = nullptr;
        for (TFieldIterator<FProperty> It(Predicate); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_Parm)
                && !It->HasAnyPropertyFlags(CPF_OutParm)
                && InnerProp->SameType(*It))
            {
                InputParam = *It;
                break;
            }
        }
        if (!InputParam)
        {
            UE_LOG(LogTemp, Error, TEXT("GenericArrayFilterByPredicate: Can not find in Predicate '%s' input param type '%s'"),
                *Predicate->GetFName().ToString(),
                *InnerProp->GetCPPType());
            return;
        }

        void* DestPtr = (uint8*)Params + InputParam->GetOffset_ForUFunction();
        InputParam->CopyCompleteValue(DestPtr, ArrayHelper.GetRawPtr(Idx));
        check(DestPtr);
        check(Params);

        PredicateOwner->ProcessEvent(Predicate, Params);
        if (GetPredicateReturnValue(Predicate, Params))
        {
            const int32 ValueIdx = ResultHelper.AddValue();
            DestPtr = ResultHelper.GetRawPtr(ValueIdx);
            const void* Src = (uint8*)Params + InputParam->GetOffset_ForUFunction();
            InputParam->CopyCompleteValue(DestPtr, Src);
        }
        FMemory::Free(Params);
    }
}

UFunction* ULittleBlueprintUtilsLibrary::FindPredicate(
    const UObject* PredicateOwner,
    const FName& PredicateName)
{
    if (!PredicateOwner)
    {
#if WITH_EDITOR
        UE_LOG(LogTemp, Error, TEXT("FindByPredicate: PredicateName '%s', PredicateOwner is not valid"),
            *PredicateName.ToString());
#endif
        return nullptr;
    }

    if (PredicateName.IsNone())
    {
#if WITH_EDITOR
        UE_LOG(LogTemp, Error, TEXT("FindByPredicate: PredicateOwner '%s', PredicateName is empty"),
            *PredicateOwner->GetFName().ToString());
#endif
        return nullptr;
    }
    auto* Predicate = PredicateOwner->FindFunction(PredicateName);
    if (!Predicate)
    {
#if WITH_EDITOR
        UE_LOG(LogTemp, Error, TEXT("FindByPredicate: PredicateOwner '%s', PredicateName %s not found"),
            *PredicateOwner->GetFName().ToString(),
            *PredicateName.ToString());
#endif
        return nullptr;
    }
    return Predicate;
}

bool ULittleBlueprintUtilsLibrary::GetPredicateReturnValue(
    const UFunction* Predicate,
    const void* PredicateArgs)
{
    if (!Predicate || !PredicateArgs)
    {
        return false;
    }

    for (TFieldIterator<FProperty> It(Predicate); It; ++It)
    {
        FProperty* Property = *It;
        if (Property && Property->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
        {
            if (const FBoolProperty* ReturnProperty = CastField<FBoolProperty>(Property))
            {
                return ReturnProperty->GetPropertyValue_InContainer(PredicateArgs);
            }
        }
    }
    return false;
}
