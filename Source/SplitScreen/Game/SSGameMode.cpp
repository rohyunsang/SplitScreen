// SSGameMode.cpp

#include "Game/SSGameMode.h"
#include "Player/SSPlayerController.h"
#include "Pawn/PartnerSpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"

ASSGameMode::ASSGameMode()
{
    PlayerControllerClass = ASSPlayerController::StaticClass();
    SpectatorClass = APartnerSpectatorPawn::StaticClass();

    bUseSeamlessTravel = false;
    bPauseable = true;
    bStartPlayersAsSpectators = false;
}

void ASSGameMode::BeginPlay()
{
    Super::BeginPlay();

    ConnectedPlayers.Empty();

    // 리슨서버 호스트도 목록에 보장 추가
    if (APlayerController* PC0 = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ASSPlayerController* SS0 = Cast<ASSPlayerController>(PC0))
        {
            ConnectedPlayers.AddUnique(SS0);
        }
    }

    TryAutoPair();
}

void ASSGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (ASSPlayerController* SS = Cast<ASSPlayerController>(NewPlayer))
    {
        ConnectedPlayers.AddUnique(SS);
        TryAutoPair();
    }
}

void ASSGameMode::Logout(AController* Exiting)
{
    if (ASSPlayerController* SS = Cast<ASSPlayerController>(Exiting))
    {
        // 떠나는 쪽 정리
        ClearPartner(SS);

        // 나를 물고 있는 상대도 정리
        for (ASSPlayerController* Other : ConnectedPlayers)
        {
            if (Other && Other->GetPartner() == SS)
            {
                ClearPartner(Other);
            }
        }

        ConnectedPlayers.Remove(SS);
    }

    Super::Logout(Exiting);

    // 남은 인원에 맞춰 재매칭
    TryAutoPair();
}

bool ASSGameMode::IsPaired(const ASSPlayerController* PC)
{
    return (PC && PC->GetPartner() != nullptr);
}

void ASSGameMode::ClearPartner(ASSPlayerController* PC)
{
    if (!PC) return;
    PC->SetPartner(nullptr);          // 서버에서만 호출(복제됨)
    PC->ClientDisablePartnerView();   // 로컬 PIP 닫기
}

void ASSGameMode::TryAutoPair()
{
    // 페어 아님 두 명 뽑기
    ASSPlayerController* A = nullptr;
    ASSPlayerController* B = nullptr;

    for (ASSPlayerController* PC : ConnectedPlayers)
    {
        if (PC && !IsPaired(PC))
        {
            if (!A) A = PC;
            else if (!B) { B = PC; break; }
        }
    }

    if (A && B)
    {
        // 서로 파트너 지정(Replicated)
        A->SetPartner(B);
        B->SetPartner(A);

        // 각자 로컬에서 PIP 생성(클라/호스트 모두)
        A->ClientEnablePartnerView();
        B->ClientEnablePartnerView();

        if (GEngine)
        {
            const FString AName = (A->PlayerState ? A->PlayerState->GetPlayerName() : TEXT("A"));
            const FString BName = (B->PlayerState ? B->PlayerState->GetPlayerName() : TEXT("B"));
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
                FString::Printf(TEXT("Auto-paired: %s <-> %s"), *AName, *BName));
        }
    }
}
