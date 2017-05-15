
#include "FlareCreditsMenu.h"
#include "../../Flare.h"
#include "../Components/FlareButton.h"
#include "../../Game/FlareGame.h"
#include "../../Game/FlareSaveGame.h"
#include "../../Player/FlareMenuPawn.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"


#define LOCTEXT_NAMESPACE "FlareCreditsMenu"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareCreditsMenu::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	int32 Width = 1.75 * Theme.ContentWidth;
	int32 TextWidth = Width - Theme.ContentPadding.Left - Theme.ContentPadding.Right;

	// Build structure
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SVerticalBox)

		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(Theme.ContentPadding)
		[
			SNew(SHorizontalBox)

			// Icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage).Image(FFlareStyleSet::GetImage("HeliumRain"))
			]

			// Title
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(Theme.ContentPadding)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DONT-TRANSLATE-Title", "HELIUM RAIN"))
				.TextStyle(&Theme.SpecialTitleFont)
			]
		]

		// Main
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.Padding(Theme.ContentPadding)
		[
			SNew(SBox)
			.WidthOverride(Width)
			.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)
				
				// Separator
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 20))
				[
					SNew(SImage).Image(&Theme.SeparatorBrush)
				]
		
				// Scroll container
				+ SVerticalBox::Slot()
				[
					SNew(SScrollBox)
					.Style(&Theme.ScrollBoxStyle)
					.ScrollBarStyle(&Theme.ScrollBarStyle)

					// Team 1
					+ SScrollBox::Slot()
					[
						SNew(SHorizontalBox)

						// Stranger
						+ SHorizontalBox::Slot()
						.Padding(Theme.ContentPadding)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("DONT-TRANSLATE-Stranger", "Gwenna\u00EBl 'Stranger' ARBONA"))
								.TextStyle(&Theme.TitleFont)
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("DONT-TRANSLATE-Stranger-Info", "Art \u2022 UI \u2022 Game design"))
								.TextStyle(&Theme.NameFont)
							]
						]

						// Niavok
						+ SHorizontalBox::Slot()
						.Padding(Theme.ContentPadding)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("DONT-TRANSLATE-Niavok", "Fr\u00E9d\u00E9ric 'Niavok' BERTOLUS"))
								.TextStyle(&Theme.TitleFont)
							]

							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("DONT-TRANSLATE-Niavok-Info", "Gameplay \u2022 Game design"))
								.TextStyle(&Theme.NameFont)
							]
						]
					]

					// Daisy
					+ SScrollBox::Slot()
					.HAlign(HAlign_Center)
					.Padding(Theme.ContentPadding)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DONT-TRANSLATE-Daisy", "Daisy HERBAULT"))
							.TextStyle(&Theme.TitleFont)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DONT-TRANSLATE-Daisy-Info", "Music"))
							.TextStyle(&Theme.NameFont)
						]
					]

					// J�r�me
					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Grom", "Game logo by J\u00E9r\u00F4me MILLION-ROUSSEAU."))
						.TextStyle(&Theme.NameFont)
					]

					// Thanks
					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Special Thanks", "Special thanks to Johanna and our friends at NERD."))
						.TextStyle(&Theme.NameFont)
					]

					// Thanks
					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Project", "This game took us more than three years to create. We hope you'll have a great time playing it !"))
						.TextStyle(&Theme.NameFont)
					]

					// Separator
					+ SScrollBox::Slot()
					.Padding(FMargin(0, 20))
					[
						SNew(SImage).Image(&Theme.SeparatorBrush)
					]

					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Engine-Info", "Helium Rain uses the Unreal\u00AE Engine. Unreal\u00AE is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere."))
						.TextStyle(&Theme.NameFont)
						.WrapTextAt(TextWidth)
					]

					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Engine-Info2", "Unreal\u00AE Engine, Copyright 1998 - 2017, Epic Games, Inc. All rights reserved."))
						.TextStyle(&Theme.NameFont)
						.WrapTextAt(TextWidth)
					]

					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Sound-Info", "Some sound resources were provided by FreeSFX (http://www.freesfx.co.uk)."))
						.TextStyle(&Theme.NameFont)
						.WrapTextAt(TextWidth)
					]

					+ SScrollBox::Slot()
					.Padding(Theme.ContentPadding)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DONT-TRANSLATE-Assets-Info", "Some art assets were provided by CGMontreal, Poleshift Games, W3 Studios and 'Gargore'."))
						.TextStyle(&Theme.NameFont)
						.WrapTextAt(TextWidth)
					]
				]
			]
		]

		// Back
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.AutoHeight()
		[
			SNew(SFlareButton)
			.Transparent(true)
			.Width(3)
			.Text(LOCTEXT("Back", "Back"))
			.OnClicked(this, &SFlareCreditsMenu::OnMainMenu)
		]
	];
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareCreditsMenu::Setup()
{
	SetEnabled(false);
	SetVisibility(EVisibility::Collapsed);
}

void SFlareCreditsMenu::Enter()
{
	FLOG("SFlareCreditsMenu::Enter");
	SetEnabled(true);
	SetVisibility(EVisibility::Visible);
}

void SFlareCreditsMenu::Exit()
{
	SetEnabled(false);
	SetVisibility(EVisibility::Collapsed);
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

void SFlareCreditsMenu::OnMainMenu()
{
	MenuManager->OpenMenu(EFlareMenu::MENU_Main);
}


#undef LOCTEXT_NAMESPACE

