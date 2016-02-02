
#include "../../Flare.h"
#include "FlareShipList.h"

#include "../../Game/FlareGame.h"
#include "../../Player/FlareMenuManager.h"
#include "../../Player/FlarePlayerController.h"

#define LOCTEXT_NAMESPACE "FlareSectorList"


/*----------------------------------------------------
	Construct
----------------------------------------------------*/

void SFlareShipList::Construct(const FArguments& InArgs)
{
	// Data
	MenuManager = InArgs._MenuManager;
	const FFlareStyleCatalog& Theme = FFlareStyleSet::GetDefaultTheme();
	AFlarePlayerController* PC = MenuManager->GetPC();
	OnItemSelected = InArgs._OnItemSelected;
	
	// Build structure
	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Left)
	[
		SNew(SBox)
		.WidthOverride(Theme.ContentWidth)
		[
			SNew(SVerticalBox)

			// Section title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.TitlePadding)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(InArgs._Title)
				.TextStyle(&FFlareStyleSet::GetDefaultTheme().SubTitleFont)
			]

			// Section title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.TitlePadding)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Nothing", "No objects."))
				.TextStyle(&FFlareStyleSet::GetDefaultTheme().TextFont)
				.Visibility(this, &SFlareShipList::GetNoObjectsVisibility)
			]

			// Box
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Theme.ContentPadding)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(TargetList, SListView< TSharedPtr<FInterfaceContainer> >)
				.ListItemsSource(&TargetListData)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SFlareShipList::GenerateTargetInfo)
				.OnSelectionChanged(this, &SFlareShipList::OnTargetSelected)
			]
		]
	];
}


/*----------------------------------------------------
	Interaction
----------------------------------------------------*/

void SFlareShipList::AddShip(IFlareSpacecraftInterface* ShipCandidate)
{
	TargetListData.AddUnique(FInterfaceContainer::New(ShipCandidate));
}

void SFlareShipList::RefreshList()
{
	struct FSortBySize
	{
		FORCEINLINE bool operator()(const TSharedPtr<FInterfaceContainer> PtrA, const TSharedPtr<FInterfaceContainer> PtrB) const
		{
			check(PtrA.IsValid());
			check(PtrB.IsValid());
			IFlareSpacecraftInterface* A = PtrA->ShipInterfacePtr;
			IFlareSpacecraftInterface* B = PtrB->ShipInterfacePtr;

			if (A->IsStation())
			{
				if (B->IsStation())
				{
					return A->GetDockingSystem()->GetDockCount() > B->GetDockingSystem()->GetDockCount();
				}
				else
				{
					return true;
				}
			}
			else if (B->IsStation())
			{
				return false;
			}
			else
			{
				if (A->GetSize() > B->GetSize())
				{
					return true;
				}
				else if (A->GetSize() < B->GetSize())
				{
					return false;
				}
				else if (A->IsMilitary())
				{
					if (!B->IsMilitary())
					{
						return true;
					}
					else
					{
						return A->GetWeaponsSystem()->GetWeaponGroupCount() > B->GetWeaponsSystem()->GetWeaponGroupCount();
					}
				}
				else
				{
					return false;
				}
			}
			return false;
		}
	};

	ClearSelection();
	TargetListData.Sort(FSortBySize());
	TargetList->RequestListRefresh();
}

void SFlareShipList::ClearSelection()
{
	TargetList->ClearSelection();
}

void SFlareShipList::Reset()
{
	TargetListData.Empty();
	TargetList->RequestListRefresh();
	SelectedItem.Reset();
}

IFlareSpacecraftInterface* SFlareShipList::GetSelectedSpacecraft() const
{
	if (SelectedItem.IsValid() && SelectedItem->ShipInterfacePtr)
	{
		return SelectedItem->ShipInterfacePtr;
	}
	return NULL;
}

FFlareSpacecraftComponentDescription* SFlareShipList::GetSelectedPart() const
{
	if (SelectedItem.IsValid() && SelectedItem->PartDescription)
	{
		return SelectedItem->PartDescription;
	}
	return NULL;
}

UFlareCompany* SFlareShipList::GetSelectedCompany() const
{
	if (SelectedItem.IsValid() && SelectedItem->CompanyPtr)
	{
		return SelectedItem->CompanyPtr;
	}
	return NULL;
}


/*----------------------------------------------------
	Callbacks
----------------------------------------------------*/

EVisibility SFlareShipList::GetNoObjectsVisibility() const
{
	return (TargetListData.Num() > 0 ? EVisibility::Collapsed : EVisibility::Visible);
}

TSharedRef<ITableRow> SFlareShipList::GenerateTargetInfo(TSharedPtr<FInterfaceContainer> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	AFlarePlayerController* PC = MenuManager->GetPC();

	if (Item->ShipInterfacePtr)
	{
		return SNew(SFlareListItem, OwnerTable)
			.Width(15)
			.Height(2)
			.Content()
			[
				SNew(SFlareSpacecraftInfo)
				.Player(PC)
				.Spacecraft(Item->ShipInterfacePtr)
				.Minimized(true)
				.Visible(true)
			];
	}
	else
	{
		return SNew(SFlareListItem, OwnerTable)
			.Content()
			[
				SNew(STextBlock).Text(LOCTEXT("Invalid", "Invalid item"))
			];
	}
}

void SFlareShipList::OnTargetSelected(TSharedPtr<FInterfaceContainer> Item, ESelectInfo::Type SelectInfo)
{
	FLOG("SFlareShipList::OnTargetSelected");
	TSharedPtr<SFlareListItem> ItemWidget = StaticCastSharedPtr<SFlareListItem>(TargetList->WidgetFromItem(Item));
	SelectedItem = Item;

	// Update selection
	if (PreviousSelection.IsValid())
	{
		TSharedRef<SFlareSpacecraftInfo> ShipInfoWidget = StaticCastSharedRef<SFlareSpacecraftInfo>(PreviousSelection->GetContainer()->GetContent());
		ShipInfoWidget->SetMinimized(true);
		PreviousSelection->SetSelected(false);
	}
	if (ItemWidget.IsValid())
	{
		TSharedRef<SFlareSpacecraftInfo> ShipInfoWidget = StaticCastSharedRef<SFlareSpacecraftInfo>(ItemWidget->GetContainer()->GetContent());

		if (OnItemSelected.IsBound())
		{
			OnItemSelected.Execute(Item);
		}
		else
		{
			ShipInfoWidget->SetMinimized(false);
			ItemWidget->SetSelected(true);
		}

		PreviousSelection = ItemWidget;
	}
}


#undef LOCTEXT_NAMESPACE
