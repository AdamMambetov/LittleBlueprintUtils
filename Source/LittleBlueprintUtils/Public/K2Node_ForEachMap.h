// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_ForEachMap.generated.h"


UCLASS()
class LITTLEBLUEPRINTUTILS_API UK2Node_ForEachMap : public UK2Node
{
	GENERATED_BODY()
	
protected:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual void AllocateDefaultPins() override;
	
	virtual void PostReconstructNode() override;

	virtual FText GetMenuCategory() const override;

	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	virtual void ExpandNode(
		class FKismetCompilerContext& CompilerContext,
		UEdGraph* SourceGraph) override;

	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
};
