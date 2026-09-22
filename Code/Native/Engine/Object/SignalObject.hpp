// File /Native/Engine/Object/SignalObject.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

#pragma once
#include "../Core/Environment.h"
#include "../Coroutine/ICoroutineInstruction.hpp"
#include "../DebugTools/DebugVerify.hpp"
#include "../Exception/Exception.hpp"
#include <concepts>
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace PenEngine
{
	class SignalObject;
	class SignalBase;

	template <typename... Args>
	class ObjectSignal;

	template <typename... Args>
	class NothrowObjectSignal;

	/// @brief 作用域连接的槽位令牌（内部类型，不属于用户API）
	/// @note 由 ScopedConnection 独占持有，地址在连接存活期间稳定，
	///       因此信号侧只记这个地址就够：连接对象怎么移动都不需要重绑
	class ScopedSlotToken
	{
	public:
		/// 信号析构时被置空，连接随之失效
		SignalBase* Signal = nullptr;
	};

	/// @brief 作用域连接：以槽位令牌为身份，析构即断开
	///
	/// 与 Connect(SignalObject*, F) 的区别：它不需要接收者身份，因此匿名的、临时的、
	/// 甚至存放在协程帧里的等待者都能安全持有；断开只影响自己那一个槽位，不会碰到别人。
	///
	/// @note 信号先析构时连接自动失效（令牌里的信号指针被置空），所以两侧析构顺序天然安全
	/// @note 断开是幂等的：未连接或已失效时为空操作
	class ScopedConnection
	{
		friend class SignalBase;

		template <typename... Args>
		friend class ObjectSignal;

		template <typename... Args>
		friend class NothrowObjectSignal;
	public:
		ScopedConnection() noexcept = default;
		ScopedConnection(const ScopedConnection&) = delete;
		ScopedConnection& operator=(const ScopedConnection&) = delete;

		ScopedConnection(ScopedConnection&& other) noexcept : m_token(std::move(other.m_token)) {}

		ScopedConnection& operator=(ScopedConnection&& other) noexcept
		{
			if (this == &other)
				return *this;

			Disconnect(); // 先断开自己原有的连接
			m_token = std::move(other.m_token);
			return *this;
		}

		~ScopedConnection() noexcept
		{
			Disconnect();
		}

		[[nodiscard]] bool IsConnected() const noexcept
		{
			return m_token != nullptr && m_token->Signal != nullptr;
		}

		/// @brief 断开连接（幂等）；信号已析构时为空操作
		void Disconnect() noexcept;
	private:
		/// @brief 由信号在建立连接时交付令牌
		void AdoptToken(std::unique_ptr<ScopedSlotToken> token) noexcept
		{
			m_token = std::move(token);
		}

		std::unique_ptr<ScopedSlotToken> m_token;
	};

	/// 信号机制的总协议
	/// @note 设计前提：对象地址即身份。信号自身持有 owner 指针（全系统唯一的 owner 记录），
	///       因此不存在"信号属于谁"的第二种表达，也就不需要维护双向登记表的同步。
	///
	/// 四条生命周期路径：
	/// 1. 接收者销毁     -> SignalObject::~SignalObject 遍历 m_connectedSignal 并调用 ForgetReceiver
	/// 2. 发送者销毁     -> ~ObjectSignal 通知每个接收者移除登记，并让作用域连接失效
	/// 3. 显式断开       -> SignalBase::Disconnect：槽位表与接收者登记表各删一个指针
	/// 4. 作用域连接析构 -> ScopedConnection::~ScopedConnection：只撤自己那一个槽位
	///
	/// @note 连接关系的唯一真相来源是信号自己的槽位表，接收者侧的 m_connectedSignal 只是清理路径
	/// @note 槽位失效一律是"标记"而不是"就地擦除"：Emit 正在遍历时擦除会毁掉迭代器，
	///       真正的擦除推迟到没有 Emit 在遍历时（详见 SignalSlotTable）
	/// @note 各信号的成员模板实现在 SignalObject.h 末尾：那里 SignalObject 才是完整类型，
	///       而 MSVC 不允许在 SignalObject 不完整时从非模板函数体中调用其成员
	class SignalBase
	{
	public:
		SignalBase(const SignalBase&) noexcept = delete;
		SignalBase& operator=(const SignalBase&) noexcept = delete;

		/// 禁止移动：m_owner 缓存了 owner 地址，移动会让 owner 身份错位
		SignalBase(SignalBase&&) noexcept = delete;
		SignalBase& operator=(SignalBase&&) noexcept = delete;

		/// @brief 断开指定接收者：同时清理本信号的槽位与该接收者的登记
		virtual void Disconnect(SignalObject* receiver) = 0;

		/// @brief 仅清理本信号的槽位，交由接收者自己维护其登记表
		/// @note 供接收者析构路径使用；此时接收者的登记表正处于销毁过程中，不能再被回写
		virtual void ForgetReceiver(SignalObject* receiver) noexcept = 0;

		/// @brief 撤掉某个作用域连接占用的槽位
		/// @note 内部接口：只由 ScopedConnection 调用，因此不接受任意 SignalObject 身份
		virtual void DisconnectScoped(ScopedSlotToken& token) noexcept = 0;

		[[nodiscard]] SignalObject* GetOwner() const noexcept { return m_owner; }

		/// @note 必须是 public 且 virtual：派生类的析构需要能以 override 声明
		virtual ~SignalBase() noexcept = default;
	protected:
		/// @brief 由信号的构造函数显式指定 owner
		/// @note owner 为空说明信号没有归属，析构通知协议会静默失效，因此在此拦截
		explicit SignalBase(SignalObject* owner) noexcept : m_owner(owner)
		{
			// owner 是析构通知协议的唯一依据，为空则协议静默失效
			DEBUG_VERIFY_REPORT(m_owner != nullptr, "A signal must be owned by a SignalObject");
		}

		/// @brief 发送者销毁时，通知各接收者放弃对本信号的登记，并让作用域连接失效
		/// @note 由派生类在析构体中显式调用（~SignalBase 期间虚调用不会下派到派生类）
		virtual void NotifyOwnerGoneImpl() noexcept = 0;

		SignalObject* m_owner = nullptr;
	};

	inline void ScopedConnection::Disconnect() noexcept
	{
		if (m_token != nullptr && m_token->Signal != nullptr)
			m_token->Signal->DisconnectScoped(*m_token);

		m_token.reset();
	}

	/// @brief 等待某个信号的下一次 Emit
	///
	/// 用法：
	///   `auto [a, b] = co_await obj.SomeSignal;`   // 有参数：结果是 std::tuple<Args...>
	///   `co_await obj.OnDeath;`                    // 无参数（ObjectSignal<void>/<>）：结果就是 void
	///
	/// @note 它同时是"协程等待器"和"调度器轮询的 ICoroutineInstruction"：
	///       订阅动作只做记录，真正的 Resume 发生在调度器下一次驱动循环里，
	///       绝不会在信号的 Emit 过程中恢复协程（否则会把槽位表的重入问题引进来）
	/// @note 只能等到"下一次"Emit：信号是无状态的，无法得知进入等待之前是否已经发过；
	///       等待期间若有多次 Emit，只有最后一次的参数会被保留
	/// @note 等待期间信号源（信号所属对象）析构时，本次等待以异常了结，不会永久挂起
	/// @note 等待器只会在 await_suspend 期间访问信号对象，因此信号先析构也不会踩空指针

	/// @brief 等待器公共部分：订阅、就绪判定、取消
	/// @note 结果类型与结果存储由派生类决定；等待器固定存放在协程帧里，槽位捕获的就是它的地址
	template <typename SignalType>
	class SignalAwaitableBase : public ICoroutineInstruction
	{
	public:
		// ---- 指令身份（调度器每帧轮询）----
		[[nodiscard]] bool IsReady() override { return m_fired || m_sourceGone; }

		/// @brief 让 drain 能优雅解除等待：取消后 IsReady() 即为真，恢复时 await_resume 抛异常
		bool Cancel() override
		{
			m_sourceGone = true;
			return true;
		}
	protected:
		explicit SignalAwaitableBase(SignalType& signal) noexcept : m_signal(&signal) {}

		/// @brief 订阅事件与"源析构"，并把本等待器登记为任务的当前等待对象
		/// @note 两个订阅都必须在挂起前挂上：漏掉事件订阅会永远等不到事件，
		///       漏掉源析构订阅会在信号源先死时永久挂起
		template <typename EventSlot, typename PromiseType>
		void Subscribe(EventSlot eventSlot, std::coroutine_handle<PromiseType> handle)
		{
			m_eventGuard = m_signal->ConnectScoped(eventSlot);
			m_sourceGuard = m_signal->ConnectSourceGone(SourceGoneSlot{ this });

			handle.promise().SetAwaitedObject(this);
		}

		[[nodiscard]] bool Fired() const noexcept { return m_fired; }

		void MarkFired() noexcept { m_fired = true; }
	private:
		/// @brief 源析构槽位：了结本次等待
		struct SourceGoneSlot
		{
			SignalAwaitableBase* Self = nullptr;

			void operator()(SignalObject*) noexcept
			{
				Self->m_sourceGone = true;
				Self->m_eventGuard.Disconnect(); // 源没了就不会再有事件，顺手断开
			}
		};

		SignalType* m_signal = nullptr;
		ScopedConnection m_eventGuard;
		ScopedConnection m_sourceGuard;
		bool m_fired = false;
		bool m_sourceGone = false;
	};

	/// @brief 等待器主模板（有参数）：恢复时得到 std::tuple<Args...>
	template <typename SignalType>
	class SignalAwaitable;

	template <template <typename...> typename SignalTemplate, typename... Args>
	class SignalAwaitable<SignalTemplate<Args...>> final : public SignalAwaitableBase<SignalTemplate<Args...>>
	{
		using Base = SignalAwaitableBase<SignalTemplate<Args...>>;

		// 信号类那一层已经把 void 折成空包，等待器不该再收到 void（空包版本见下面的特化）
		static_assert(!(std::is_void_v<Args> || ...),
			"SignalAwaitable 不应以 void 参数实例化：请直接写 ObjectSignal<void>，信号类会把它折成空参数包");
	public:
		using SignalType = SignalTemplate<Args...>;
		using ResultType = std::tuple<Args...>;

		explicit SignalAwaitable(SignalType& signal) noexcept : Base(signal) {}

		SignalAwaitable(const SignalAwaitable&) = delete;
		SignalAwaitable& operator=(const SignalAwaitable&) = delete;
		SignalAwaitable(SignalAwaitable&&) = delete;
		SignalAwaitable& operator=(SignalAwaitable&&) = delete;

		// ---- 等待器身份 ----
		[[nodiscard]] bool await_ready() const noexcept { return Base::Fired(); }

		template <typename PromiseType>
		bool await_suspend(std::coroutine_handle<PromiseType> handle)
		{
			Base::Subscribe(EventSlot{ this }, handle);
			return true;
		}

		/// @brief 取回本次 Emit 的参数元组；被中断（源析构/取消）时抛异常
		ResultType await_resume()
		{
			if (!Base::Fired())
				ThrowException(Exception("SignalAwaitInterrupted", "等待的信号源已析构或等待被取消，本次等待不会再有事件"));

			return std::move(*m_value);
		}
	private:
		/// @brief 事件槽位：只记录参数，绝不 resume
		/// @note noexcept 由"元组能否无异常构造"决定：不满足的话，
		///       本等待器就只适用于 ObjectSignal，NothrowObjectSignal 的 ConnectScoped 会拒绝它
		struct EventSlot
		{
			SignalAwaitable* Self = nullptr;

			void operator()(Args... args) noexcept(std::is_nothrow_constructible_v<ResultType, Args...>)
			{
				Self->m_value.emplace(std::forward<Args>(args)...);
				Self->MarkFired();
			}
		};

		std::optional<ResultType> m_value;
	};

	/// @brief 等待器 void 特化（无参数）：恢复时不产出任何值
	/// @note `ObjectSignal<void>` 与 `ObjectSignal<>` 都走这里（void 在信号那一层已折成空包），
	///       因此 `co_await obj.OnDeath;` 的结果就是 void，而不是语义含糊的 std::tuple<>
	template <template <typename...> typename SignalTemplate>
	class SignalAwaitable<SignalTemplate<>> final : public SignalAwaitableBase<SignalTemplate<>>
	{
		using Base = SignalAwaitableBase<SignalTemplate<>>;
	public:
		using SignalType = SignalTemplate<>;
		using ResultType = void;

		explicit SignalAwaitable(SignalType& signal) noexcept : Base(signal) {}

		SignalAwaitable(const SignalAwaitable&) = delete;
		SignalAwaitable& operator=(const SignalAwaitable&) = delete;
		SignalAwaitable(SignalAwaitable&&) = delete;
		SignalAwaitable& operator=(SignalAwaitable&&) = delete;

		// ---- 等待器身份 ----
		[[nodiscard]] bool await_ready() const noexcept { return Base::Fired(); }

		template <typename PromiseType>
		bool await_suspend(std::coroutine_handle<PromiseType> handle)
		{
			Base::Subscribe(EventSlot{ this }, handle);
			return true;
		}

		/// @brief 无参数可返回；被中断（源析构/取消）时抛异常
		void await_resume()
		{
			if (!Base::Fired())
				ThrowException(Exception("SignalAwaitInterrupted", "等待的信号源已析构或等待被取消，本次等待不会再有事件"));
		}
	private:
		/// @brief 事件槽位：只置就绪标记（无参数可存）
		struct EventSlot
		{
			SignalAwaitable* Self = nullptr;

			void operator()() noexcept
			{
				Self->MarkFired();
			}
		};
	};

	/// @brief 槽位表：两个信号实现只差回调的可调用性约束，槽位的增删/遍历/回收完全一致，抽在这里
	///
	/// 三条关键纪律：
	/// 1. Emit 期间只把槽位标记失效，绝不动回调本身 —— 回调可能正在栈上执行，
	///    销毁或在容器里搬移它都会毁掉正在执行的闭包（捕获被析构/被掏空）
	/// 2. 失效槽位的销毁推迟到"最后一层 Emit 退出"之后统一进行
	/// 3. 槽位容器用 std::deque：push_back 不会搬移已有元素（引用保持有效），
	///    因此回调里 Connect 新槽位也不会让正在执行的回调被搬家
	template <typename CallbackType>
	class SignalSlotTable
	{
	protected:
		struct Slot
		{
			/// 接收者身份（作用域槽位为空）
			SignalObject* Receiver = nullptr;

			/// 作用域令牌（接收者槽位为空）
			ScopedSlotToken* Scoped = nullptr;

			/// 已失效：不再参与 Emit，等 Emit 结束后由 Compact 真正销毁
			bool Dead = false;

			CallbackType Callback;

			Slot() = default;

			Slot(SignalObject* receiver, ScopedSlotToken* scoped, CallbackType callback)
				: Receiver(receiver), Scoped(scoped), Dead(false), Callback(std::move(callback))
			{}

			[[nodiscard]] bool IsAlive() const noexcept { return !Dead; }
		};

		/// @brief Emit 深度守卫：最后一层退出时才销毁失效槽位
		struct EmitGuard
		{
			SignalSlotTable& Table;

			explicit EmitGuard(SignalSlotTable& table) noexcept : Table(table) { ++Table.m_emitDepth; }

			~EmitGuard() noexcept
			{
				if (--Table.m_emitDepth == 0 && Table.m_hasDeadSlots)
					Table.Compact();
			}
		};

		[[nodiscard]] Slot* FindSlotOfReceiver(SignalObject* receiver) noexcept
		{
			// 下标遍历：deque 的 push_back 会失效迭代器，但引用保持有效
			for (Usize index = 0; index < m_slot.size(); ++index)
			{
				Slot& slot = m_slot[index];

				if (slot.IsAlive() && slot.Receiver == receiver)
					return &slot;
			}

			return nullptr;
		}

		[[nodiscard]] Slot* FindSlotOfToken(const ScopedSlotToken* token) noexcept
		{
			for (Usize index = 0; index < m_slot.size(); ++index)
			{
				Slot& slot = m_slot[index];

				if (slot.IsAlive() && slot.Scoped == token)
					return &slot;
			}

			return nullptr;
		}

		/// @brief 撤掉一个槽位：只标记失效，绝不动回调
		/// @note 回调可能正在栈上执行（接收者在自己的回调里销毁自己），
		///       销毁或在容器里搬移它都会毁掉那个闭包，因此真正的销毁交给 Compact
		void KillSlot(Slot& slot) noexcept
		{
			if (slot.Scoped != nullptr)
			{
				slot.Scoped->Signal = nullptr;
				slot.Scoped = nullptr;
			}

			slot.Receiver = nullptr;
			slot.Dead = true;
			m_hasDeadSlots = true;
		}

		/// @brief 撤掉全部槽位；onReceiverGone 由派生类给出（它需要访问 SignalObject 的私有接口）
		template <typename F>
		void KillAllSlots(F&& onReceiverGone) noexcept
		{
			if (m_emitDepth != 0)
			{
				// Emit 期间：只能标记，回调可能正在栈上执行
				for (Usize index = 0; index < m_slot.size(); ++index)
				{
					Slot& slot = m_slot[index];

					if (!slot.IsAlive())
						continue;

					if (slot.Receiver != nullptr)
						onReceiverGone(slot.Receiver);

					if (slot.Scoped != nullptr)
					{
						slot.Scoped->Signal = nullptr;
						slot.Scoped = nullptr;
					}

					slot.Receiver = nullptr;
					slot.Dead = true;
				}

				m_hasDeadSlots = true;
				return;
			}

			// 没有 Emit 在遍历：可以安全地整体摘出（接收者的清理回调会反向操作 m_slot），
			// 摘出的槽位随本函数返回而销毁，此刻没有任何回调在栈上
			std::deque<Slot> slots = std::move(m_slot);
			m_slot.clear();
			m_hasDeadSlots = false;

			for (Slot& slot : slots)
			{
				if (slot.Receiver != nullptr)
					onReceiverGone(slot.Receiver);

				if (slot.Scoped != nullptr)
				{
					slot.Scoped->Signal = nullptr;
					slot.Scoped = nullptr;
				}

				slot.Receiver = nullptr;
				slot.Dead = true;
			}
		}

		/// @brief 只有当前没有 Emit 在遍历时才真正销毁失效槽位
		void TryCompact() noexcept
		{
			if (m_emitDepth == 0 && m_hasDeadSlots)
				Compact();
		}

		/// @note std::deque：push_back 不搬移已有元素，回调里 Connect 新槽位也不会
		///       让正在执行的那个回调被搬家
		std::deque<Slot> m_slot;
		Usize m_emitDepth = 0;
		bool m_hasDeadSlots = false;
	private:
		void Compact() noexcept
		{
			std::erase_if(m_slot, [](const Slot& slot) noexcept { return !slot.IsAlive(); });
			m_hasDeadSlots = false;
		}
	};

	/// @brief 对象信号
	template <typename... Args>
	class ObjectSignal : public SignalBase, private SignalSlotTable<std::move_only_function<void(Args...)>>
	{
		using SlotTable = SignalSlotTable<std::move_only_function<void(Args...)>>;

		static_assert(!(std::is_void_v<Args> || ...),
			"void 不能与其它类型并列");
	public:
		using CallbackType = std::move_only_function<void(Args...)>;

		/// @note 推荐用默认成员初始化器传入 this，派生类构造函数无需写初始化列表
		explicit ObjectSignal(SignalObject* owner) noexcept : SignalBase(owner) {}

		/// 禁止移动：信号以自身地址为身份，接收者侧的登记记的就是这个地址
		ObjectSignal(const ObjectSignal&) noexcept = delete;
		ObjectSignal& operator=(const ObjectSignal&) noexcept = delete;
		ObjectSignal(ObjectSignal&&) noexcept = delete;
		ObjectSignal& operator=(ObjectSignal&&) noexcept = delete;

		virtual ~ObjectSignal() noexcept override;

		/// @brief 把接收者的槽位挂到本信号上，并在接收者侧登记一条反向记录
		/// @param receiver 接收者，之后可作为断开依据
		/// @param f 槽位，必须能以 Args... 调用
		template <typename F> requires std::invocable<F&, Args...>
		void Connect(SignalObject* receiver, F&& f);

		/// @brief 建立一个不需要接收者身份的作用域连接；析构返回的 ScopedConnection 即断开
		/// @param f 槽位，必须能以 Args... 调用
		/// @note 返回值必须被持有（或者立即用于构造等待者），否则会立刻断开
		template <typename F> requires std::invocable<F&, Args...>
		[[nodiscard]] ScopedConnection ConnectScoped(F&& f);

		/// @brief 订阅"信号源（所属对象）析构"，让等待者/缓存有个了结
		/// @note 对 DestroySignal 本身调用等价于订阅它自己
		template <typename F> requires std::is_nothrow_invocable_v<F&, SignalObject*>
		[[nodiscard]] ScopedConnection ConnectSourceGone(F&& f);

		/// @brief 等待本信号的下一次 Emit，恢复时得到当时的参数元组
		/// @note 只能等到"下一次"；等待期间信号源析构会以异常了结（见 SignalAwaitable）
		[[nodiscard]] SignalAwaitable<ObjectSignal<Args...>> operator co_await() noexcept
		{
			return SignalAwaitable<ObjectSignal<Args...>>{ *this };
		}

		/// @brief 发出信号
		/// @note 只遍历进入本次 Emit 时已存在的槽位：期间新增的槽位等下一次 Emit，
		///       期间的断开只标记失效；因此回调里 Connect/Disconnect/销毁接收者都不会打断遍历
		void Emit(Args... args)
		{
			const typename SlotTable::EmitGuard guard(*this);

			const Usize slotCount = this->m_slot.size();

			for (Usize index = 0; index < slotCount; ++index)
			{
				typename SlotTable::Slot& slot = this->m_slot[index];

				if (slot.IsAlive())
					slot.Callback(args...);
			}
		}

		void Disconnect(SignalObject* receiver) override;
		void ForgetReceiver(SignalObject* receiver) noexcept override;
		void DisconnectScoped(ScopedSlotToken& token) noexcept override;

		/// @brief 断开本信号上的全部接收者
		void DisconnectAll() noexcept;
	protected:
		using Slot = typename SlotTable::Slot;
		using SlotTable::FindSlotOfReceiver;
		using SlotTable::FindSlotOfToken;
		using SlotTable::KillSlot;
		using SlotTable::KillAllSlots;
		using SlotTable::TryCompact;

		virtual void NotifyOwnerGoneImpl() noexcept override;
	};

	/// @brief 不抛异常的对象信号，用于析构等不允许抛出的路径
	template <typename... Args>
	class NothrowObjectSignal : public SignalBase, private SignalSlotTable<std::move_only_function<void(Args...)>>
	{
		using SlotTable = SignalSlotTable<std::move_only_function<void(Args...)>>;

		static_assert(!(std::is_void_v<Args> || ...),
			"信号参数中不允许出现 void：无参数请写 NothrowObjectSignal<void> 或 NothrowObjectSignal<>，void 不能与其它类型并列");
	public:
		using CallbackType = std::move_only_function<void(Args...)>;

		explicit NothrowObjectSignal(SignalObject* owner) noexcept : SignalBase(owner) {}

		NothrowObjectSignal(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal& operator=(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal(NothrowObjectSignal&&) noexcept = delete;
		NothrowObjectSignal& operator=(NothrowObjectSignal&&) noexcept = delete;

		~NothrowObjectSignal() noexcept override;

		template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
		void Connect(SignalObject* receiver, F&& f);

		/// @brief 建立一个不需要接收者身份的作用域连接；析构返回的 ScopedConnection 即断开
		template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
		[[nodiscard]] ScopedConnection ConnectScoped(F&& f);

		/// @brief 订阅"信号源（所属对象）析构"，让等待者/缓存有个了结
		/// @note 对 DestroySignal 本身调用等价于订阅它自己
		template <typename F> requires std::is_nothrow_invocable_v<F&, SignalObject*>
		[[nodiscard]] ScopedConnection ConnectSourceGone(F&& f);

		/// @brief 等待本信号的下一次 Emit，恢复时得到当时的参数元组
		/// @note 只能等到"下一次"；等待期间信号源析构会以异常了结（见 SignalAwaitable）
		[[nodiscard]] SignalAwaitable<NothrowObjectSignal<Args...>> operator co_await() noexcept
		{
			return SignalAwaitable<NothrowObjectSignal<Args...>>{ *this };
		}

		/// @brief 发出信号
		/// @note 只遍历进入本次 Emit 时已存在的槽位，行为与 ObjectSignal::Emit 一致
		void Emit(Args... args) noexcept
		{
			const typename SlotTable::EmitGuard guard(*this);

			const Usize slotCount = this->m_slot.size();

			for (Usize index = 0; index < slotCount; ++index)
			{
				typename SlotTable::Slot& slot = this->m_slot[index];

				if (slot.IsAlive())
					slot.Callback(args...);
			}
		}

		void Disconnect(SignalObject* receiver) override;
		void ForgetReceiver(SignalObject* receiver) noexcept override;
		void DisconnectScoped(ScopedSlotToken& token) noexcept override;

		void DisconnectAll() noexcept;
	protected:
		using Slot = typename SlotTable::Slot;
		using SlotTable::FindSlotOfReceiver;
		using SlotTable::FindSlotOfToken;
		using SlotTable::KillSlot;
		using SlotTable::KillAllSlots;
		using SlotTable::TryCompact;

		void NotifyOwnerGoneImpl() noexcept override;
	};

	template <>
	class ObjectSignal<void> final : public ObjectSignal<>
	{
	public:
		explicit ObjectSignal(SignalObject* owner) noexcept : ObjectSignal<>(owner) {}

		ObjectSignal(const ObjectSignal&) noexcept = delete;
		ObjectSignal& operator=(const ObjectSignal&) noexcept = delete;
		ObjectSignal(ObjectSignal&&) noexcept = delete;
		ObjectSignal& operator=(ObjectSignal&&) noexcept = delete;
	};

	template <>
	class NothrowObjectSignal<void> final : public NothrowObjectSignal<>
	{
	public:
		explicit NothrowObjectSignal(SignalObject* owner) noexcept : NothrowObjectSignal<>(owner) {}

		NothrowObjectSignal(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal& operator=(const NothrowObjectSignal&) noexcept = delete;
		NothrowObjectSignal(NothrowObjectSignal&&) noexcept = delete;
		NothrowObjectSignal& operator=(NothrowObjectSignal&&) noexcept = delete;
	};

	class SignalObject
	{
		template <typename... Args>
		friend class ObjectSignal;
		template <typename... Args>
		friend class NothrowObjectSignal;
	public:
		NothrowObjectSignal<SignalObject*> DestroySignal{ this };

		SignalObject() noexcept = default;

		SignalObject(const SignalObject&) = delete;
		SignalObject(SignalObject&&) = delete;
		SignalObject& operator=(const SignalObject&) = delete;
		SignalObject& operator=(SignalObject&&) = delete;

		virtual ~SignalObject() noexcept
		{
			DisconnectAll();
		}

		/// @brief 显式连接：本对象作为接收者，把自己的槽位挂到发送者的信号上
		/// @param signal 发送者对象持有的信号，句法上即"谁发的信号"
		/// @param f 槽位，其可调用性由信号自身的参数列表约束
		/// @note 连接持续到任一方销毁，或显式调用 Disconnect
		/// @note 接收者就是本对象，因此参数里不再需要 sender 一栏
		template <typename F, typename... Args> requires std::invocable<F&, Args...>
		void Connect(ObjectSignal<Args...>& signal, F&& f)
		{
			signal.Connect(this, std::forward<F>(f));
		}

		template <typename F, typename... Args> requires std::is_nothrow_invocable_v<F&, Args...>
		void Connect(NothrowObjectSignal<Args...>& signal, F&& f)
		{
			signal.Connect(this, std::forward<F>(f));
		}

		/// @brief 断开本对象在指定信号上的连接
		void Disconnect(SignalBase& signal) noexcept
		{
			// 信号侧声明是不完整类型，虚调用必须留在本翻译单元
			signal.Disconnect(this);
		}

		/// @brief 断开本对象在所有信号上的全部连接
		/// @note 析构亦复用此路径
		void DisconnectAll() noexcept
		{
			// 只让信号清理槽位，登记表由本对象自行维护：
			// 此时本对象可能正在析构，不能让信号回写 m_connectedSignal
			// 先遍历再清空，避免 ForgetReceiver 期间改动被遍历的容器
			for (SignalBase* signal : m_connectedSignal)
				signal->ForgetReceiver(this);

			m_connectedSignal.clear();
		}
	private:
		/// @brief 在接收者侧登记一条连接；信号侧保证同一信号只调用一次
		void InternalAddConnection(SignalBase* signal)
		{
			DEBUG_VERIFY_REPORT(signal != nullptr, "Signal must not be null");

			m_connectedSignal.emplace_back(signal);
		}

		/// @brief 移除本对象在指定信号上的登记
		/// @note 只动本对象的登记表；信号侧的槽位由调用方先行清理
		void InternalRemoveConnection(SignalBase* signal) noexcept
		{
			// 信号侧的槽位由调用方先行清理：无论是信号正在遍历槽位，还是刚刚摘出槽位，
			// 回头调用 signal->ForgetReceiver 都是多余的，且会重入同一份容器
			std::erase(m_connectedSignal, signal);
		}

		/// 自身监听的信号：连接关系的唯一真相来源在信号自己的槽位表里，本表是接收者侧的清理路径
		std::vector<SignalBase*> m_connectedSignal;
	};


	template <typename... Args>
	ObjectSignal<Args...>::~ObjectSignal() noexcept
	{
		NotifyOwnerGoneImpl();
	}

	template <typename... Args>
	template <typename F> requires std::invocable<F&, Args...>
	void ObjectSignal<Args...>::Connect(SignalObject* receiver, F&& f)
	{
		DEBUG_VERIFY_REPORT(receiver != nullptr, "Signal receiver must not be null");
		DEBUG_VERIFY_REPORT(receiver != m_owner, "An object must not connect to its own signal");

		// 上一轮留下的失效槽位可以先回收，避免槽位表只增不减
		TryCompact();

		if (Slot* existing = FindSlotOfReceiver(receiver); existing != nullptr)
		{
			existing->Callback = CallbackType(std::forward<F>(f));
			return;
		}

		this->m_slot.emplace_back(receiver, nullptr, CallbackType(std::forward<F>(f)));

		if (receiver != m_owner)
			receiver->InternalAddConnection(this);
	}

	template <typename... Args>
	template <typename F> requires std::invocable<F&, Args...>
	[[nodiscard]] ScopedConnection ObjectSignal<Args...>::ConnectScoped(F&& f)
	{
		TryCompact();

		auto token = std::make_unique<ScopedSlotToken>();
		token->Signal = this;

		this->m_slot.emplace_back(nullptr, token.get(), CallbackType(std::forward<F>(f)));

		ScopedConnection connection;
		connection.AdoptToken(std::move(token));
		return connection;
	}

	template <typename... Args>
	template <typename F> requires std::is_nothrow_invocable_v<F&, SignalObject*>
	[[nodiscard]] ScopedConnection ObjectSignal<Args...>::ConnectSourceGone(F&& f)
	{
		DEBUG_VERIFY_REPORT(m_owner != nullptr, "A signal must be owned by a SignalObject");

		return m_owner->DestroySignal.ConnectScoped(std::forward<F>(f));
	}

	template <typename... Args>
	void ObjectSignal<Args...>::Disconnect(SignalObject* receiver)
	{
		ForgetReceiver(receiver);

		if (receiver != m_owner)
			receiver->InternalRemoveConnection(this);
	}

	template <typename... Args>
	void ObjectSignal<Args...>::ForgetReceiver(SignalObject* receiver) noexcept
	{
		// 只标记失效，不就地擦除：本函数可能在别人（甚至自己）的 Emit 回调里被调用
		for (Slot& slot : this->m_slot)
		{
			if (slot.IsAlive() && slot.Receiver == receiver)
				KillSlot(slot);
		}

		TryCompact();
	}

	template <typename... Args>
	void ObjectSignal<Args...>::DisconnectScoped(ScopedSlotToken& token) noexcept
	{
		if (Slot* slot = FindSlotOfToken(&token); slot != nullptr)
			KillSlot(*slot);
		else
			token.Signal = nullptr; // 槽位已被整表撤掉，令牌只作废

		TryCompact();
	}

	template <typename... Args>
	void ObjectSignal<Args...>::DisconnectAll() noexcept
	{
		KillAllSlots([this](SignalObject* receiver) noexcept
		{
			if (receiver != m_owner)
				receiver->InternalRemoveConnection(this);
		});
	}

	template <typename... Args>
	void ObjectSignal<Args...>::NotifyOwnerGoneImpl() noexcept
	{
		// 发送者销毁路径：通知每个接收者放弃对本信号的登记，并让作用域连接失效。
		// 槽位表已被摘出，接收者侧的清理不会回到这里
		KillAllSlots([this](SignalObject* receiver) noexcept
		{
			if (receiver != m_owner)
				receiver->InternalRemoveConnection(this);
		});
	}

	template <typename... Args>
	NothrowObjectSignal<Args...>::~NothrowObjectSignal() noexcept
	{
		NotifyOwnerGoneImpl();
	}

	template <typename... Args>
	template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
	void NothrowObjectSignal<Args...>::Connect(SignalObject* receiver, F&& f)
	{
		DEBUG_VERIFY_REPORT(receiver != nullptr, "Signal receiver must not be null");

		TryCompact();

		// 同一接收者只占一个槽位，重复 Connect 视为替换
		if (Slot* existing = FindSlotOfReceiver(receiver); existing != nullptr)
		{
			existing->Callback = CallbackType(std::forward<F>(f));
			return;
		}

		this->m_slot.emplace_back(receiver, nullptr, CallbackType(std::forward<F>(f)));

		if (receiver != m_owner)
			receiver->InternalAddConnection(this);
	}

	template <typename... Args>
	template <typename F> requires std::is_nothrow_invocable_v<F&, Args...>
	[[nodiscard]] ScopedConnection NothrowObjectSignal<Args...>::ConnectScoped(F&& f)
	{
		TryCompact();

		auto token = std::make_unique<ScopedSlotToken>();
		token->Signal = this;

		this->m_slot.emplace_back(nullptr, token.get(), CallbackType(std::forward<F>(f)));

		ScopedConnection connection;
		connection.AdoptToken(std::move(token));
		return connection;
	}

	template <typename... Args>
	template <typename F> requires std::is_nothrow_invocable_v<F&, SignalObject*>
	[[nodiscard]] ScopedConnection NothrowObjectSignal<Args...>::ConnectSourceGone(F&& f)
	{
		DEBUG_VERIFY_REPORT(m_owner != nullptr, "A signal must be owned by a SignalObject");

		return m_owner->DestroySignal.ConnectScoped(std::forward<F>(f));
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::Disconnect(SignalObject* receiver)
	{
		ForgetReceiver(receiver);

		if (receiver != m_owner)
			receiver->InternalRemoveConnection(this);
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::ForgetReceiver(SignalObject* receiver) noexcept
	{
		for (Slot& slot : this->m_slot)
		{
			if (slot.IsAlive() && slot.Receiver == receiver)
				KillSlot(slot);
		}

		TryCompact();
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::DisconnectScoped(ScopedSlotToken& token) noexcept
	{
		if (Slot* slot = FindSlotOfToken(&token); slot != nullptr)
			KillSlot(*slot);
		else
			token.Signal = nullptr;

		TryCompact();
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::DisconnectAll() noexcept
	{
		KillAllSlots([this](SignalObject* receiver) noexcept
		{
			if (receiver != m_owner)
				receiver->InternalRemoveConnection(this);
		});
	}

	template <typename... Args>
	void NothrowObjectSignal<Args...>::NotifyOwnerGoneImpl() noexcept
	{
		KillAllSlots([this](SignalObject* receiver) noexcept
		{
			if (receiver != m_owner)
				receiver->InternalRemoveConnection(this);
		});
	}
}
