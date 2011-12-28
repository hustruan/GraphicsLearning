#include "Math.h"
#include <iostream>

const Vector2 Vector2::Zero = Vector2( 0, 0 );
const Vector2 Vector2::Ones = Vector2( 1, 1 );
const Vector2 Vector2::UnitX = Vector2( 1, 0 );
const Vector2 Vector2::UnitY = Vector2( 0, 1 );

std::ostream& operator<<( std::ostream& os, const Vector2& v )
{
	return os << '(' << v.x << ',' << v.y << ')';
}

const Vector3 Vector3::Zero = Vector3( 0, 0, 0 );
const Vector3 Vector3::Ones = Vector3( 1, 1, 1 );
const Vector3 Vector3::UnitX = Vector3( 1, 0, 0 );
const Vector3 Vector3::UnitY = Vector3( 0, 1, 0 );
const Vector3 Vector3::UnitZ = Vector3( 0, 0, 1 );

std::ostream& operator<<( std::ostream& os, const Vector3& v )
{
	return os << '(' << v.x << ',' << v.y << ',' << v.z << ')';
}


void BuildPerspectiveMatrix(float fieldOfView,
                                   float aspectRatio,
                                   float zNear, float zFar,
                                   Matrix4f& matrix)
{
  float sine, cotangent, deltaZ;
  float radians = fieldOfView / 2.0f * PI / 180.0f;
  
  deltaZ = zFar - zNear;
  sine = sin(radians);
  /* Should be non-zero to avoid division by zero. */
  assert(deltaZ);
  assert(sine);
  assert(aspectRatio);
  cotangent = cos(radians) / sine;

  matrix.m[0][0] = cotangent / aspectRatio;
  matrix.m[0][1] =  matrix.m[0][2] = matrix.m[0][3] = 0.0f;

  matrix.m[1][1] = cotangent;
  matrix.m[1][0] =  matrix.m[1][2] = matrix.m[1][3] = 0.0f;

  matrix.m[2][2] =  -(zFar + zNear) / deltaZ;
  matrix.m[2][3] = -2 * zNear * zFar / deltaZ;
  matrix.m[2][0] =  matrix.m[2][1]  = 0.0f;
  
  matrix.m[3][2] = -1;
  matrix.m[3][0] =  matrix.m[3][1] = matrix.m[3][3] = 0.0f;

}

/* Build a row-major (C-style) 4x4 matrix transform based on the
   parameters for gluLookAt. */
void BuildLookAtMatrix(float eyex, float eyey, float eyez,
                              float centerx, float centery, float centerz,
                              float upx, float upy, float upz,
                              Matrix4f& matrix)
{
  float x[3], y[3], z[3], mag;

  /* Difference eye and center vectors to make Z vector. */
  z[0] = eyex - centerx;
  z[1] = eyey - centery;
  z[2] = eyez - centerz;
  /* Normalize Z. */
  mag = sqrt(z[0]*z[0] + z[1]*z[1] + z[2]*z[2]);
  if (mag) {
    z[0] /= mag;
    z[1] /= mag;
    z[2] /= mag;
  }

  /* Up vector makes Y vector. */
  y[0] = upx;
  y[1] = upy;
  y[2] = upz;

  /* X vector = Y cross Z. */
  x[0] =  y[1]*z[2] - y[2]*z[1];
  x[1] = -y[0]*z[2] + y[2]*z[0];
  x[2] =  y[0]*z[1] - y[1]*z[0];

  /* Recompute Y = Z cross X. */
  y[0] =  z[1]*x[2] - z[2]*x[1];
  y[1] = -z[0]*x[2] + z[2]*x[0];
  y[2] =  z[0]*x[1] - z[1]*x[0];

  /* Normalize X. */
  mag = sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
  if (mag) {
    x[0] /= mag;
    x[1] /= mag;
    x[2] /= mag;
  }

  /* Normalize Y. */
  mag = sqrt(y[0]*y[0] + y[1]*y[1] + y[2]*y[2]);
  if (mag) {
    y[0] /= mag;
    y[1] /= mag;
    y[2] /= mag;
  }

  /* Build resulting view matrix. */
  matrix.m[0][0] = x[0];  matrix.m[0][1] = x[1];
  matrix.m[0][2] = x[2];  matrix.m[0][3] = -x[0]*eyex + -x[1]*eyey + -x[2]*eyez;

  matrix.m[1][0] = y[0];  matrix.m[1][1] = y[1];
  matrix.m[1][2] = y[2];  matrix.m[1][3] = -y[0]*eyex + -y[1]*eyey + -y[2]*eyez;

  matrix.m[2][0] = z[0];  matrix.m[2][1] = z[1];
  matrix.m[2][2] = z[2];  matrix.m[2][3] = -z[0]*eyex + -z[1]*eyey + -z[2]*eyez;

  matrix.m[3][0] = 0.0;   matrix.m[3][1] = 0.0;  matrix.m[3][2] = 0.0;  matrix.m[3][3] = 1.0;

}


void MakeTranslateMatrix(float x, float y, float z, Matrix4f& matrix)
{
	matrix.m[0][0]  = 1;  matrix.m[0][1]  = 0;  matrix.m[0][2]  = 0;  matrix.m[0][3]  = x;
	matrix.m[1][0]  = 0;  matrix.m[1][1]  = 1;  matrix.m[1][2]  = 0;  matrix.m[1][3]  = y;
	matrix.m[2][0]  = 0;  matrix.m[2][1]  = 0;  matrix.m[2][2] = 1;   matrix.m[2][3] = z;
	matrix.m[3][0]  = 0;  matrix.m[3][1]  = 0;  matrix.m[3][2] = 0;   matrix.m[3][3] = 1;
}

void MakePlanarShadowMatrix( float planeNormalX, float planeNormalY, float planeNormalZ, float planeDist, float lightX, float lightY, float lightZ, Matrix4f& shadowMatrix )
{
	float nDotl = planeNormalX * lightX + planeNormalY * lightY + planeNormalZ * lightZ ;

	shadowMatrix.m[0][0] = nDotl + planeDist - planeNormalX * lightX;
	shadowMatrix.m[0][1] = -lightX * planeNormalY;
	shadowMatrix.m[0][2] = -lightX * planeNormalZ;
	shadowMatrix.m[0][3] = -lightX * planeDist;

	shadowMatrix.m[1][0] = -lightY * planeNormalX;
	shadowMatrix.m[1][1] = nDotl + planeDist - lightY * planeNormalY;
	shadowMatrix.m[1][2] = -lightY * planeNormalZ;
	shadowMatrix.m[1][3] = -lightY * planeDist;

	shadowMatrix.m[2][0] = -lightZ * planeNormalX;
	shadowMatrix.m[2][1] = -lightZ * planeNormalY;
	shadowMatrix.m[2][2] = nDotl + planeDist - lightZ * planeNormalZ;
	shadowMatrix.m[2][3] = -lightZ * planeDist;

	shadowMatrix.m[3][0] = -planeNormalX;
	shadowMatrix.m[3][1] = -planeNormalY;
	shadowMatrix.m[3][2] = -planeNormalZ;
	shadowMatrix.m[3][3] = nDotl;

	/*float nDotl = planeNormalX * lightX + planeNormalY * lightY + planeNormalZ * lightZ + planeDist * 1;

	shadowMatrix.m[0][0] = nDotl  - planeNormalX * lightX;
	shadowMatrix.m[0][1] = -lightX * planeNormalY;
	shadowMatrix.m[0][2] = -lightX * planeNormalZ;
	shadowMatrix.m[0][3] = -lightX * planeDist;

	shadowMatrix.m[1][0] = -lightY * planeNormalX;
	shadowMatrix.m[1][1] = nDotl  - lightY * planeNormalY;
	shadowMatrix.m[1][2] = -lightY * planeNormalZ;
	shadowMatrix.m[1][3] = -lightY * planeDist;

	shadowMatrix.m[2][0] = -lightZ * planeNormalX;
	shadowMatrix.m[2][1] = -lightZ * planeNormalY;
	shadowMatrix.m[2][2] = nDotl - lightZ * planeNormalZ;
	shadowMatrix.m[2][3] = -lightZ * planeDist;

	shadowMatrix.m[3][0] = -planeNormalX;
	shadowMatrix.m[3][1] = -planeNormalY;
	shadowMatrix.m[3][2] = -planeNormalZ;
	shadowMatrix.m[3][3] = nDotl - planeDist;*/

}

void MakeScaleMatrix( float sX, float sY, float sZ, Matrix4f& matrix )
{
	matrix.m[0][0] = sX;
	matrix.m[0][1] = 0;
	matrix.m[0][2] = 0;
	matrix.m[0][3] = 0;

	matrix.m[1][0] = 0;
	matrix.m[1][1] = sY;
	matrix.m[1][2] = 0;
	matrix.m[1][3] = 0;

	matrix.m[2][0] = 0;
	matrix.m[2][1] = 0;
	matrix.m[2][2] = sZ;
	matrix.m[2][3] = 0;

	matrix.m[3][0] = 0;
	matrix.m[3][1] = 0;
	matrix.m[3][2] = 0;
	matrix.m[3][3] = 1;
}
