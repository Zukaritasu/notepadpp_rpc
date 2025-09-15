// Copyright (C) 2022 - 2025 Zukaritasu
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

#include <string>

class StringBuilder
{
private:
	size_t _length;
	size_t _count;
	char*  _buf;

public:
	StringBuilder(const StringBuilder&) = delete;
	StringBuilder(char* buf, size_t length);

	void Append(const std::string& str);
	void Append(char c);
	bool IsFull() const;
};

inline StringBuilder::StringBuilder(char* buf, size_t length)
{
	_buf = buf;
	_length = length;
	_count = 0;
}

inline void StringBuilder::Append(const std::string& str)
{
	for (char c : str)
	{
		Append(c);
		if (IsFull())
		{
			break;
		}
	}
}

inline void StringBuilder::Append(char c)
{
	if ((_count + 2) < _length)
	{
		_buf[_count++] = c;
		_buf[_count] = '\0';
	}
}

inline bool StringBuilder::IsFull() const
{
	return _count >= _length - 1;
}

