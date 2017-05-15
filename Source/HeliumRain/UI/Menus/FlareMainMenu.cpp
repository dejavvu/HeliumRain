
#include "FlareMainMenu.h"
#include "../../Flare.h"
#include "../../Game/FlareGame.h"
#include "../../Game/FlareSaveGame.h"
#include "../../Player/FlareMenuPawn.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"


#define LOCTEXT_NAMESPACE "FlareMainMenu"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareMainMenu::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	Game = MenuManager->GetPC()->GetGame();
	SaveSlotToDelete = -1;
	
	// Build structure
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	.Padding(FMargin(0, AFlareMenuManager::GetMainOverlayHeight(), 0, 0))
	[
		SNew(SVerticalBox)
		
		// Save slots
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SaveBox, SHorizontalBox)
		]

		// Bottom bar
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Fill)
		[
			SNew(SBackgroundBlur)
			.BlurRadius(Theme.BlurRadius)
			.BlurStrength(Theme.BlurStrength)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0))
			[
				SNew(SVerticalBox)

				// Header
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(2)
					[
						SNew(SImage)
						.Image(&Theme.InvertedBrush)
						.ColorAndOpacity(Theme.ObjectiveColor)
					]
				]

				// Container
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.HAlign(HAlign_Center)
					.BorderImage(&Theme.BackgroundBrush)
					[
						SNew(SVerticalBox)

						// Beta warning
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(Theme.ContentPadding)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Beta", "Helium Rain is in beta ! You could encounter bugs or missing features. Please report issues at dev.helium-rain.com."))
							.TextStyle(&Theme.NameFont)
							.Justification(ETextJustify::Center)
						]

						// Buttons
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)

							// Credits
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							.AutoWidth()
							[
								SNew(SFlareButton)
								.Transparent(true)
								.Width(3)
								.Text(LOCTEXT("GameCredits", "Credits"))
								.OnClicked(this, &SFlareMainMenu::OnOpenCredits)
							]

							// EULA
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							[
								SNew(SFlareButton)
								.Transparent(true)
								.Width(7)
								.Text(LOCTEXT("GameEULA", "End-user Licence Agreement"))
								.OnClicked(this, &SFlareMainMenu::OnOpenEULA)
							]

							// Version
							+ SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.AutoWidth()
							.Padding(Theme.ContentPadding)
							[
								SNew(STextBlock)
								.Text(FText::Format(FText::FromString("HELIUM RAIN / Beta / Built on {0} at {1}"),
									FText::FromString(__DATE__),  // FString neded here
									FText::FromString(__TIME__))) // FString neded here
								.TextStyle(&Theme.TextFont)
							]
						]
					]
				]
			]
		]
	];

	// Add save slots
	for (int32 Index = 1; Index <= Game->GetSaveSlotCount(); Index++)
	{
		TSharedPtr<int32> IndexPtr(new int32(Index));

		SaveBox->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)

			// Slot N°
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.TitlePadding)
			[
				SNew(STextBlock)
				.TextStyle(&Theme.TitleFont)
				.Text(FText::Format(LOCTEXT("NumberFormat", "{0}/"), FText::AsNumber(Index)))
			]

			// Company emblem
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.ContentPadding)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SFlareMainMenu::GetSaveIcon, Index)
				]
			]

			// Description
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(&Theme.TextFont)
				.Text(this, &SFlareMainMenu::GetText, Index)
			]

			// Launch
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(SFlareButton)
				.Text(this, &SFlareMainMenu::GetButtonText, Index)
				.HelpText(LOCTEXT("StartGameInfo", "Start playing"))
				.Icon(this, &SFlareMainMenu::GetButtonIcon, Index)
				.OnClicked(this, &SFlareMainMenu::OnOpenSlot, IndexPtr)
				.Width(5)
				.Height(1)
			]

			// Delete
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoHeight()
			.Padding(Theme.SmallContentPadding)
			[
				SNew(SFlareButton)
				.Text(LOCTEXT("Delete", "Delete game"))
				.HelpText(LOCTEXT("DeleteInfo", "Delete this game, forever, without backup !"))
				.Icon(FFlareStyleSet::GetIcon("Delete"))
				.OnClicked(this, &SFlareMainMenu::OnDeleteSlot, IndexPtr)
				.Width(5)
				.Height(1)
				.Visibility(this, &SFlareMainMenu::GetDeleteButtonVisibility, Index)
			]
		];
	}
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareMainMenu::Setup()
{
	SetEnabled(false);
	SetVisibility(EVisibility::Collapsed);
}

void SFlareMainMenu::Enter()
{
	FLOG("SFlareMainMenu::Enter");

	Game->UnloadGame();
	Game->ReadAllSaveSlots();
	
	MenuManager->GetPC()->GetSoundManager()->RequestMusicTrack(EFlareMusicTrack::Menu);
	
	SetEnabled(true);
	SetVisibility(EVisibility::Visible);
}

void SFlareMainMenu::Exit()
{
	SetEnabled(false);
	SetVisibility(EVisibility::Collapsed);
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

FText SFlareMainMenu::GetText(int32 Index) const
{
	FText CompanyText;
	FText MoneyText;
	FText ShipText;

	if (Game->DoesSaveSlotExist(Index))
	{
		const FFlareSaveSlotInfo& SaveSlotInfo = Game->GetSaveSlotInfo(Index);

		// Build info strings
		CompanyText = SaveSlotInfo.CompanyName;
		ShipText = FText::Format(LOCTEXT("ShipInfoFormat", "{0} {1}"),
			FText::AsNumber(SaveSlotInfo.CompanyShipCount), (SaveSlotInfo.CompanyShipCount == 1 ? LOCTEXT("Ship", "ship") : LOCTEXT("Ships", "ships")));
		MoneyText = FText::Format(LOCTEXT("Credits", "{0} credits"), FText::AsNumber(UFlareGameTools::DisplayMoney(SaveSlotInfo.CompanyValue)));
	}

	return FText::Format(LOCTEXT("SaveInfoFormat", "{0}\n{1}\n{2}\n"), CompanyText, MoneyText, ShipText);
}

const FSlateBrush* SFlareMainMenu::GetSaveIcon(int32 Index) const
{
	return (Game->DoesSaveSlotExist(Index) ? &(Game->GetSaveSlotInfo(Index).EmblemBrush) : FFlareStyleSet::GetIcon("Help"));
}

EVisibility SFlareMainMenu::GetDeleteButtonVisibility(int32 Index) const
{
	return (Game->DoesSaveSlotExist(Index) ? EVisibility::Visible : EVisibility::Collapsed);
}

FText SFlareMainMenu::GetButtonText(int32 Index) const
{
	return (Game->DoesSaveSlotExist(Index) ? LOCTEXT("Load", "Load game") : LOCTEXT("Create", "New game"));
}

const FSlateBrush* SFlareMainMenu::GetButtonIcon(int32 Index) const
{
	return (Game->DoesSaveSlotExist(Index) ? FFlareStyleSet::GetIcon("Load") : FFlareStyleSet::GetIcon("New"));
}

void SFlareMainMenu::OnOpenSlot(TSharedPtr<int32> Index)
{
	AFlarePlayerController* PC = MenuManager->GetPC();
	if (PC && Game)
	{
		Game->SetCurrentSlot(*Index);
		if (Game->DoesSaveSlotExist(*Index))
		{
			MenuManager->OpenMenu(EFlareMenu::MENU_LoadGame);
		}
		else
		{
			MenuManager->OpenMenu(EFlareMenu::MENU_Story);
		}
	}
}

void SFlareMainMenu::OnDeleteSlot(TSharedPtr<int32> Index)
{
	SaveSlotToDelete = *Index;
	MenuManager->Confirm(LOCTEXT("AreYouSure", "ARE YOU SURE ?"),
						 LOCTEXT("ConfirmExit", "Do you really want to delete this save slot ?"),
						 FSimpleDelegate::CreateSP(this, &SFlareMainMenu::OnDeleteSlotConfirmed));
}

void SFlareMainMenu::OnDeleteSlotConfirmed()
{
	if (SaveSlotToDelete >= 0)
	{
		Game->DeleteSaveSlot(SaveSlotToDelete);
		Game->ReadAllSaveSlots();
		SaveSlotToDelete = -1;
	}
}

void SFlareMainMenu::OnOpenCredits()
{
	MenuManager->OpenMenu(EFlareMenu::MENU_Credits);
}

void SFlareMainMenu::OnOpenEULA()
{
	MenuManager->OpenMenu(EFlareMenu::MENU_EULA);
}


#undef LOCTEXT_NAMESPACE

