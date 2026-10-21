#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LyudGameMenu.generated.h"

class SLyudGameMenu;
class SWidget;
class FLyudMenuInput;

/**
 * The game menu: the main menu on start-up of a packaged game, the pause menu on Esc (F10 in the editor,
 * where Esc stops PIE; Back on Android; a "Меню" button on phones). Pauses the game while open.
 * Lives in every game world; also tells ULyudSaveSubsystem when a world starts and ends.
 */
UCLASS()
class LYUDMILOVSK_API ULyudGameMenuSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& World) override;
	virtual void Deinitialize() override;

	/** Opens the main menu (bMain) or the pause menu. */
	void Open(bool bMain);
	void Close();
	bool IsOpen() const { return bOpen; }

	/** A key reached the game before anything else; true if the menu took it. */
	bool HandleKey(const FKey& Key);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	bool GameHasFocus() const;
	void RestoreGameInput();

	TSharedPtr<SLyudGameMenu> Menu;
	TSharedPtr<SWidget> MenuButton;
	TSharedPtr<FLyudMenuInput> Input;
	bool bOpen = false;
	bool bWasPaused = false;
};
