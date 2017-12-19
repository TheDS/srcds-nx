/**
 * vim: set ts=4 :
 * =============================================================================
 * Source Dedicated Server NX
 * Copyright (C) 2011-2017 Scott Ehlert and AlliedModders LLC.
 * All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "Steamworks SDK," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.
 */

#ifndef _INCLUDE_SRCDS_SIGNATURE_H_
#define _INCLUDE_SRCDS_SIGNATURE_H_

#include <utility>

namespace SrcDS::Signature
{
	using Byte = char;
	constexpr Byte wildcard = 0x2A;

	template <Byte... Bytes>
	struct ByteSequence
	{
		static constexpr size_t length = sizeof...(Bytes);
		static constexpr Byte seq[length == 0 ? 1 : length] = { Bytes... };

		template <Byte C>
		using Append = ByteSequence<Bytes..., C>;
	};

	template <char C, typename Enable = void>
	struct HexValue;

	template <char C>
	struct HexValue<C, typename std::enable_if_t<C >= '0' && C <= '9'>>
		: std::integral_constant<Byte, C - '0'> {};

	template <char C>
	struct HexValue<C, typename std::enable_if_t<C >= 'A' && C <= 'F'>>
		: std::integral_constant<Byte, C + 10 - 'A'> {};

	template <char C>
	struct HexValue<C, typename std::enable_if_t<C >= 'a' && C <= 'f'>>
		: std::integral_constant<Byte, C + 10 - 'a'> {};

	template <char C>
	constexpr auto HexValueV = HexValue<C>::value;

	template <Byte Value, char... Chars>
	struct FromHex;

	template <Byte Value, char A, char... Chars>
	struct FromHex<Value, A, Chars...>
		: std::integral_constant<Byte, FromHex<Byte(Value * 16 + HexValueV<A>), Chars...>::value> {};

	template <Byte Value>
	struct FromHex<Value> : std::integral_constant<Byte, Value> {};

	template <char... Chars>
	constexpr auto FromHexV = FromHex<0, Chars...>::value;

	template <typename SigBytes>
	struct Signature
	{
		template <Byte Sig>
		using Append = Signature<typename SigBytes::template Append<Sig>>;

		static constexpr auto pattern   = SigBytes::seq;
		static constexpr auto length    = SigBytes::length;

		static constexpr auto offsetOfWild(size_t which = 1) {
			size_t numWild = 0;
			for (size_t i = 0; i < length; i++) {
				if (pattern[i] == wildcard && ++numWild == which)
					return i;
			}
			return length;
		}
	};

	template <typename Sig, char... Args> struct SigParse;

	template <typename Sig, char... Args>
	struct SigParse<Sig, '?', '?', ' ', Args...>
	{
		using value = typename SigParse<typename Sig::template Append<wildcard>, Args...>::value;
	};

	template <typename Sig, char... Args>
	struct SigParse<Sig, '?', ' ', Args...>
	{
		using value = typename SigParse<typename Sig::template Append<wildcard>, Args...>::value;
	};

	template <typename Sig, char A, char B, char... Args>
	struct SigParse<Sig, A, B, ' ', Args...>
	{
		using value = typename SigParse<typename Sig::template Append<FromHexV<A, B>>,Args...>::value;
	};

	template <typename Sig, char A, char... Args>
	struct SigParse<Sig, A, ' ', Args...>
	{
		using value = typename SigParse<typename Sig::template Append<FromHexV<A>>, Args...>::value;
	};

	template <typename Sig, char... Args>
	struct SigParse<Sig, ' ', Args...>
	{
		using value = typename SigParse<Sig, Args...>::value;
	};

	template <typename Sig>
	struct SigParse<Sig>
	{
		using value = Sig;
	};

	template <char ... Bytes>
	using DoSigParse = typename SigParse<Signature<ByteSequence<>>, Bytes..., ' '>::value;

	template <std::size_t ... N, typename Literal>
	constexpr DoSigParse<Literal::get(N)...> make_signature(std::index_sequence<N...>, Literal)
	{
		return {};
	}
}

#define MAKE_SIG(LITERAL)                                                                 \
  SrcDS::Signature::make_signature(std::make_index_sequence<sizeof(LITERAL)-1>{}, []{     \
    struct Literal {                                                                      \
      static constexpr char get(const int i) { return LITERAL[i]; }                       \
    };                                                                                    \
    return Literal{};                                                                     \
  }())

#endif // _INCLUDE_SRCDS_SIGNATURE_H_
