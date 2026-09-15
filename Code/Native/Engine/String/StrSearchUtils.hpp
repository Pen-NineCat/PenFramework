// File /Native/Engine/String/StrSearchUtils.hpp
//
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 - Present Pen-NineCat(PenNineCat) https://github.com/Pen-NineCat

// NOTE: 字符串/字符查找的底层实现。
//
// 所有查找都归一化为两类原语：
//   StrFind(XXX)  —— 字符串查找
//   CharFind(XXX) —— 字符查找
// 其中字符串版本按“首字符候选 + 全串校验”的策略由字符版本驱动；
// 集合版本(XXXOf/XXXNotOf)则是带谓词的规范化扫描。
//
// 实现按 sizeof(CharType) 分派：
//   - 定义 PENFRAMEWORK_DISABLE_STRING_VECTOR_FUNC 时：仅 std::char_traits + 手写算法
//   - 否则：运行期使用 SWAR/SSE2 向量化块查找，if consteval 常量求值走标量路径
//
// 所有函数均为 constexpr，可在常量求值期使用。

#pragma once
#include "../Core/Environment.h"
#include <bit>
#include <cstring>
#include <string>
#include <type_traits>

#if defined(PENFRAMEWORK_INTRINSIC_SSE)
#include <emmintrin.h>
#elif defined(PENFRAMEWORK_INTRINSIC_NEON) && defined(__ARM_NEON)
#include <arm_neon.h>
#define PENFRAMEWORK_INTRINSIC_NEON 1
#endif

namespace PenEngine
{
	namespace StringSearchDetail
	{
		constexpr Usize NPos = static_cast<Usize>(-1);

		constexpr Usize MinIndex(Usize a, Usize b) noexcept
		{
			return a < b ? a : b;
		}

		// 字符查找使用的无符号“字”类型
		template <typename CharType>
		using UnsignedChar = std::make_unsigned_t<CharType>;

		template <typename CharType>
		constexpr UnsignedChar<CharType> ToBits(CharType ch) noexcept
		{
			return static_cast<UnsignedChar<CharType>>(ch);
		}

		template <typename CharType>
		constexpr bool LessChar(CharType lhs, CharType rhs) noexcept
		{
			if constexpr (sizeof(CharType) == 1)
				return static_cast<UnsignedChar<CharType>>(lhs) < static_cast<UnsignedChar<CharType>>(rhs);
			else
				return lhs < rhs;
		}

		template <typename CharType>
		constexpr bool EqualChar(CharType lhs, CharType rhs) noexcept
		{
			return lhs == rhs;
		}

		template <typename CharType>
		constexpr Usize Length(const CharType* str) noexcept
		{
			Usize count = 0;
			while (str[count] != CharType())
				++count;
			return count;
		}

		// 先比较公共前缀，返回值语义与 std::char_traits::compare 一致
		template <typename CharType>
		[[nodiscard]] constexpr int CompareChars(const CharType* lhs, const CharType* rhs, Usize count) noexcept
		{
			for (Usize index = 0; index < count; ++index)
			{
				if (LessChar(lhs[index], rhs[index]))
					return -1;
				if (LessChar(rhs[index], lhs[index]))
					return 1;
			}
			return 0;
		}

		template <typename CharType>
		struct EqualsPredicate
		{
			CharType Value;

			constexpr bool operator()(CharType ch) const noexcept
			{
				return ch == Value;
			}
		};

		template <typename CharType>
		struct InSetPredicate
		{
			const CharType* Set;
			Usize SetSize;

			constexpr bool operator()(CharType ch) const noexcept
			{
				for (Usize index = 0; index < SetSize; ++index)
				{
					if (Set[index] == ch)
						return true;
				}
				return false;
			}
		};

		template <typename CharType>
		struct NotInSetPredicate
		{
			const CharType* Set;
			Usize SetSize;

			constexpr bool operator()(CharType ch) const noexcept
			{
				for (Usize index = 0; index < SetSize; ++index)
				{
					if (Set[index] == ch)
						return false;
				}
				return true;
			}
		};

		// STRINGSEARCH_VECTOR: 运行期向量化路径是否可用
#if !defined(PENFRAMEWORK_DISABLE_STRING_VECTOR_FUNC) && \
	(defined(PENFRAMEWORK_INTRINSIC_SSE) || defined(PENFRAMEWORK_INTRINSIC_NEON))
#define STRINGSEARCH_VECTOR 1
#else
#define STRINGSEARCH_VECTOR 0
#endif

		template <typename CharType, typename Predicate>
		constexpr Usize ScalarFind(const CharType* fromStr, Usize fromSize, Predicate pred) noexcept
		{
			for (Usize index = 0; index < fromSize; ++index)
			{
				if (pred(fromStr[index]))
					return index;
			}
			return NPos;
		}

		template <typename CharType, typename Predicate>
		constexpr Usize ScalarRFind(const CharType* fromStr, Usize fromSize, Usize fromPos, Predicate pred) noexcept
		{
			if (fromSize == 0)
				return NPos;

			Usize index = fromPos < fromSize ? fromPos : fromSize - 1;
			for (;;)
			{
				if (pred(fromStr[index]))
					return index;

				if (index == 0)
					break;

				--index;
			}
			return NPos;
		}

#if STRINGSEARCH_VECTOR
		template <typename CharType, Usize CharSize = sizeof(CharType)>
		struct BlockMatch;

		// sizeof == 1
		template <typename CharType>
		struct BlockMatch<CharType, 1>
		{
#ifdef PENFRAMEWORK_INTRINSIC_NEON
			using Vector = uint8x16_t;

			static Vector Broadcast(CharType ch) noexcept
			{
				return vdupq_n_u8(static_cast<U8>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return vld1q_u8(reinterpret_cast<const U8*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(vget_lane_u64(vreinterpret_u64_u8(vceqq_u8(block, pattern)), 0));
			}
#else
			using Vector = __m128i;

			static Vector Broadcast(CharType ch) noexcept
			{
				return _mm_set1_epi8(static_cast<char>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(_mm_movemask_epi8(_mm_cmpeq_epi8(block, pattern)));
			}
#endif
			constexpr static Usize Lanes = 16;
		};

		// sizeof == 2
		template <typename CharType>
		struct BlockMatch<CharType, 2>
		{
#ifdef PENFRAMEWORK_INTRINSIC_NEON
			using Vector = uint16x8_t;

			static Vector Broadcast(CharType ch) noexcept
			{
				return vdupq_n_u16(static_cast<U16>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return vld1q_u16(reinterpret_cast<const U16*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(vget_lane_u64(vreinterpret_u64_u8(vceqq_u16(block, pattern)), 0));
			}
#else
			using Vector = __m128i;

			static Vector Broadcast(CharType ch) noexcept
			{
				return _mm_set1_epi16(static_cast<short>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(_mm_movemask_epi8(_mm_cmpeq_epi16(block, pattern)));
			}
#endif
			constexpr static Usize Lanes = 8;
		};

		// sizeof == 4
		template <typename CharType>
		struct BlockMatch<CharType, 4>
		{
#ifdef PENFRAMEWORK_INTRINSIC_NEON
			using Vector = uint32x4_t;

			static Vector Broadcast(CharType ch) noexcept
			{
				return vdupq_n_u32(static_cast<U32>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return vld1q_u32(reinterpret_cast<const U32*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(vget_lane_u64(vreinterpret_u64_u8(vceqq_u32(block, pattern)), 0));
			}
#else
			using Vector = __m128i;

			static Vector Broadcast(CharType ch) noexcept
			{
				return _mm_set1_epi32(static_cast<int>(ToBits(ch)));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
			}

			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return static_cast<U32>(_mm_movemask_epi8(_mm_cmpeq_epi32(block, pattern)));
			}
#endif
			constexpr static Usize Lanes = 4;
		};

		// sizeof == 8：SWAR 字比较
		template <typename CharType>
		struct BlockMatch<CharType, 8>
		{
			using Vector = U64;

			static Vector Broadcast(CharType ch) noexcept
			{
				return static_cast<U64>(ToBits(ch));
			}

			static Vector Load(const CharType* ptr) noexcept
			{
				U64 word = 0;
				std::memcpy(&word, ptr, sizeof(U64));
				return word;
			}

			// 单通道时命中掩码即通道掩码
			static U32 Mask(Vector block, Vector pattern) noexcept
			{
				return (block ^ pattern) == 0 ? 0x1u : 0x0u;
			}

			constexpr static Usize Lanes = 1;
		};

		template <bool Reverse, typename CharType, typename Predicate>
		constexpr Usize ScanBlock(const CharType* ptr, Predicate pred) noexcept
		{
			constexpr Usize Lanes = BlockMatch<CharType>::Lanes;
			U32 laneHits = 0;

			if constexpr (std::is_same_v<Predicate, EqualsPredicate<CharType>>)
			{
				// 相等谓词：一次广播 + 一次块比较
				using Matcher = BlockMatch<CharType>;
				using Vector = typename Matcher::Vector;

				const Vector pattern = Matcher::Broadcast(pred.Value);
				const Vector block = Matcher::Load(ptr);
				const U32 hits = Matcher::Mask(block, pattern);

				// movemask 每个字节对应 1 位，一个通道占 sizeof(CharType) 个字节位，
				// 因此通道 lane 的最低字节位偏移为 lane * sizeof(CharType)（恒 < 32，不会移位越界）；
				// 命中通道的所有字节位均为 1，只需判定最低位即可
				for (Usize lane = 0; lane < Lanes; ++lane)
				{
					if (((hits >> (lane * sizeof(CharType))) & 1u) != 0)
						laneHits |= 1u << lane;
				}
			}
			else
			{
				// 集合类谓词：逐通道判定，块化访问仍避免边界外的读取
				for (Usize lane = 0; lane < Lanes; ++lane)
				{
					if (pred(ptr[lane]))
						laneHits |= 1u << lane;
				}
			}

			if (laneHits == 0)
				return NPos;

			return Reverse
				? static_cast<Usize>(std::bit_width(laneHits) - 1)
				: static_cast<Usize>(std::countr_zero(laneHits));
		}

		template <typename CharType, typename Predicate>
		constexpr Usize VectorFind(const CharType* fromStr, Usize fromSize, Predicate pred) noexcept
		{
			constexpr Usize Lanes = BlockMatch<CharType>::Lanes;

			Usize index = 0;
			for (; index + Lanes <= fromSize; index += Lanes)
			{
				const Usize hit = ScanBlock<false>(fromStr + index, pred);
				if (hit != NPos)
					return index + hit;
			}

			for (; index < fromSize; ++index)
			{
				if (pred(fromStr[index]))
					return index;
			}
			return NPos;
		}

		template <typename CharType, typename Predicate>
		constexpr Usize VectorRFind(const CharType* fromStr, Usize fromSize, Usize fromPos, Predicate pred) noexcept
		{
			constexpr Usize Lanes = BlockMatch<CharType>::Lanes;

			// 反向块 [blockStart, blockStart + Lanes) 必须完整落在 [0, fromPos] 内
			if (fromPos >= Lanes - 1)
			{
				Usize blockEnd = fromPos + 1;
				while (blockEnd >= Lanes)
				{
					const Usize blockStart = blockEnd - Lanes;
					const Usize hit = ScanBlock<true>(fromStr + blockStart, pred);
					if (hit != NPos)
						return blockStart + hit;
					blockEnd = blockStart;
				}
			}

			if (fromSize == 0)
				return NPos;

			// 未覆盖到的前缀（不足一整块）用标量反向扫描
			return ScalarRFind(fromStr, fromSize, MinIndex(fromPos, fromSize - 1), pred);
		}
#endif // STRINGSEARCH_VECTOR

		template <typename CharType, typename Predicate>
		constexpr Usize FindPredicate(const CharType* fromStr, Usize fromSize, Predicate pred) noexcept
		{
#if STRINGSEARCH_VECTOR
			if consteval
			{
				return ScalarFind(fromStr, fromSize, pred);
			}
			return VectorFind(fromStr, fromSize, pred);
#else
			return ScalarFind(fromStr, fromSize, pred);
#endif
		}

		template <typename CharType, typename Predicate>
		constexpr Usize RFindPredicate(const CharType* fromStr, Usize fromSize, Usize fromPos,
			Predicate pred) noexcept
		{
#if STRINGSEARCH_VECTOR
			if consteval
			{
				return ScalarRFind(fromStr, fromSize, fromPos, pred);
			}
			return VectorRFind(fromStr, fromSize, fromPos, pred);
#else
			return ScalarRFind(fromStr, fromSize, fromPos, pred);
#endif
		}

		// 返回 [fromPos, fromSize) 内的查找区间长度，pos 越界时为 0
		constexpr Usize ForwardLength(Usize fromSize, Usize fromPos) noexcept
		{
			return fromPos < fromSize ? fromSize - fromPos : 0;
		}

		// 返回 Min(fromPos, fromSize - 1)，fromSize == 0 时为 NPos
		constexpr Usize ClampBackwardPos(Usize fromSize, Usize fromPos) noexcept
		{
			if (fromSize == 0)
				return NPos;
			return fromPos < fromSize ? fromPos : fromSize - 1;
		}

		// ================================================================
		// 内部查找：唯一的实现层，正向/反向、单字符/字符串共用
		// 正向：命中从 fromPos 起的最小下标；反向：命中 [0, fromPos] 内的最大下标
		// ================================================================

		template <typename CharType, typename Predicate>
		constexpr Usize InternalFind(const CharType* fromStr, Usize fromSize, Usize fromPos,
			Predicate pred) noexcept
		{
			const Usize searchSize = ForwardLength(fromSize, fromPos);
			if (searchSize == 0)
				return NPos;

			const Usize hit = FindPredicate(fromStr + fromPos, searchSize, pred);
			return hit == NPos ? NPos : hit + fromPos;
		}

		template <typename CharType, typename Predicate>
		constexpr Usize InternalRFind(const CharType* fromStr, Usize fromSize, Usize fromPos,
			Predicate pred) noexcept
		{
			const Usize clamped = ClampBackwardPos(fromSize, fromPos);
			return clamped == NPos ? NPos : RFindPredicate(fromStr, fromSize, clamped, pred);
		}

		template <typename CharType>
		constexpr Usize InternalFindFirstOf(const CharType* chStr, Usize chSize, const CharType* fromStr,
			Usize fromSize, Usize fromPos) noexcept
		{
			return InternalFind(fromStr, fromSize, fromPos, InSetPredicate<CharType>{ chStr, chSize });
		}

		template <typename CharType>
		constexpr Usize InternalFindFirstNotOf(const CharType* chStr, Usize chSize, const CharType* fromStr,
			Usize fromSize, Usize fromPos) noexcept
		{
			// 空集合：首个不属于空集合的字符即 fromPos 本身（与 STL 一致）
			if (chSize == 0)
				return fromPos < fromSize ? fromPos : NPos;
			return InternalFind(fromStr, fromSize, fromPos, NotInSetPredicate<CharType>{ chStr, chSize });
		}

		template <typename CharType>
		constexpr Usize InternalFindLastOf(const CharType* chStr, Usize chSize, const CharType* fromStr,
			Usize fromSize, Usize fromPos) noexcept
		{
			return InternalRFind(fromStr, fromSize, fromPos, InSetPredicate<CharType>{ chStr, chSize });
		}

		template <typename CharType>
		constexpr Usize InternalFindLastNotOf(const CharType* chStr, Usize chSize, const CharType* fromStr,
			Usize fromSize, Usize fromPos) noexcept
		{
			// 空集合：结果被 ClampBackwardPos 限定的最后一个字符（与 STL 一致）
			if (chSize == 0)
				return fromSize == 0 ? NPos : ClampBackwardPos(fromSize, fromPos);
			return InternalRFind(fromStr, fromSize, fromPos, NotInSetPredicate<CharType>{ chStr, chSize });
		}
	}

	// ========================================================================
	// 字符查找：CharFind / CharRFind —— 对外入口，仅接受字符
	// 对外入口一律路由到 StringSearchDetail::InternalFind(XXX)
	// ========================================================================

#define PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(CharType) \
	[[nodiscard]] constexpr Usize CharFind(CharType ch, const CharType* fromStr, Usize fromSize, \
		Usize fromPos = 0) noexcept \
	{ \
		return StringSearchDetail::InternalFind(fromStr, fromSize, fromPos, \
			StringSearchDetail::EqualsPredicate<CharType>{ ch }); \
	} \
	[[nodiscard]] constexpr Usize CharRFind(CharType ch, const CharType* fromStr, Usize fromSize, \
		Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		return StringSearchDetail::InternalRFind(fromStr, fromSize, fromPos, \
			StringSearchDetail::EqualsPredicate<CharType>{ ch }); \
	}


	PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(char)
	PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(wchar_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(char8_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(char16_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS(char32_t)

#undef PENFRAMEWORK_STRING_SEARCH_CHAR_FIND_FUNCS

#define PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(CharType) \
	[[nodiscard]] constexpr Usize CharFindFirstOf(const CharType* chStr, Usize chSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = 0) noexcept \
	{ \
		return StringSearchDetail::InternalFindFirstOf(chStr, chSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize CharFindLastOf(const CharType* chStr, Usize chSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		return StringSearchDetail::InternalFindLastOf(chStr, chSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize CharFindFirstNotOf(const CharType* chStr, Usize chSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = 0) noexcept \
	{ \
		return StringSearchDetail::InternalFindFirstNotOf(chStr, chSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize CharFindLastNotOf(const CharType* chStr, Usize chSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		return StringSearchDetail::InternalFindLastNotOf(chStr, chSize, fromStr, fromSize, fromPos); \
	}
	PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(char)
	PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(wchar_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(char8_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(char16_t)
	PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS(char32_t)

#undef PENFRAMEWORK_STRING_SEARCH_CHAR_SET_FUNCS

#define PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(CharType) \
	[[nodiscard]] constexpr Usize StrFind(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = 0) noexcept \
	{ \
		using namespace StringSearchDetail; \
		if (targetSize == 1) \
			return CharFind(targetStr[0], fromStr, fromSize, fromPos); \
		if (targetSize == 0) \
			return fromPos <= fromSize ? fromPos : NPos; \
		if (targetSize > fromSize || fromPos > fromSize - targetSize) \
			return NPos; \
		Usize index = fromPos; \
		for (;;) \
		{ \
			index = CharFind(targetStr[0], fromStr, fromSize, index); \
			if (index == NPos || index > fromSize - targetSize) \
				return NPos; \
			if (CompareChars(fromStr + index, targetStr, targetSize) == 0) \
				return index; \
			++index; \
		} \
	} \
	[[nodiscard]] constexpr Usize StrRFind(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		using namespace StringSearchDetail; \
		if (targetSize == 1) \
			return CharRFind(targetStr[0], fromStr, fromSize, fromPos); \
		if (targetSize == 0) \
			return fromPos < fromSize ? fromPos : fromSize; \
		if (targetSize > fromSize) \
			return NPos; \
		const Usize maxStart = fromSize - targetSize; \
		Usize index = fromPos < maxStart ? fromPos : maxStart; \
		for (;;) \
		{ \
			index = CharRFind(targetStr[0], fromStr, fromSize, index); \
			if (index == NPos) \
				return NPos; \
			if (CompareChars(fromStr + index, targetStr, targetSize) == 0) \
				return index; \
			if (index == 0) \
				return NPos; \
			--index; \
		} \
	}
	PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(char)
	PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(wchar_t)
	PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(char8_t)
	PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(char16_t)
	PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS(char32_t)

#undef PENFRAMEWORK_STRING_SEARCH_STR_FIND_FUNCS

	// ========================================================================
	// 字符串集合查找：StrFindFirstOf / StrFindLastOf / StrFindFirstNotOf / StrFindLastNotOf
	// 语义与 STL 的 find_first_of(const CharT*) 系列一致：pattern 中任意字符命中即可
	// ========================================================================

#define PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(CharType) \
	[[nodiscard]] constexpr Usize StrFindFirstOf(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = 0) noexcept \
	{ \
		return StringSearchDetail::InternalFindFirstOf(targetStr, targetSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize StrFindLastOf(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		return StringSearchDetail::InternalFindLastOf(targetStr, targetSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize StrFindFirstNotOf(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = 0) noexcept \
	{ \
		return StringSearchDetail::InternalFindFirstNotOf(targetStr, targetSize, fromStr, fromSize, fromPos); \
	} \
	[[nodiscard]] constexpr Usize StrFindLastNotOf(const CharType* targetStr, Usize targetSize, \
		const CharType* fromStr, Usize fromSize, Usize fromPos = static_cast<Usize>(-1)) noexcept \
	{ \
		return StringSearchDetail::InternalFindLastNotOf(targetStr, targetSize, fromStr, fromSize, fromPos); \
	}
	PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(char)
	PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(wchar_t)
	PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(char8_t)
	PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(char16_t)
	PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS(char32_t)

#undef PENFRAMEWORK_STRING_SEARCH_STR_SET_FUNCS
}
