// Developer-mode shortcuts for the game's features that came before the registry (docs/dev_mode.md): each runs the
// feature's own console command, so the panel and the tests drive the same code. New features register their own
// actions in their own .cpp instead of here.

#include "Dev/LyudDev.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace LyudDevFeatures
{
	using LyudDev::FAutoAction;

	void Run(UWorld* World, const TCHAR* Cmd)
	{
		if (GEngine && World)
		{
			GEngine->Exec(World, Cmd);
		}
	}

	// the cable car: into each station's vestibule (the names are the stations' own)
	FAutoAction GAton(TEXT("transport.aton"), TEXT("transport"), TEXT("transport.station.aton"), [](UWorld* W) { Run(W, TEXT("Transport.Goto aton vestibule")); });
	FAutoAction GAcadem(TEXT("transport.academ"), TEXT("transport"), TEXT("transport.station.academ"), [](UWorld* W) { Run(W, TEXT("Transport.Goto academ vestibule")); });
	FAutoAction GZenit(TEXT("transport.zenit"), TEXT("transport"), TEXT("transport.station.zenit"), [](UWorld* W) { Run(W, TEXT("Transport.Goto zenit vestibule")); });
	FAutoAction GPolar(TEXT("transport.polar"), TEXT("transport"), TEXT("transport.station.polar"), [](UWorld* W) { Run(W, TEXT("Transport.Goto polar vestibule")); });
	FAutoAction GMayak(TEXT("transport.mayak"), TEXT("transport"), TEXT("transport.station.mayak"), [](UWorld* W) { Run(W, TEXT("Transport.Goto mayak vestibule")); });
	FAutoAction GAmber(TEXT("transport.amber"), TEXT("transport"), TEXT("transport.station.amber"), [](UWorld* W) { Run(W, TEXT("Transport.Goto amber vestibule")); });
	FAutoAction GHorizon(TEXT("transport.horizon"), TEXT("transport"), TEXT("transport.station.horizon"), [](UWorld* W) { Run(W, TEXT("Transport.Goto horizon vestibule")); });
	FAutoAction GDiagram(TEXT("transport.diagram"), TEXT("transport"), TEXT("dev.action.transport.diagram"), [](UWorld* W) { Run(W, TEXT("Transport.Diagram")); });
	// the train: its doors open at the stop in 20 s
	FAutoAction GTrainBeach(TEXT("transport.train_beach"), TEXT("transport"), TEXT("dev.action.transport.train_beach"), [](UWorld* W) { Run(W, TEXT("lyud.Railway.Next beach")); });
	FAutoAction GTrainIsland(TEXT("transport.train_island"), TEXT("transport"), TEXT("dev.action.transport.train_island"), [](UWorld* W) { Run(W, TEXT("lyud.Railway.Next island")); });

	// fishing
	FAutoAction GRod(TEXT("fishing.give"), TEXT("fishing"), TEXT("dev.action.fishing.give"), [](UWorld* W) { Run(W, TEXT("lyud.Fish.Give")); });
	FAutoAction GBite(TEXT("fishing.bite"), TEXT("fishing"), TEXT("dev.action.fishing.bite"), [](UWorld* W) { Run(W, TEXT("lyud.Fish.Bite")); });
	FAutoAction GBoat(TEXT("fishing.boat"), TEXT("fishing"), TEXT("dev.action.fishing.boat"), [](UWorld* W) { Run(W, TEXT("lyud.Fish.Boat board")); });
	FAutoAction GBook(TEXT("fishing.book"), TEXT("fishing"), TEXT("dev.action.fishing.book"), [](UWorld* W) { Run(W, TEXT("lyud.Fish.Book")); });
	FAutoAction GSell(TEXT("fishing.sell"), TEXT("fishing"), TEXT("dev.action.fishing.sell"), [](UWorld* W) { Run(W, TEXT("lyud.Fish.Kitchen sell")); });

	// eating out
	FAutoAction GRestOpen(TEXT("food.restaurant_open"), TEXT("food"), TEXT("dev.action.food.restaurant_open"), [](UWorld* W) { Run(W, TEXT("lyud.Restaurant.Open 1")); });
	FAutoAction GRestSit(TEXT("food.restaurant_sit"), TEXT("food"), TEXT("dev.action.food.restaurant_sit"), [](UWorld* W) { Run(W, TEXT("lyud.Restaurant.Sit")); });
	FAutoAction GRestFast(TEXT("food.restaurant_fast"), TEXT("food"), TEXT("dev.action.food.restaurant_fast"), [](UWorld* W) { Run(W, TEXT("lyud.Restaurant.WaitScale 0.1")); });
	FAutoAction GDinerOpen(TEXT("food.diner_open"), TEXT("food"), TEXT("dev.action.food.diner_open"), [](UWorld* W) { Run(W, TEXT("lyud.Diner.Open 1")); });
	FAutoAction GSmokes(TEXT("food.cigarettes"), TEXT("food"), TEXT("dev.action.food.cigarettes"), [](UWorld* W) { Run(W, TEXT("lyud.Smoke.Give")); });

	// the sky and the season
	FAutoAction GLightning(TEXT("weather.lightning"), TEXT("weather"), TEXT("dev.action.weather.lightning"), [](UWorld* W) { Run(W, TEXT("lyud.Weather.Strike 800")); });
	FAutoAction GSnowPaths(TEXT("weather.snow_paths"), TEXT("weather"), TEXT("dev.action.weather.snow_paths"), [](UWorld* W) { Run(W, TEXT("lyud.Winter.SnowOnPaths 10")); });
	FAutoAction GFlash(TEXT("weather.sun_blind"), TEXT("weather"), TEXT("dev.action.weather.sun_blind"), [](UWorld* W) { Run(W, TEXT("lyud.Sun.Glare.FlashBlind")); });
}
