// Fill out your copyright notice in the Description page of Project Settings.

#include "K2Node_ForEachMap.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "KismetCompiler.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintMapLibrary.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"


FText UK2Node_ForEachMap::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromString(TEXT("For Each Loop"));
}

void UK2Node_ForEachMap::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	UEdGraphNode::FCreatePinParams MapParams{};
	MapParams.ContainerType = EPinContainerType::Map;
	MapParams.ValueTerminalType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
	MapParams.ValueTerminalType.TerminalSubCategory = TEXT("");
	MapParams.ValueTerminalType.TerminalSubCategoryObject = nullptr;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, TEXT("Map"), MapParams);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Loop);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT("Key"));
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, TEXT("Value"));
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Completed);
}

void UK2Node_ForEachMap::PostReconstructNode()
{
	// cannot use a ranged for here, as PinConnectionListChanged() might end up 
	// collapsing split pins (subtracting elements from Pins)
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		if (Pin->LinkedTo.Num() > 0)
		{
			PinConnectionListChanged(Pin);
		}
	}

	Super::PostReconstructNode();
}

FText UK2Node_ForEachMap::GetMenuCategory() const
{
	return FText::FromString(TEXT("Map For Each Loop"));
}

void UK2Node_ForEachMap::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions(ActionRegistrar);

	if (ActionRegistrar.IsOpenForRegistration(GetClass()))
	{
		auto* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		ActionRegistrar.AddBlueprintAction(GetClass(), Spawner);
	}
}

void UK2Node_ForEachMap::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UEdGraphPin* MapPin			= FindPinChecked(TEXT("Map"));
	UEdGraphPin* LoopPin		= FindPinChecked(UEdGraphSchema_K2::PN_Loop);
	UEdGraphPin* KeyPin			= FindPinChecked(TEXT("Key"));
	UEdGraphPin* ValuePin		= FindPinChecked(TEXT("Value"));
	UEdGraphPin* CompletedPin	= FindPinChecked(UEdGraphSchema_K2::PN_Completed);

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	bool bResult = true;

	if (MapPin->PinType.ContainerType != EPinContainerType::Map ||
	    MapPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard ||
	    MapPin->PinType.PinSubCategory == UEdGraphSchema_K2::PC_Wildcard ||
	    MapPin->PinType.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
	    CompilerContext.MessageLog.Error(*FString::Printf(
	        TEXT("Map pin type is undetermined or wildcard. Please connect a typed map pin to node %s."),
	        *GetNameSafe(this)));
	    BreakAllNodeLinks();
	    return;
	}

	// Create int Map Index
	auto* MapIndexNode = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
	MapIndexNode->VariableType.PinCategory = UEdGraphSchema_K2::PC_Int;
	MapIndexNode->AllocateDefaultPins();
	UEdGraphPin* MapIndexPin = MapIndexNode->GetVariablePin();
	check(MapIndexPin);

	// Initialize Map index
	auto* MapIndexInitialize = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	MapIndexInitialize->AllocateDefaultPins();
	MapIndexInitialize->GetValuePin()->DefaultValue = TEXT("0");
	bResult &= Schema->TryCreateConnection(MapIndexPin, MapIndexInitialize->GetVariablePin());
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *MapIndexInitialize->GetExecPin());
	//UEdGraphPin* StartLoopExecInPin = ArrayIndexInitialize->GetExecPin();

	// Get Map Keys
	auto* MapKeys = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName MapKeysFuncName = GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Keys);
	MapKeys->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(MapKeysFuncName));
	MapKeys->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(MapIndexInitialize->GetThenPin(), MapKeys->GetExecPin());
	
	UEdGraphPin* KeysTargetMapPin = MapKeys->FindPinChecked(TEXT("TargetMap"));
	UEdGraphPin* MapKeysPin = MapKeys->FindPinChecked(TEXT("Keys"));
	 
	// ВАЖНО: явно копируем PinType
	KeysTargetMapPin->PinType = MapPin->PinType;
	MapKeysPin->PinType.PinCategory = MapPin->PinType.PinCategory;
	MapKeysPin->PinType.PinSubCategory = MapPin->PinType.PinSubCategory;
	MapKeysPin->PinType.PinSubCategoryObject = MapPin->PinType.PinSubCategoryObject;

	CompilerContext.CopyPinLinksToIntermediate(*MapPin, *KeysTargetMapPin);

	
	// Get Map Values
	auto* MapValues = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName MapValuesFuncName = GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Values);
	MapValues->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(MapValuesFuncName));
	MapValues->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(MapKeys->GetThenPin(), MapValues->GetExecPin());
	
	UEdGraphPin* MapValuesTargetMapPin = MapValues->FindPinChecked(TEXT("TargetMap"));
	UEdGraphPin* MapValuesPin = MapValues->FindPinChecked(TEXT("Values"));
	 
	// ВАЖНО: явно копируем PinType
	MapValuesTargetMapPin->PinType = MapPin->PinType;
	MapValuesPin->PinType.PinCategory = MapPin->PinType.PinValueType.TerminalCategory;
	MapValuesPin->PinType.PinSubCategory = MapPin->PinType.PinValueType.TerminalSubCategory;
	MapValuesPin->PinType.PinSubCategoryObject = MapPin->PinType.PinValueType.TerminalSubCategoryObject;

	CompilerContext.CopyPinLinksToIntermediate(*MapPin, *MapValuesTargetMapPin);

	// Do loop branch
	auto* Branch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	Branch->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(MapValues->GetThenPin(), Branch->GetExecPin());
	// bResult &= Schema->TryCreateConnection(MapIndexInitialize->GetThenPin(), Branch->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*CompletedPin, *Branch->GetElsePin());

	// Do loop condition
	auto* Condition = CompilerContext
		.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName LessFuncName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Less_IntInt);
	Condition->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(LessFuncName));
	Condition->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Condition->GetReturnValuePin(), Branch->GetConditionPin());
	bResult &= Schema->TryCreateConnection(Condition->FindPinChecked(TEXT("A")), MapIndexPin);
	// Condition->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("3");

	// Map Length
	auto* MapLength = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName MapLengthFuncName = GET_FUNCTION_NAME_CHECKED(UBlueprintMapLibrary, Map_Length);
	MapLength->SetFromFunction(UBlueprintMapLibrary::StaticClass()->FindFunctionByName(MapLengthFuncName));
	MapLength->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(MapLength->GetReturnValuePin(), Condition->FindPinChecked(TEXT("B")));
	
	UEdGraphPin* LengthTargetMapPin = MapLength->FindPinChecked(TEXT("TargetMap"));
	
	// ВАЖНО: явно копируем PinType, включая ContainerType и ключ/значение
	LengthTargetMapPin->PinType = MapPin->PinType;
	CompilerContext.CopyPinLinksToIntermediate(*MapPin, *LengthTargetMapPin);

	// body sequence
	auto* Sequence = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	Sequence->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Branch->GetThenPin(), Sequence->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*LoopPin, *Sequence->GetThenPinGivenIndex(0));

	// Get Map Key
	auto* GetKey = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName GetKeyFuncName = GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get);
	GetKey->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GetKeyFuncName));
	GetKey->AllocateDefaultPins();
	UEdGraphPin* GetKeyTargetArrayPin = GetKey->FindPinChecked(TEXT("TargetArray"));
	UEdGraphPin* GetKeyTargetArrayItemPin = GetKey->FindPinChecked(TEXT("Item"));
	
	// ВАЖНО: явно копируем PinType
	GetKeyTargetArrayPin->PinType.PinCategory = MapPin->PinType.PinCategory;
	GetKeyTargetArrayPin->PinType.PinSubCategory = MapPin->PinType.PinSubCategory;
	GetKeyTargetArrayPin->PinType.PinSubCategoryObject = MapPin->PinType.PinSubCategoryObject;
	GetKeyTargetArrayItemPin->PinType.PinCategory = MapPin->PinType.PinCategory;
	GetKeyTargetArrayItemPin->PinType.PinSubCategory = MapPin->PinType.PinSubCategory;
	GetKeyTargetArrayItemPin->PinType.PinSubCategoryObject = MapPin->PinType.PinSubCategoryObject;

	bResult &= Schema->TryCreateConnection(MapKeysPin, GetKeyTargetArrayPin);
	if (!bResult)
	{
		UE_LOG(LogTemp, Error, TEXT("MapKeysPin error"));
		BreakAllNodeLinks();
		return;
	}
	bResult &= Schema->TryCreateConnection(GetKey->FindPinChecked(TEXT("Index")), MapIndexPin);
	if (!bResult)
	{
		UE_LOG(LogTemp, Error, TEXT("MapKeysIndexPin error"));
		BreakAllNodeLinks();
		return;
	}
	CompilerContext.MovePinLinksToIntermediate(*KeyPin, *GetKeyTargetArrayItemPin);

	// Get Map Value
	auto* GetValue = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName GetValueFuncName = GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get);
	GetValue->SetFromFunction(UKismetArrayLibrary::StaticClass()->FindFunctionByName(GetValueFuncName));
	GetValue->AllocateDefaultPins();
	UEdGraphPin* GetValueTargetArrayPin = GetValue->FindPinChecked(TEXT("TargetArray"));
	UEdGraphPin* GetValueTargetArrayItemPin = GetValue->FindPinChecked(TEXT("Item"));
	
	// ВАЖНО: явно копируем PinType
	GetValueTargetArrayPin->PinType.PinCategory = MapPin->PinType.PinValueType.TerminalCategory;
	GetValueTargetArrayPin->PinType.PinSubCategory = MapPin->PinType.PinValueType.TerminalSubCategory;
	GetValueTargetArrayPin->PinType.PinSubCategoryObject = MapPin->PinType.PinValueType.TerminalSubCategoryObject;
	GetValueTargetArrayItemPin->PinType.PinCategory = MapPin->PinType.PinValueType.TerminalCategory;
	GetValueTargetArrayItemPin->PinType.PinSubCategory = MapPin->PinType.PinValueType.TerminalSubCategory;
	GetValueTargetArrayItemPin->PinType.PinSubCategoryObject = MapPin->PinType.PinValueType.TerminalSubCategoryObject;

	bResult &= Schema->TryCreateConnection(MapValuesPin, GetValueTargetArrayPin);
	if (!bResult)
	{
		UE_LOG(LogTemp, Error, TEXT("MapValuesPin error"));
		BreakAllNodeLinks();
		return;
	}
	bResult &= Schema->TryCreateConnection(GetValue->FindPinChecked(TEXT("Index")), MapIndexPin);
	if (!bResult)
	{
		UE_LOG(LogTemp, Error, TEXT("MapValuesIndexPin error"));
		BreakAllNodeLinks();
		return;
	}
	CompilerContext.MovePinLinksToIntermediate(*ValuePin, *GetValueTargetArrayItemPin);

	// Map Index increment
	auto* Increment = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	FName AddFuncName = GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Add_IntInt);
	Increment->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(AddFuncName));
	Increment->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(Increment->FindPinChecked(TEXT("A")), MapIndexPin);
	Increment->FindPinChecked(TEXT("B"))->DefaultValue = TEXT("1");

	// Loop Counter assigned
	auto* MapIndexIncrementAssign = CompilerContext
		.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
	MapIndexIncrementAssign->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(MapIndexIncrementAssign->GetExecPin(), Sequence->GetThenPinGivenIndex(1));
	bResult &= Schema->TryCreateConnection(MapIndexIncrementAssign->GetVariablePin(), MapIndexPin);
	bResult &= Schema->TryCreateConnection(MapIndexIncrementAssign->GetValuePin(), Increment->GetReturnValuePin());
	bResult &= Schema->TryCreateConnection(MapIndexIncrementAssign->GetThenPin(), Branch->GetExecPin());

	if (!bResult)
	{
		CompilerContext.MessageLog.Error(TEXT("Error!"));
	}
	
	BreakAllNodeLinks();
}

void UK2Node_ForEachMap::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	auto* MapPin = FindPinChecked(TEXT("Map"));
	auto* KeyPin = FindPinChecked(TEXT("Key"));
	auto* ValuePin = FindPinChecked(TEXT("Value"));

	MapPin->PinType.ContainerType = EPinContainerType::Map;
	
	if (Pin->PinName == MapPin->PinName)
	{
		if (MapPin->LinkedTo.IsEmpty())
		{
			if (KeyPin->LinkedTo.IsEmpty())
			{
				MapPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				MapPin->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Wildcard;
				MapPin->PinType.PinSubCategoryObject = nullptr;

				KeyPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				KeyPin->PinType.PinSubCategory = TEXT("");
	        	KeyPin->PinType.PinSubCategoryObject = nullptr;
			}
			if (ValuePin->LinkedTo.IsEmpty())
			{
				MapPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
				MapPin->PinType.PinValueType.TerminalSubCategory = TEXT("");
	        	MapPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;

				ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
				ValuePin->PinType.PinSubCategory = TEXT("");
	        	ValuePin->PinType.PinSubCategoryObject = nullptr;
			}
		}
		else
		{
			const FEdGraphPinType& LinkedType = MapPin->LinkedTo[0]->PinType;

			MapPin->PinType.PinCategory = LinkedType.PinCategory;
			MapPin->PinType.PinSubCategory = LinkedType.PinSubCategory;
			MapPin->PinType.PinSubCategoryObject = LinkedType.PinSubCategoryObject;
			MapPin->PinType.PinValueType.TerminalCategory = LinkedType.PinValueType.TerminalCategory;
			MapPin->PinType.PinValueType.TerminalSubCategory = LinkedType.PinValueType.TerminalSubCategory;
			MapPin->PinType.PinValueType.TerminalSubCategoryObject = LinkedType.PinValueType.TerminalSubCategoryObject;

			KeyPin->PinType.PinCategory = LinkedType.PinCategory;
	        KeyPin->PinType.PinSubCategory = LinkedType.PinSubCategory;
	        KeyPin->PinType.PinSubCategoryObject = LinkedType.PinSubCategoryObject;
			
			ValuePin->PinType.PinCategory = LinkedType.PinValueType.TerminalCategory;
	        ValuePin->PinType.PinSubCategory = LinkedType.PinValueType.TerminalSubCategory;
	        ValuePin->PinType.PinSubCategoryObject = LinkedType.PinValueType.TerminalSubCategoryObject;
		}
		GetGraph()->NotifyGraphChanged();
	}
	else if (Pin->PinName == KeyPin->PinName)
	{
		if (!KeyPin->LinkedTo.IsEmpty())
		{
			const FEdGraphPinType& LinkedType = KeyPin->LinkedTo[0]->PinType;

			MapPin->PinType.PinCategory = LinkedType.PinCategory;
			MapPin->PinType.PinSubCategory = LinkedType.PinSubCategory;
	        MapPin->PinType.PinSubCategoryObject = LinkedType.PinSubCategoryObject;
	        
			KeyPin->PinType.PinCategory = LinkedType.PinCategory;
	        KeyPin->PinType.PinSubCategory = LinkedType.PinSubCategory;
	        KeyPin->PinType.PinSubCategoryObject = LinkedType.PinSubCategoryObject;
		}
		else if (MapPin->LinkedTo.IsEmpty())
		{
			MapPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			MapPin->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Wildcard;
			MapPin->PinType.PinSubCategoryObject = nullptr;

			KeyPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			KeyPin->PinType.PinSubCategory = TEXT("");
			KeyPin->PinType.PinSubCategoryObject = nullptr;
		}
		GetGraph()->NotifyGraphChanged();
	}
	else if (Pin->PinName == ValuePin->PinName)
	{
		if (!ValuePin->LinkedTo.IsEmpty())
		{
			const FEdGraphPinType& LinkedType = ValuePin->LinkedTo[0]->PinType;
	 
			MapPin->PinType.PinValueType.TerminalCategory = LinkedType.PinCategory;
			MapPin->PinType.PinValueType.TerminalSubCategory = LinkedType.PinSubCategory;
			MapPin->PinType.PinValueType.TerminalSubCategoryObject = LinkedType.PinSubCategoryObject;

			ValuePin->PinType.PinCategory = LinkedType.PinCategory;
	        ValuePin->PinType.PinSubCategory = LinkedType.PinSubCategory;
	        ValuePin->PinType.PinSubCategoryObject = LinkedType.PinSubCategoryObject;
		}
		else if (MapPin->LinkedTo.IsEmpty())
		{
			MapPin->PinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_Wildcard;
			MapPin->PinType.PinValueType.TerminalSubCategory = TEXT("");
			MapPin->PinType.PinValueType.TerminalSubCategoryObject = nullptr;

			ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			ValuePin->PinType.PinSubCategory = TEXT("");
			ValuePin->PinType.PinSubCategoryObject = nullptr;
		}
		GetGraph()->NotifyGraphChanged();
	}
}
