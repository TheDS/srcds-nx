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

#ifndef _INCLUDE_SRCDS_IDETOUR_H_
#define _INCLUDE_SRCDS_IDETOUR_H_

/**
 * CDetours class for SourceMod Extensions by pRED*
 * Modified by DS to make use of SourceHook's GenBuffer and code generation.
 *
 * detourhelpers.h entirely stolen from CSS:DM and were written by BAILOPAN (I assume).
 * asm.h/c from devmaster.net (thanks cybermind) edited by pRED* to handle gcc -fPIC thunks correctly
 * Concept by Nephyrin Zey (http://www.doublezen.net/) and Windows Detour Library (http://research.microsoft.com/sn/detours/)
 * Member function pointer ideas by Don Clugston (http://www.codeproject.com/cpp/FastDelegate.asp)
 */

#define DETOUR_MEMBER_CALL(name) (this->*name##_Actual)
#define DETOUR_STATIC_CALL(name) (name##_Actual)

#define DETOUR_DECL_STATIC0(name, ret) \
ret (*name##_Actual)(void) = NULL; \
ret name(void)

#define DETOUR_DECL_STATIC1(name, ret, p1type, p1name) \
ret (*name##_Actual)(p1type) = NULL; \
ret name(p1type p1name)

#define DETOUR_DECL_STATIC2(name, ret, p1type, p1name, p2type, p2name) \
ret (*name##_Actual)(p1type, p2type) = NULL; \
ret name(p1type p1name, p2type p2name)

#define DETOUR_DECL_STATIC3(name, ret, p1type, p1name, p2type, p2name, p3type, p3name) \
ret (*name##_Actual)(p1type, p2type, p3type) = NULL; \
ret name(p1type p1name, p2type p2name, p3type p3name)

#define DETOUR_DECL_STATIC4(name, ret, p1type, p1name, p2type, p2name, p3type, p3name, p4type, p4name) \
ret (*name##_Actual)(p1type, p2type, p3type, p4type) = NULL; \
ret name(p1type p1name, p2type p2name, p3type p3name, p4type p4name)

#define DETOUR_DECL_MEMBER0(name, ret) \
class name##Class \
{ \
public: \
ret name(); \
static ret (name##Class::* name##_Actual)(void); \
}; \
ret (name##Class::* name##Class::name##_Actual)(void) = NULL; \
ret name##Class::name()

#define DETOUR_DECL_MEMBER1(name, ret, p1type, p1name) \
class name##Class \
{ \
public: \
ret name(p1type p1name); \
static ret (name##Class::* name##_Actual)(p1type); \
}; \
ret (name##Class::* name##Class::name##_Actual)(p1type) = NULL; \
ret name##Class::name(p1type p1name)

#define DETOUR_DECL_MEMBER2(name, ret, p1type, p1name, p2type, p2name) \
class name##Class \
{ \
public: \
ret name(p1type p1name, p2type p2name); \
static ret (name##Class::* name##_Actual)(p1type, p2type); \
}; \
ret (name##Class::* name##Class::name##_Actual)(p1type, p2type) = NULL; \
ret name##Class::name(p1type p1name, p2type p2name)

#define DETOUR_DECL_MEMBER3(name, ret, p1type, p1name, p2type, p2name, p3type, p3name) \
class name##Class \
{ \
public: \
ret name(p1type p1name, p2type p2name, p3type p3name); \
static ret (name##Class::* name##_Actual)(p1type, p2type, p3type); \
}; \
ret (name##Class::* name##Class::name##_Actual)(p1type, p2type, p3type) = NULL; \
ret name##Class::name(p1type p1name, p2type p2name, p3type p3name)

#define DETOUR_DECL_MEMBER4(name, ret, p1type, p1name, p2type, p2name, p3type, p3name, p4type, p4name) \
class name##Class \
{ \
public: \
ret name(p1type p1name, p2type p2name, p3type p3name, p4type p4name); \
static ret (name##Class::* name##_Actual)(p1type, p2type, p3type, p4type); \
}; \
ret (name##Class::* name##Class::name##_Actual)(p1type, p2type, p3type, p4type) = NULL; \
ret name##Class::name(p1type p1name, p2type p2name, p3type p3name, p4type p4name)

#define GET_MEMBER_CALLBACK(name) (void *)GetCodeAddress(&name##Class::name)
#define GET_MEMBER_TRAMPOLINE(name) (void **)(&name##Class::name##_Actual)

#define GET_STATIC_CALLBACK(name) (void *)&name
#define GET_STATIC_TRAMPOLINE(name) (void **)&name##_Actual

#define DETOUR_CREATE_MEMBER(name, addr) g_ServerAPI->CreateDetour(GET_MEMBER_CALLBACK(name), GET_MEMBER_TRAMPOLINE(name), addr);
#define DETOUR_CREATE_STATIC(name, addr) g_ServerAPI->CreateDetour(GET_STATIC_CALLBACK(name), GET_STATIC_TRAMPOLINE(name), addr);

class GenericClass {};
typedef void (GenericClass::*VoidFunc)();

inline void *GetCodeAddr(VoidFunc mfp)
{
	return *(void **)&mfp;
}

/**
 * Converts a member function pointer to a void pointer.
 * This relies on the assumption that the code address lies at mfp+0
 * This is the case for both g++ and later MSVC versions on non virtual functions but may be different for other compilers
 * Based on research by Don Clugston : http://www.codeproject.com/cpp/FastDelegate.asp
 */
#define GetCodeAddress(mfp) GetCodeAddr(reinterpret_cast<VoidFunc>(mfp))

class IDetour
{
public:
	virtual void Enable() = 0;
	virtual void Disable() = 0;
	virtual bool IsEnabled() = 0;
	virtual void *GetTargetAddress() = 0;
	virtual void Destroy(bool undoPatch = true) = 0;
};

#endif // _INCLUDE_SRCDS_IDETOUR_H_
