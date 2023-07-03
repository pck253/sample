#pragma once

#define	EPSILON (1.e-4f)

template<typename T> requires std::is_arithmetic<T>::value
struct Vector2d
{
	using ValueType_t = T;
	T x = 0;
	T y = 0;

	Vector2d() = default;
	Vector2d(const T& _x, const T& _y) : x(_x), y(_y) {}
	Vector2d(const Vector2d& _other) : x(_other.x), y(_other.y) {}
	Vector2d(Vector2d&& _other) : x(_other.x), y(_other.y) { _other.x = 0; _other.y = 0; }

	Vector2d& operator=(const Vector2d& _other)
	{
		x = _other.x;
		y = _other.y;
		return *this;
	}
	Vector2d& operator=(Vector2d&& _other)
	{
		x = _other.x;
		y = _other.y;
		_other.x = 0;
		_other.y = 0;
		return *this;
	}

	bool operator!=(const Vector2d<T>_other)
	{
		return (x != _other.x || y != _other.y);
	}
	bool operator==(const Vector2d<T>& _other)
	{
		return (x == _other.x && y == _other.y);
	}

	template<typename VT> requires std::is_arithmetic<VT>::value
		Vector2d<T> operator*(const VT& _value)
	{
		return Vector2d<T>((T)(x * _value), (T)(y * _value));
	}

	template<typename VT> requires std::is_arithmetic<VT>::value
		Vector2d<T> operator/(const VT& _value)
	{
		return Vector2d<T>((T)(x / _value), (T)(y / _value));
	}

	Vector2d<T> operator+(const Vector2d<T>& _other)
	{
		return Vector2d<T>(x + _other.x, y + _other.y);
	}

	Vector2d<T> operator-(const Vector2d<T>& _other)
	{
		return Vector2d<T>(x - _other.x, y - _other.y);
	}

	inline bool IsZero() { return ((x + y) == 0); }
};

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

template<typename T> requires std::is_arithmetic<T>::value
struct Vector
{
	using ValueType_t = T;
	T x = 0;
	T y = 0;
	T z = 0;

	Vector() = default;
	Vector(const T& _x, const T& _y, const T& _z) : x(_x), y(_y), z(_z) {}
	Vector(const Vector& _other) : x(_other.x), y(_other.y), z(_other.z) {}
	Vector(Vector&& _other) : x(_other.x), y(_other.y), z(_other.z) { _other.x = 0; _other.y = 0; _other.z = 0; }

	void Reset()
	{
		x = 0;
		y = 0;
		z = 0;
	}

	Vector<T>& operator=(const Vector<T>& _other)
	{
		x = _other.x;
		y = _other.y;
		z = _other.z;
		return *this;
	}
	Vector<T>& operator=(Vector<T>&& _other)
	{
		x = _other.x;
		y = _other.y;
		z = _other.z;
		_other.x = 0;
		_other.y = 0;
		_other.z = 0;
		return *this;
	}

	bool operator!=(const Vector<T>_other)
	{
		return (x != _other.x || y != _other.y || z != _other.z);
	}
	bool operator==(const Vector<T>& _other)
	{
		return (x == _other.x && y == _other.y && z == _other.z);
	}

	template<typename VT> requires std::is_arithmetic<VT>::value
	Vector<T> operator*(const VT& _value) const
	{
		return Vector<T>((T)(x * _value), (T)(y * _value), (T)(z * _value));
	}

	template<typename VT> requires std::is_arithmetic<VT>::value
	Vector<T> operator/(const VT& _value) const
	{
		return Vector<T>((T)(x / _value), (T)(y / _value), (T)(z / _value));
	}

	Vector<T> operator+(const Vector<T>& _other) const
	{
		return Vector<T>(x + _other.x, y + _other.y, z + _other.z);
	}

	Vector<T> operator-(const Vector<T>& _other) const
	{
		return Vector<T>(x - _other.x, y - _other.y, z - _other.z);
	}

	float Magnitude() const
	{
		return (float)sqrtl((x * x) + (y * y) + (z * z));
	}

	double SqrtMagnitude() const
	{
		return ((x * x) + (y * y) + (z * z));
	}
	Vector<T> Normalize()
	{
		double num = Magnitude();
		if (num > EPSILON)
		{
			return (*this) / num;
		}

		return Vector<T>();
	}

	static Vector<T> Lerp(const Vector<T>& _a, const Vector<T>& _b, const float& _delta)
	{
		return Vector<T>(static_cast<T>(_a.x + (_b.x - _a.x) * _delta), static_cast<T>(_a.y + (_b.y - _a.y) * _delta), static_cast<T>(_a.z + (_b.z - _a.z) * _delta));
	}

	inline bool IsZero() { return ((x + y + z) == 0); }
};