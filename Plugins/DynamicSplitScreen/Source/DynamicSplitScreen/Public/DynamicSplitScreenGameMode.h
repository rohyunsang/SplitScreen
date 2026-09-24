// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DynamicSplitScreenGameMode.generated.h"

class ADynamicSplitScreenCameraProxy;

/**
 * Base GameMode for online (listen server) two-player split screen.
 *
 * Spawns the camera proxies that replicate each player's camera:
 *   - one server proxy for the host's camera
 *   - one client proxy per remote player (owned by that player's PlayerController)
 * and enables split screen on the host once two players are connected.
 * Clients enable it themselves in ADynamicSplitScreenPlayerController::BeginPlay.
 *
 * Usage: inherit your GameMode from ADynamicSplitScreenGameMode
 * (and your PlayerController from ADynamicSplitScreenPlayerController).
 */
UCLASS()
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** If true, split screen is enabled automatically on the host when the second player joins. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	bool bAutoEnableSplitScreen = true;

	/** Proxy class used to replicate player cameras. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen")
	TSubclassOf<ADynamicSplitScreenCameraProxy> CameraProxyClass;

private:
	void CreateCameraProxiesForPlayer(APlayerController* NewPlayer);
	void TryEnableSplitScreen();

	/** Proxy of the listen server's own camera */
	UPROPERTY(Transient)
	TObjectPtr<ADynamicSplitScreenCameraProxy> ServerCameraProxy;

	/** Proxies of remote clients' cameras */
	UPROPERTY(Transient)
	TMap<TObjectPtr<APlayerController>, TObjectPtr<ADynamicSplitScreenCameraProxy>> ClientCameraProxies;
};
