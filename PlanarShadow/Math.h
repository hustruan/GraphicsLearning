#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>


const float PI = 3.1415926f;

class Matrix4f;
class Vector3;


void MakeTranslateMatrix(float x, float y, float z, Matrix4f& matrix);
void MakeScaleMatrix(float sX, float sY, float sZ, Matrix4f& matrix);

void BuildPerspectiveMatrix(float fieldOfView, float aspectRatio, float zNear, float zFar, Matrix4f& matrix);

void BuildLookAtMatrix(float eyex, float eyey, float eyez, float centerx, float centery, float centerz, 
	float upx, float upy, float upz, Matrix4f& matrix);

void MakePlanarShadowMatrix(float planeNormalX,  float planeNormalY, float planeNormalZ, float planeDist, 
	float lightX, float lightY, float lightZ, Matrix4f& shadowMatrix);



/**
 * A 2d vector.
 */
class Vector2
{
public:

    /**
     * The number of dimensions.
     */
    static const size_t DIM = 2;

    /**
     * The zero vector.
     */
    static const Vector2 Zero;

    /**
     * The vector (1,1).
     */
    static const Vector2 Ones;

    /**
     * The vector (1,0).
     */
    static const Vector2 UnitX;

    /**
     * The vector (0,1).
     */
    static const Vector2 UnitY;

    /**
     * Components of this vector.
     */
    float x, y;

    /**
     * Default constructor. Leaves values unitialized.
     */
    Vector2() {}

    /**
     * Create a vector with the given values.
     */
    Vector2( float x, float y )
        : x( x ), y( y ) {}

    // also uses default copy and assignment

    Vector2 operator+( const Vector2& rhs ) const {
        return Vector2( x + rhs.x, y + rhs.y );
    }

    Vector2& operator+=( const Vector2& rhs ) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vector2 operator-( const Vector2& rhs ) const {
        return Vector2( x - rhs.x, y - rhs.y );
    }

    Vector2& operator-=( const Vector2& rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    Vector2 operator*( float s ) const {
        return Vector2( x * s, y * s );
    }

    Vector2& operator*=( float s ) {
        x *= s;
        y *= s;
        return *this;
    }

    Vector2 operator/( float s ) const {
        float inv = 1.0f / s;
        return Vector2( x * inv, y * inv );
    }

    Vector2& operator/=( float s ) {
        float inv = 1.0f / s;
        x *= inv;
        y *= inv;
        return *this;
    }

    Vector2 operator-() const {
        return Vector2( -x, -y );
    }

    /**
     * @remark No bounds checking.
     */
    const float& operator[]( size_t i ) const {
        // assumes all members are in a contiguous block
        assert( i >= 0 && i < DIM );
        return ( &x )[i];
    }

    /**
     * @remark No bounds checking.
     */
    float& operator[]( size_t i ) {
        // assumes all members are in a contiguous block
        assert( i >= 0 && i < DIM );
        return ( &x )[i];
    }

    bool operator==( const Vector2& rhs ) const {
        return x == rhs.x && y == rhs.y;
    }

    bool operator!=( const Vector2& rhs ) const {
        return !operator==( rhs );
    }

    void to_array( float arr[DIM] ) const {
        arr[0] = float( x );
        arr[1] = float( y );
    }
};

/**
 * Returns the dot product of two vectors
 */
inline float dot( const Vector2& lhs, const Vector2& rhs ) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

/**
 * Efficiency function: does not require square root operation.
 */
inline float squared_length( const Vector2& v ) {
    return v.x * v.x + v.y * v.y;
}

/**
 * Returns the length of a vector.
 */
inline float length( const Vector2& v ) {
    return sqrt( squared_length( v ) );
}

/**
 * Calculate the positive distance between two vectors.
 */
inline float distance( const Vector2& lhs, const Vector2& rhs ) {
    return length( lhs - rhs );
}

/**
 * Efficiency function: does not require square root operation.
 */
inline float squared_distance( const Vector2& lhs, const Vector2& rhs ) {
    return squared_length( lhs - rhs );
}

/**
 * Returns the unit vector pointing in the same direction as this vector.
 */
inline Vector2 normalize( const Vector2& v ) {
    return v / length( v );
}


/**
 * A 3d vector.
 */
class Vector3
{
public:

    /**
     * The number of dimensions.
     */
    static const size_t DIM = 3;

    /**
     * The zero vector.
     */
    static const Vector3 Zero;

    /**
     * The vector (1,1,1).
     */
    static const Vector3 Ones;

    /**
     * The vector (1,0,0)
     */
    static const Vector3 UnitX;

    /**
     * The vector (0,1,0)
     */
    static const Vector3 UnitY;

    /**
     * The vector (0,0,1)
     */
    static const Vector3 UnitZ;

    /**
     * Components of this vector.
     */
    float x, y, z;

    /**
     * Default constructor. Leaves values unitialized.
     */
    Vector3() {}

    /**
     * Create a vector with the given values.
     */
    Vector3( float x, float y, float z )
        : x( x ), y( y ), z( z ) {}


    /**
     * Create a vector from a float array.
     */
    explicit Vector3( const float arr[3] )
        : x( arr[0] ), y( arr[1] ), z( arr[2] ) { }

    // also uses default copy and assignment

    Vector3 operator+( const Vector3& rhs ) const {
        return Vector3( x + rhs.x, y + rhs.y, z + rhs.z );
    }

    Vector3& operator+=( const Vector3& rhs ) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vector3 operator-( const Vector3& rhs ) const {
        return Vector3( x - rhs.x, y - rhs.y, z - rhs.z );
    }

    Vector3& operator-=( const Vector3& rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    Vector3 operator*( float s ) const {
        return Vector3( x * s, y * s, z * s );
    }

    Vector3& operator*=( float s ) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vector3 operator/( float s ) const {
        float inv = 1.0f / s;
        return Vector3( x * inv, y * inv, z * inv );
    }

    Vector3& operator/=( float s ) {
        float inv = 1.0f / s;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    Vector3 operator-() const {
        return Vector3( -x, -y, -z );
    }

    /**
     * @remark No bounds checking.
     */
    const float& operator[]( size_t i ) const {
        // assumes all members are in a contiguous block
        assert( i >= 0 && i < DIM );
        return ( &x )[i];
    }

    /**
     * @remark No bounds checking.
     */
    float& operator[]( size_t i ) {
        // assumes all members are in a contiguous block
        assert( i >= 0 && i < DIM );
        return ( &x )[i];
    }

    bool operator==( const Vector3& rhs ) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    bool operator!=( const Vector3& rhs ) const {
        return !operator==( rhs );
    }

    void to_array( float arr[DIM] ) const {
        arr[0] = float( x );
        arr[1] = float( y );
        arr[2] = float( z );
    }
};

/**
 * Returns the dot product of two vectors
 */
inline float dot( const Vector3& lhs, const Vector3& rhs ) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

/**
 * Returns the cross product of two vectors
 */
inline Vector3 cross( const Vector3& lhs, const Vector3& rhs ) {
    return Vector3(
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    );
}

/**
 * Efficiency function: does not require square root operation.
 */
inline float squared_length( const Vector3& v ) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

/**
 * Returns the length of a vector.
 */
inline float length( const Vector3& v ) {
    return sqrt( squared_length( v ) );
}

/**
 * Calculate the positive distance between two vectors.
 */
inline float distance( const Vector3& lhs, const Vector3& rhs ) {
    return length( lhs - rhs );
}

/**
 * Efficiency function: does not require square root operation.
 */
inline float squared_distance( const Vector3& lhs, const Vector3& rhs ) {
    return squared_length( lhs - rhs );
}

/**
 * Returns the unit vector pointing in the same direction as this vector.
 */
inline Vector3 normalize( const Vector3& v ) {
    return v / length( v );
}


inline Vector3 operator*( float s, const Vector3& rhs ) {
    return rhs * s;
}

/**
 * Outputs a vector text formatted as "(x,y,z)".
 */
std::ostream& operator<<( std::ostream& os, const Vector3& rhs );




class Matrix4f
{
public:
	float m[4][4];

	Matrix4f()
	{        
	}


	inline void InitIdentity()
	{
		m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
		m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
		m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
		m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
	}

	inline Matrix4f operator*(const Matrix4f& Right) const
	{
		Matrix4f Ret;

		for (unsigned int i = 0 ; i < 4 ; i++) {
			for (unsigned int j = 0 ; j < 4 ; j++) {
				Ret.m[i][j] = m[i][0] * Right.m[0][j] +
					m[i][1] * Right.m[1][j] +
					m[i][2] * Right.m[2][j] +
					m[i][3] * Right.m[3][j];
			}
		}

		return Ret;
	}

};
