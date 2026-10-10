// File /Code/Native/Setup.cpp
//
// SPDX - License - Identifier: MIT
// Copyright(c) 2026 - Present Pen - NineCat(PenNineCat) https://github.com/Pen-NineCat

#include "Engine/Core/Environment.h"

#ifdef PENFRAMEWORK_TARGET_GAME
#include "Game/Core/GameCoreApplication.h"
#endif

#ifdef PENFRAMEWORK_WIN_SUBSYSTEM_NATIVE

#else
int main(int argc, char* argv[])
{
	#ifdef PENFRAMEWORK_TARGET_GAME
	PenEngine::GameCoreApplication app(argc, argv);
	return app.Exec();
	#endif
}
#endif