// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "SSGameViewportClient.generated.h"

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API USSGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()
	
public:
    UPROPERTY(EditAnywhere, Category = "PIP")
    FVector2D PIPSize = FVector2D(0.30f, 0.30f);

    UPROPERTY(EditAnywhere, Category = "PIP")
    FVector2D PIPOrigin = FVector2D(0.68f, 0.02f);

    virtual void LayoutPlayers() override;
};
