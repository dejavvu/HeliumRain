
#include "FlareQuestCatalog.h"
#include "../Flare.h"
#include "AssetRegistryModule.h"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

UFlareQuestCatalog::UFlareQuestCatalog(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	TArray<FAssetData> AssetList;
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.GetAssetsByClass(UFlareQuestCatalogEntry::StaticClass()->GetFName(), AssetList);

	for (int32 Index = 0; Index < AssetList.Num(); Index++)
	{
		//FLOGV("UFlareQuestCatalog::UFlareQuestCatalog : Found '%s'", *AssetList[Index].GetFullName());
		UFlareQuestCatalogEntry* Quest = Cast<UFlareQuestCatalogEntry>(AssetList[Index].GetAsset());
		FCHECK(Quest);
		Quests.Add(Quest);
	}
}
