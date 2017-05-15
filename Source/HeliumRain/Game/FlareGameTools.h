#pragma once

#include "../Flare.h"
#include "GameFramework/CheatManager.h"
#include "FlareGameTools.generated.h"

class UFlareWorld;
class AFlarePlayerController;
class UFlareSector;
class AFlareGame;

UCLASS(Within=PlayerController)
class UFlareGameTools : public UCheatManager
{
public:

	GENERATED_UCLASS_BODY()


	/*----------------------------------------------------
		Game tools
	----------------------------------------------------*/

	/** Force sector activation */
	UFUNCTION(exec)
	void ForceSectorActivation(FName SectorIdentifier);

	/** Force sector deactivation */
	UFUNCTION(exec)
	void ForceSectorDeactivation();

	/** Set the default weapon for new created ship */
	UFUNCTION(exec)
	void SetDefaultWeapon(FName NewDefaultWeaponIdentifier);

	/** Set the default turret for new created ship */
	UFUNCTION(exec)
	void SetDefaultTurret(FName NewDefaultTurretIdentifier);

	UFUNCTION(exec)
	void CheckEconomyBalance();

	UFUNCTION(exec)
	void PrintEconomyStatus();

	UFUNCTION(exec)
	void SetAutoSave(bool Autosave);

	UFUNCTION(exec)
	void DisableAutoSave();

	UFUNCTION(exec)
	void ReloadGameWithoutSave();

	/*----------------------------------------------------
		World tools
	----------------------------------------------------*/

	/** Get world time */
	UFUNCTION(exec)
	int64 GetWorldDate();

	/** Set world time */
	UFUNCTION(exec)
	void SetWorldDate(int64 Date);

	/** Simulate a world duration */
	UFUNCTION(exec)
	void Simulate();

	/** Configure time multiplier for active sector planetarium */
	UFUNCTION(exec)
	void SetPlanatariumTimeMultiplier(float Multiplier);

	/** Set all sectors as visted */
	UFUNCTION(exec)
	void RevealMap();

	UFUNCTION(exec)
	void SetFastFastForward(bool FFF);

	/*----------------------------------------------------
		Company tools
	----------------------------------------------------*/

	/** Make war between two company */
	UFUNCTION(exec)
	void DeclareWar(FName Company1ShortName, FName Company2ShortName);

	/** Make peace two company */
	UFUNCTION(exec)
	void MakePeace(FName Company1ShortName, FName Company2ShortName);

	UFUNCTION(exec)
	void PrintCompanyList();

	UFUNCTION(exec)
	void PrintCompany(FName CompanyShortName);

	UFUNCTION(exec)
	void PrintCompanyByIndex(int32 Index);

	/** Give reputation to a company, can be negative */
	UFUNCTION(exec)
	void GiveReputation(FName CompanyShortName1, FName CompanyShortName2, float Amount);

	UFUNCTION(exec)
	void TakeCompanyControl(FName CompanyShortName);

	/*----------------------------------------------------
		Fleet tools
	----------------------------------------------------*/

	/** Create a fleet */
	UFUNCTION(exec)
	void CreateFleet(FString FleetName, FName FirstShipImmatriculation);

	/** Disband a fleet. Not possible during travelling */
	UFUNCTION(exec)
	void DisbandFleet(FName FleetIdentifier);

	/** Add a ship to a fleet. Not possible during travelling*/
	UFUNCTION(exec)
	void AddToFleet(FName FleetIdentifier, FName ShipImmatriculation);

	/** Remove a ship from a fleet. Not possible during travelling*/
	UFUNCTION(exec)
	void RemoveFromFleet(FName FleetIdentifier, FName ShipImmatriculation);

	/** Merge fleet 2 in fleet 1. Not possible during travelling*/
	UFUNCTION(exec)
	void MergeFleets(FName Fleet1Identifier, FName Fleet2Identifier);

	/*----------------------------------------------------
		Trade route tools
	----------------------------------------------------*/

	/** Create a trade route */
	UFUNCTION(exec)
	void CreateTradeRoute(FString TradeRouteName, FName CompanyShortName);

	/** Dissolve a trade route */
	UFUNCTION(exec)
	void DissolveTradeRoute(FName TradeRouteIdentifier);

	/** Add a fleet to a trade route */
	UFUNCTION(exec)
	void AddToTradeRoute(FName TradeRouteIdentifier, FName FleetIdentifier);

	/** Remove a fleet from a trade route */
	UFUNCTION(exec)
	void RemoveFromTradeRoute(FName TradeRouteIdentifier, FName FleetIdentifier);


	/*----------------------------------------------------
		Travel tools
	----------------------------------------------------*/

	/** Start travel. Not possible during travelling */
	UFUNCTION(exec)
	void StartTravel(FName FleetIdentifier, FName SectorIdentifier);

	UFUNCTION(exec)
	void PrintTravelList();

	UFUNCTION(exec)
	void PrintTravelByIndex(int32 Index);

	/*----------------------------------------------------
		Sector tools
	----------------------------------------------------*/

	/** Create a ship in a sector for the current player*/
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateShipForMeInSector(FName ShipClass, FName SectorIdentifier, bool UnderConstruction);

	/** Create a station attached to an asteroid in the level */
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateStationInCompanyAttachedInSector(FName StationClass, FName CompanyShortName, FName SectorIdentifier, bool UnderConstruction);

	UFUNCTION(exec)
	void PrintSectorList();

	UFUNCTION(exec)
	void PrintSector(FName SectorIdentifier);

	UFUNCTION(exec)
	void PrintSectorByIndex(int32 Index);

	UFUNCTION(exec)
	void GiveBirth(int32 SectorIndex, uint32 Population);

	UFUNCTION(exec)
	void Scrap(FName ShipImmatriculation, FName TargetStationImmatriculation);


	/*----------------------------------------------------
		Trade tools
	----------------------------------------------------*/

	UFUNCTION(exec)
	void PrintCargoBay(FName ShipImmatriculation);

	UFUNCTION(exec)
	void GiveResources(FName ShipImmatriculation, FName ResourceIdentifier, uint32 Quantity);

	UFUNCTION(exec)
	void TakeResources(FName ShipImmatriculation, FName ResourceIdentifier, uint32 Quantity);

	UFUNCTION(exec)
	void TakeMoney(FName CompanyShortName, int64 Amount);

	UFUNCTION(exec)
	void GiveMoney(FName CompanyShortName, int64 Amount);

	UFUNCTION(exec)
	void GiveResearch(FName CompanyShortName, int64 Amount);

	UFUNCTION(exec)
	void TransferResources(FName SourceImmatriculation, FName DestinationImmatriculation, FName ResourceIdentifier, uint32 Quantity);


	/*----------------------------------------------------
		Active Sector tools
	----------------------------------------------------*/

	/** Create a station in the level */
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateStationForMe(FName StationClass, bool UnderConstruction);

	/** Create a station in the level */
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateStationInCompany(FName StationClass, FName CompanyShortName, float Distance, bool UnderConstruction);

	/** Create a ship in the level for the current player*/
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateShipForMe(FName ShipClass);

	/** Create a ship in the level for a specific company identified by its short name*/
	UFUNCTION(exec)
	UFlareSimulatedSpacecraft* CreateShipInCompany(FName ShipClass, FName CompanyShortName, float Distance);

	/** Create ships in the level for a specific company identified by its short name*/
	UFUNCTION(exec)
	void CreateShipsInCompany(FName ShipClass, FName CompanyShortName, float Distance, int32 Count);

	/** Create 2 fleets for 2 companies At a defined distance */
	UFUNCTION(exec)
	void CreateQuickBattle(float Distance, FName Company1, FName Company2, FName ShipClass1, int32 ShipClass1Count, FName ShipClass2, int32 ShipClass2Count);

	/** Add an asteroid to the world */
	UFUNCTION(exec)
	void CreateAsteroid(int32 ID, FName Name);

	/*----------------------------------------------------
		Helper
	----------------------------------------------------*/

	static const int64 DAYS_IN_YEAR = 365;
	static const int64 SECONDS_IN_MINUTE = 60;
	static const int64 SECONDS_IN_HOUR = 60 * SECONDS_IN_MINUTE;
	static const int64 SECONDS_IN_DAY = 24 * SECONDS_IN_HOUR;
	static const int64 SECONDS_IN_YEAR = DAYS_IN_YEAR * SECONDS_IN_DAY;
	static const int64 START_YEAR = 0;

	static FVector ColorToVector(FLinearColor Color);

	static FString FormatTime(int64 Time, int Deep);

	static FString FormatDate(int64 Date, int Deep);

	static FText GetDisplayDate(int64 Days);

	/** Get the cost of a spacecraft */
	static int64 ComputeSpacecraftPrice(FName ShipIdentifier, UFlareSimulatedSector* Sector, bool WithMargin, bool ConstructionPrice = false, bool LocalPrice = true);

	static inline int64 DisplayMoney(int64 Money)
	{
		return Money/100;
	}

	/** Format an immatriculation for display */
	static FText DisplaySpacecraftName(UFlareSimulatedSpacecraft* Spacecraft, bool ForceHidePrefix = false, bool ToUpper = false);


	/*----------------------------------------------------
		Getter
	----------------------------------------------------*/

	AFlarePlayerController* GetPC() const;

	AFlareGame* GetGame() const;

	UFlareWorld* GetGameWorld() const;

	UFlareSector* GetActiveSector() const;

	static bool FastFastForward;

};
