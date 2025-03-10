/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

extern "C" {
#include "SDL_systhread_c.h"
}

#include <system_error>

#include "SDL_sysmutex_c.h"
#include <windows.h>

extern "C"
SDL_Mutex * SDL_CreateMutex(void)
{
    // Allocate and initialize the mutex
    try {
        SDL_Mutex *mutex = new SDL_Mutex;
        return mutex;
    } catch (std::system_error &ex) {
        SDL_SetError("unable to create a C++ mutex: code=%d; %s", ex.code(), ex.what());
    } catch (std::bad_alloc &) {
        SDL_OutOfMemory();
    }
    return NULL;
}

extern "C"
void SDL_DestroyMutex(SDL_Mutex *mutex)
{
    if (mutex) {
        delete mutex;
    }
}

extern "C"
void SDL_LockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    if (mutex) {
        try {
            mutex->cpp_mutex.lock();
        } catch (std::system_error &/*ex*/) {
            SDL_assert(!"Error trying to lock mutex");  // assume we're in a lot of trouble if this assert fails.
        }
    }
}

extern "C"
bool SDL_TryLockMutex(SDL_Mutex *mutex)
{
    bool result = true;
    if (mutex) {
        result = mutex->cpp_mutex.try_lock();
    }
    return result;
}

// Unlock the mutex
extern "C"
void SDL_UnlockMutex(SDL_Mutex *mutex) SDL_NO_THREAD_SAFETY_ANALYSIS  // clang doesn't know about NULL mutexes
{
    if (mutex) {
        mutex->cpp_mutex.unlock();
    }
}

