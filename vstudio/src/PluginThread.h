// Copyright (C) 2022 Zukaritasu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <Windows.h>
#include <exception>
#include <stdio.h>

////////////////////////////////////////////////////////////////////

typedef void(*BASIC_THREAD_ROUTINE)(void*, volatile bool*);

class BasicThread {
public:
	BasicThread(BASIC_THREAD_ROUTINE run, void* data = nullptr)
		: data(data), keepRunning(new volatile bool(true)), func(run)
	{
		handle = ::CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Run, this, 0, nullptr);
		if (!handle) {
			throw std::exception("CreateThread() returns NULL");
		}
	}

	~BasicThread() {
		Terminate();
		delete keepRunning;
	}

	void Terminate() const {
		DWORD exitCode;
		if (!::GetExitCodeThread(handle, &exitCode)) {
			*keepRunning = false;
			::WaitForSingleObject(handle, INFINITE);
		}

		::CloseHandle(handle);
	}

	void Wait(DWORD milliseconds = INFINITE) const {
		DWORD exitCode;
		if (!::GetExitCodeThread(handle, &exitCode)) {
			::WaitForSingleObject(handle, milliseconds);
		}
	}

	void Stop() {
		*keepRunning = false;
	}

	static void Sleep(volatile bool* keepRunning, DWORD totalMilliseconds) {
		if (!keepRunning || !(*keepRunning) || totalMilliseconds == 0)
			return;

		HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
		if (!timer) return;

		LARGE_INTEGER li{};
		li.QuadPart = -static_cast<LONGLONG>(totalMilliseconds) * 10000;

		if (!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)) {
			CloseHandle(timer);
			return;
		}

		const DWORD checkInterval = 10;
		DWORD waited = 0;

		while (waited < totalMilliseconds && *keepRunning) {
			DWORD result = WaitForSingleObject(timer, checkInterval);
			if (result == WAIT_OBJECT_0) break;
			waited += checkInterval;
		}

		CloseHandle(timer);
	}


private:
	static DWORD CALLBACK Run(void* param) {
		BasicThread* thread = static_cast<BasicThread*>(param);
		thread->func(thread->data, thread->keepRunning);
		return 0;
	}

	BASIC_THREAD_ROUTINE func;
	void* data;
	volatile bool* keepRunning;
	HANDLE handle;
};


//GetExitCodeThread

////////////////////////////////////////////////////////////////////

class BasicMutex {
public:
	BasicMutex() {
		::InitializeCriticalSection(&criticalSection);
	}

	~BasicMutex() {
		::DeleteCriticalSection(&criticalSection);
	}

	void Lock() {
		::EnterCriticalSection(&criticalSection);
	}

	void Unlock() {
		::LeaveCriticalSection(&criticalSection);
	}

private:
	CRITICAL_SECTION criticalSection;
};
