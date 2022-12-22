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

class BasicThread
{
public:
	BasicThread(void(*run)(void*), void* data = nullptr);
	BasicThread(const BasicThread&) = delete;
	~BasicThread();

	void Terminate();
	void Wait(DWORD milliseconds = 0);

private:
	struct Params
	{
		void(*func)(void*);
		void* data;
	};

	HANDLE handle;

	static DWORD CALLBACK Run(void*);
};

inline BasicThread::BasicThread(void(*run)(void*), void* data)
{
	Params* params = new Params;
	params->func = run;
	params->data = data;
	handle = ::CreateThread(nullptr, 0, BasicThread::Run, params, 0, nullptr);
	if (handle == nullptr)
	{
		throw std::exception("CreateThread() function returns null");
	}
}

inline BasicThread::~BasicThread()
{
	::CloseHandle(handle);
}

inline void BasicThread::Terminate()
{
	::TerminateThread(handle, EXIT_SUCCESS);
}

inline void BasicThread::Wait(DWORD milliseconds)
{
	::WaitForSingleObject(handle, milliseconds);
}

inline DWORD BasicThread::Run(void* params)
{
	reinterpret_cast<BasicThread::Params*>(params)->func(
		reinterpret_cast<BasicThread::Params*>(params)->data);
	delete params;
	return 0;
}

class BasicMutex
{
public:
	BasicMutex();
	BasicMutex(const BasicMutex&) = delete;
	~BasicMutex();

	void Lock();
	void Unlock();

private:
	HANDLE handle;
};

inline BasicMutex::BasicMutex()
{
	handle = ::CreateMutex(nullptr, FALSE, nullptr);
	if (handle == nullptr)
	{
		throw std::exception("CreateMutex() function returns null");
	}
}

inline BasicMutex::~BasicMutex()
{
	::CloseHandle(handle);
}

inline void BasicMutex::Lock()
{
	::WaitForSingleObject(handle, INFINITE);
}

inline void BasicMutex::Unlock()
{
	::ReleaseMutex(handle);
}
