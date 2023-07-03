#pragma once

class StringUtility
{
public:
	static bool Utf8ToUnicode(const std::string& _src, std::wstring& _dest)
	{
		wchar_t wChar;
		for (size_t i = 0; i < _src.length(); )
		{
			char ch = _src[i];

			if ((ch & 0x80) == 0)
			{
				wChar = ch;
				++i;
			}
			else if ((ch & 0xE0) == 0xC0)
			{
				wChar = (_src[i] & 0x1F) << 6;
				wChar |= (_src[i + 1] & 0x3F);
				i += 2;
			}
			else if ((ch & 0xF0) == 0xE0)
			{
				wChar = (_src[i] & 0xF) << 12;
				wChar |= (_src[i + 1] & 0x3F) << 6;
				wChar |= (_src[i + 2] & 0x3F);
				i += 3;
			}
			else if ((ch & 0xF8) == 0xF0)
			{
				wChar = (_src[i] & 0x7) << 18;
				wChar |= (_src[i + 1] & 0x3F) << 12;
				wChar |= (_src[i + 2] & 0x3F) << 6;
				wChar |= (_src[i + 3] & 0x3F);
				i += 4;
			}
			else
			{
				return false;
			}
			_dest += wChar;
		}

		return true;
	}

	static size_t UnicodeToUtf8(const std::wstring& _src, std::string& _dest)
	{
		size_t length = 0;
		for (size_t i = 0; i < _src.size(); ++i)
		{
			wchar_t wChar = _src[i];
			if (0 <= wChar && wChar <= 0x7f)
			{
				_dest += static_cast<char>(wChar);
				++length;
			}
			else if (0x80 <= wChar && wChar <= 0x7ff)
			{
				_dest += (0xc0 | (wChar >> 6));
				_dest += (0x80 | (wChar & 0x3f));
				++length;
			}
			else if (0x800 <= wChar && wChar <= 0xffff)
			{
				_dest += (0xe0 | (wChar >> 12));
				_dest += (0x80 | ((wChar >> 6) & 0x3f));
				_dest += (0x80 | (wChar & 0x3f));
				++length;
			}
			else if (0x10000 <= wChar && wChar <= 0x1fffff)
			{
				uint32_t temp = wChar;
				_dest += (0xf0 | static_cast<wchar_t>(temp >> 18));
				_dest += (0x80 | ((wChar >> 12) & 0x3f));
				_dest += (0x80 | ((wChar >> 6) & 0x3f));
				_dest += (0x80 | (wChar & 0x3f));
				++length;
			}
			else
			{
				_dest.clear();
				return 0;
			}
		}
		return length;
	}
};