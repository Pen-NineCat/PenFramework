// File /Native/Engine/Core/CoreEvent.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once

#include "../Event/MouseMoveEvent.hpp"
#include "../Event/KeyPressEvent.hpp"
#include "../Event/KeyReleaseEvent.hpp"
#include "../Event/WindowResizeEvent.hpp"
#include "../Event/CharInputEvent.hpp"
#include "../Event/WindowMoveEvent.hpp"
#include "../Event/WindowFocusEvent.hpp"
#include "../Event/WindowMinimizeEvent.hpp"
#include "../Event/WindowMaximizeEvent.hpp"
#include "../Event/DPIChangeEvent.hpp"
#include "../Event/CloseEvent.hpp"
#include <memory>

#include "../Event/MouseEvent.hpp"

namespace PenEngine
{
	struct CoreEvent
	{
		std::unique_ptr<MouseEvent> Mouse;
		std::unique_ptr<MouseMoveEvent> MouseMove;
		std::unique_ptr<KeyPressEvent> KeyPress;
		std::unique_ptr<KeyReleaseEvent> KeyRelease;
		std::unique_ptr<WindowResizeEvent> WindowResize;
		std::unique_ptr<CharInputEvent>  CharInput;
		std::unique_ptr<WindowMoveEvent> WindowMove;
		std::unique_ptr<WindowFocusEvent> WindowFocus;
		std::unique_ptr<WindowMinimizeEvent> WindowMinimize;
		std::unique_ptr<WindowMaximizeEvent> WindowMaximize;
		std::unique_ptr<DPIChangeEvent> DPIChange;
		std::unique_ptr<CloseEvent> Close;

		CoreEvent() noexcept
			: Mouse(new MouseEvent()),
			MouseMove(new MouseMoveEvent()),
			KeyPress(new KeyPressEvent()),
			KeyRelease(new KeyReleaseEvent()),
			WindowResize(new WindowResizeEvent()),
			CharInput(new CharInputEvent()),
			WindowMove(new WindowMoveEvent()),
			WindowFocus(new WindowFocusEvent()),
			WindowMinimize(new WindowMinimizeEvent()),
			WindowMaximize(new WindowMaximizeEvent()),
			DPIChange(new DPIChangeEvent()),
			Close(new CloseEvent())
		{
		}

		~CoreEvent() noexcept = default;

		CoreEvent(const CoreEvent&) = delete;
		CoreEvent(CoreEvent&&) = delete;
		CoreEvent& operator=(const CoreEvent&) = delete;
		CoreEvent& operator=(CoreEvent&&) = delete;
	};
}
