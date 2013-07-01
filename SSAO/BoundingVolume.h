#pragma once

#include <D3dx9math.h>

enum ContainmentType 
{
	CT_Disjoint,
	CT_Contains,
	CT_Intersects
};

class BoundingBox
{
public:

	BoundingBox() : Defined(false) { }

	BoundingBox(const D3DXVECTOR3& min, const D3DXVECTOR3& max)
		: Min(min), Max(max), Defined(true) { }

	BoundingBox(const BoundingBox& rhs)
		: Min(rhs.Min), Max(rhs.Max), Defined(rhs.Defined) { }

	BoundingBox& operator = (const BoundingBox& rhs)
	{
		if (this != &rhs)
		{
			Max = rhs.Max;
			Min = rhs.Min;
			Defined = rhs.Defined;
		}
		return *this;
	}

	bool operator == ( const BoundingBox& rhs)
	{
		return (Defined == rhs.Defined) && (Min == rhs.Min) && (Max == rhs.Max);
	}

	/**
	 * Reset the bounding box to undefined.
	 */
	void SetNull()	{ Defined = false; }

	/**
	 * Return center of box.
	 */
	D3DXVECTOR3 Center() const { return (Min + Max) * float(0.5); }

	/**
	 * Merge a point
	 */
	void Merge( const D3DXVECTOR3& point )
	{
		if (!Defined)
		{
			Max = Min = point;
			Defined = true;
			return;
		}

		if (point.x < Min.x)	Min.x = point.x;
		if (point.y < Min.y)	Min.y = point.y;
		if (point.z < Min.z)	Min.z = point.z;

		if (point.x > Max.x)	Max.x = point.x;
		if (point.y > Max.y)	Max.y = point.y;
		if (point.z > Max.z)	Max.z = point.z;
	}
	
	/**
	 * Merge another box
	 */
	void Merge( const BoundingBox& box )
	{
		// do nothing if the rhs box id undefined.
		if (!box.Defined)
		{
			return;
		}
		// Otherwise if current null, just take rhs box
		else if (!Defined)
		{
			Min = box.Min;
			Max = box.Max;
			Defined = true;
			return;
		}
		// Otherwise merge
		else
		{
			if (box.Min.x < Min.x)	Min.x = box.Min.x;
			if (box.Min.y < Min.y)	Min.y = box.Min.y;
			if (box.Min.z < Min.z)	Min.z = box.Min.z;

			if (box.Max.x > Max.x)	Max.x = box.Max.x;
			if (box.Max.y > Max.y)	Max.y = box.Max.y;
			if (box.Max.z > Max.z)	Max.z = box.Max.z;
		}
	}

	/**
	 * Determines whether contains the specified box.
	 */
	ContainmentType Contains( const BoundingBox& box )
	{
		if( Max.x < box.Min.x || Min.x > box.Max.x )
			return CT_Disjoint;

		if( Max.y < box.Min.y || Min.y > box.Max.y )
			return CT_Disjoint;

		if( Max.z < box.Min.z || Min.z > box.Max.z )
			return CT_Disjoint;

		if( Min.x <= box.Min.x && box.Max.x <= Max.x && 
			Min.y <= box.Min.y && box.Max.y <= Max.y &&
			Min.z <= box.Min.z && box.Max.z <= Max.z )
			return CT_Contains;

		return CT_Intersects;
	}

	/**
	 * Determines whether contains the specified point.
	 */
	ContainmentType Contains( const D3DXVECTOR3& point )
	{
		if( Min.x <= point.x && point.x <= Max.x && Min.y <= point.y && 
			point.y <= Max.y && Min.z <= point.z && point.z <= Max.z )
			return CT_Contains;

		return CT_Disjoint;
	}

	/**
	 * Determines whether a sphere intersects the specified object.
	 */
	bool Intersects( const BoundingBox& box )
	{
		if ( Max.x < box.Min.x || Min.x > box.Max.x )
			return false;

		if ( Max.y < box.Min.y || Min.y > box.Max.y )
			return false;

		return ( Max.z >= box.Min.z && Min.z <= box.Max.z );
	}

	/**
	 * Return which axis has maximum extent, 0-x, 1-y, 2-z
	 */
	int MaximumExtent() const 
	{
		D3DXVECTOR3 diag = Max - Min;

		if (diag.x > diag.y && diag.x > diag.z)
			return 0;
		else if (diag.y > diag.z)
			return 1;
		else
			return 2;
	}

	/**
	 * Return which axis has maximum extent, 0-x, 1-y, 2-z
	 */
	D3DXVECTOR3 Extent() const 
	{
		return Max - Min;
	}

	/**
	 * Return Surface of this box
	 */
	float SurfaceArea() const 
	{
		D3DXVECTOR3 diag = Max - Min;
		return float(2) * (diag.x * diag.y + diag.x * diag.z + diag.y * diag.z);
	}


public:
	D3DXVECTOR3 Max, Min;

private:
	bool Defined;
};

