/* fast-poly2tri - v1.0

  Rewrite of the poly2tri library (https://github.com/jhasse/poly2tri)
  by Unspongeful (@unspongeful).

  Based on the Sweep-line, Constrained Delauney Triangulation (CDT) See: Domiter,
  V. and Zalik, B.(2008)'Sweep-line algorithm for constrained Delaunay
  triangulation', International Journal of Geographical Information Science
  "FlipScan" Constrained Edge Algorithm by Thomas hln, thahlen@gmail.com

  This library's focus is speed. No validation is performed, other than
  basic debug-time assert.
  Passing malformed data will result in a crash, _you must_ pre-validate
  the data.

  NOTE: Unlike unlike poly2tri's original implementation I removed
  the ability to add Steiner points explicitly, but they can be added
  by manipulating the internal state of the context.


  ////////////////////////////////

  To include in your project:
     #define MPE_POLY2TRI_IMPLEMENTATION
  before you include this file in *one* C or C++ file to create the implementation.

  // i.e. it should look like this:
  #include ...
  #define MPE_POLY2TRI_IMPLEMENTATION
  #include "MPE_fastpoly2tri.h"

  On multiple compilation unit builds you must also redefine
  `insternal_static` to extern.

  ////////////////////////////////
  Usage:

    // The maximum number of points you expect to need
    // This value is used by the library to calculate
    // working memory required
    uint32_t MaxPointCount = 100;

    // Request how much memory (in bytes) you should
    // allocate for the library
    size_t MemoryRequired = MPE_PolyMemoryRequired(MaxPointCount);

    // Allocate a void* memory block of size MemoryRequired
    // IMPORTANT: The memory must be zero initialized
    void* Memory = calloc(MemoryRequired, 1);

    // Initialize the poly context by passing the memory pointer,
    // and max number of points from before
    MPEPolyContext PolyContext;
    if (MPE_PolyInitContext(&PolyContext, Memory, MaxPointCount))
    {
      // Populate the points of the polyline for the shape
      // you want to triangulate using one of the following
      // two methods:

      // Option A - One point at a time
      for(...)
      {
        MPEPolyPoint* Point = MPE_PolyPushPoint(&PolyContext);
        Point->X = ...;
        Point->Y = ...;
      }

      // Option B - memcpy
      // This option requires your point data structure to
      // correspond to MPEPolyPoint. See below.
      uint32_t PointCount = ...;
      MPEPolyPoint* FirstPoint = MPE_PolyPushPointArray(&PolyContext, PointCount);
      memcpy(FirstPoint, YOUR_POINT_DATA, sizeof(MPEPolyPoint)*PointCount);

      // IMPORTANT: Both push functions perform no validation other
      // than an assert for bounds checking. You must make sure your
      // point data is correct:
      //  - Duplicate points are not supported
      //  - Bounds checking is not implemented other than debug asserts

      // Add the polyline for the edge. This will consume all points added so far.
      MPE_PolyAddEdge(&PolyContext);

      // If you want to add holes to the shape you can do:
      if (AddHoles)
      {
        MPEPolyPoint* Hole = MPE_PolyPushPointArray(&PolyContext, 4);
        Hole[0].X = 325; Hole[0].Y = 437;
        Hole[1].X = 320; Hole[1].Y = 423;
        Hole[2].X = 329; Hole[2].Y = 413;
        Hole[3].X = 332; Hole[3].Y = 423;
        MPE_PolyAddHole(&PolyContext);
      }

      // Triangulate the shape
      MPE_PolyTriangulate(&PolyContext);

      // The resulting triangles can be used like so
      for (uxx TriangleIndex = 0; TriangleIndex < PolyContext.TriangleCount; ++TriangleIndex)
      {
        MPEPolyTriangle* Triangle = PolyContext.Triangles[TriangleIndex];
        MPEPolyPoint* PointA = Triangle->Points[0];
        MPEPolyPoint* PointB = Triangle->Points[1];
        MPEPolyPoint* PointC = Triangle->Points[2];
      }
      // You may want to copy the resulting triangle soup into
      // your own data structures if you plan on using them often
      // as the Points and Triangles from the PolyContext are not
      // compact in memory.


  ////////////////////////////////
  Configuration defines:

  #define MPE_POLY2TRI_USE_DOUBLE
     To use double precision floating point for calculations

  #define MPE_POLY2TRI_USE_CUSTOM_SORT
     To use the a custom merge sort implementation. Enabling this option will
     require more working memory.

  ////////////////////////////////
  Standard library overrides

  #define MPE_MemorySet, MPE_MemoryCopy
    To avoid including string.h

  #define internal_static
    Defaults to static, but you can change it if not using a unity-style build.

  LICENSE - MIT (same as original license)

*/
#ifndef MPE_POLY2TRI_HEADER
#define MPE_POLY2TRI_HEADER

//SECTION: Engine define overrides

#include <math.h>  // fabs fabsf

#ifndef MPE_MemorySet
  #include <string.h> //memset
  #define MPE_MemorySet memset
  #define MPE_MemoryCopy memcpy
#endif

#ifndef MPE_Assert
  #include <assert.h>
  #define MPE_Assert assert
#endif

#ifndef uxx
  #include <stdint.h> // uint32_t
  typedef uint8_t u8;
  typedef int32_t i32;
  typedef int32_t b32;
  typedef uint32_t u32;
  typedef float f32;
  typedef double f64;
  typedef size_t uxx;
  //typedef ssize_t bxx;
  typedef s64 bxx;
  typedef intptr_t imm;
  typedef uintptr_t umm;
#endif

#ifdef MPE_POLY2TRI_USE_DOUBLE
  typedef double poly_float;
#else
  typedef float poly_float;
#endif

#ifndef internal_static
  #define internal_static static
#endif
#ifndef MPE_INLINE
  #define MPE_INLINE inline
#endif

#ifdef __cplusplus
  extern "C" {
#endif

/////////////////////

typedef struct MPEPolyAllocator
{
  u8* Memory;
  umm Size;
  umm Used;
} MPEPolyAllocator;

typedef struct MPEPolyPoint
{
  struct MPEPolyEdge* FirstEdge;
  poly_float X;
  poly_float Y;
} MPEPolyPoint;

typedef struct MPEPolyTriangle
{
  struct MPEPolyTriangle* Neighbors[3];
  MPEPolyPoint* Points[3];
  uxx Flags;
} MPEPolyTriangle;

typedef struct MPEPolyNode
{
  struct MPEPolyNode* Next;
  struct MPEPolyNode* Prev;
  MPEPolyPoint* Point;
  MPEPolyTriangle* Triangle;
  poly_float Value;
#ifndef MPE_POLY2TRI_USE_DOUBLE
  u32 _PADDING;
#endif
} MPEPolyNode;

typedef struct MPEPolyBasin
{
  MPEPolyNode* LeftNode;
  MPEPolyNode* BottomNode;
  MPEPolyNode* RightNode;
  poly_float Width;
#ifdef MPE_POLY2TRI_USE_DOUBLE
  bxx LeftHighest;
#else
  b32 LeftHighest;
#endif
} MPEPolyBasin;

typedef struct MPEPolyEdge
{
  MPEPolyPoint* P;
  MPEPolyPoint* Q;
  struct MPEPolyEdge* Next;
} MPEPolyEdge;

typedef struct MPEEdgeEvent
{
  MPEPolyEdge* ConstrainedEdge;
  bxx Right;
} MPEEdgeEvent;


typedef struct MPEPolyContext
{
  void* UserData;
  void* Memory;
  MPEPolyAllocator Allocator;
  MPEPolyBasin Basin;
  MPEEdgeEvent EdgeEvent;
  MPEPolyNode* HeadNode;
  MPEPolyNode* TailNode;
  MPEPolyNode* SearchNode;

  MPEPolyTriangle* TrianglePool;
  MPEPolyTriangle** Triangles;
  MPEPolyTriangle** TempTriangles;
  MPEPolyPoint** Points;
  MPEPolyPoint* PointsPool;
  MPEPolyNode* Nodes;

  MPEPolyPoint* HeadPoint;
  MPEPolyPoint* TailPoint;

  u32 MaxPointCount;
  u32 PointPoolCount;
  u32 PointCount;
  u32 TrianglePoolCount;
  u32 TriangleCount;
  u32 NodeCount;
  b32 Valid;
  u32 _PADDING;
} MPEPolyContext;


/////////////////////
// PUBLIC API

internal_static
umm MPE_PolyMemoryRequired(u32 MaxPointCount);

internal_static
b32 MPE_PolyInitContext(MPEPolyContext* PolyContext, void* Memory, u32 MaxPointCount);

internal_static
MPEPolyPoint* MPE_PolyPushPoint(MPEPolyContext* PolyContext);

internal_static
MPEPolyPoint* MPE_PolyPushPointArray(MPEPolyContext* PolyContext, u32 Count);

internal_static
void MPE_PolyAddEdge(MPEPolyContext* PolyContext);

internal_static
void MPE_PolyTriangulate(MPEPolyContext* PolyContext);

//SECTION: Required overrides

#ifdef __cplusplus
}
#endif

#endif // MPE_POLY2TRI_HEADER


#ifdef MPE_POLY2TRI_IMPLEMENTATION

//////////////////////
// IMPLEMENTATION

#define MPE_POLY2TRI_EPSILON (poly_float)1e-12f
#define MPE_POLY2TRI_POINT_COUNT_EPSILON 16

#ifndef MPE_InvalidCodePath
  #define MPE_InvalidCodePath MPE_Assert(0)
#endif

#ifndef MPE_TRUE
  #define MPE_FALSE 0
  #define MPE_TRUE 1
#endif


#ifndef MPE_Assert
  #error "Please #define MPE_Assert to assert from <assert.h> or custom implementation"
#endif
#ifndef MPE_MemorySet
  #error "Please #define MPE_MemorySet to memset from <string.h> or custom implementation"
#endif


#define MPE_PolyPush(Allocator, Type) ((Type*)MPE_PolyPushRaw(&Allocator, sizeof(Type)))
#define MPE_PolyPushArray(Allocator, Type, Count) ((Type*)MPE_PolyPushRaw(&Allocator, sizeof(Type) * Count))

internal_static
u8* MPE_PolyPushRaw(MPEPolyAllocator* Allocator, umm Size);

typedef struct MPEPolyEdges
{
  u8 ConstrainedCW;
  u8 ConstrainedCCW;
  u8 DelaunayCW;
  u8 DelaunayCCW;
} MPEPolyEdges;

enum MPEPolyOrientation
{
  MPEPolyOrientation_CCW,
  MPEPolyOrientation_CW,
  MPEPolyOrientation_Collinear
};

enum MPEPolyTriFlag
{
  MPEPolyTriFlag_DelaunayEdge0    = 1 << 0,
  MPEPolyTriFlag_DelaunayEdge1    = 1 << 1,
  MPEPolyTriFlag_DelaunayEdge2    = 1 << 2,
  MPEPolyTriFlag_ConstrainedEdge0 = 1 << 3,
  MPEPolyTriFlag_ConstrainedEdge1 = 1 << 4,
  MPEPolyTriFlag_ConstrainedEdge2 = 1 << 5,

  MPEPolyTriFlag_IsInterior       = 1 << 30,

  MPEPolyTriFlag_DelaunayEdgeMask = MPEPolyTriFlag_DelaunayEdge0 | MPEPolyTriFlag_DelaunayEdge1 | MPEPolyTriFlag_DelaunayEdge2,
  MPEPolyTriFlag_ConstrainedEdgeMask = MPEPolyTriFlag_ConstrainedEdge0 | MPEPolyTriFlag_ConstrainedEdge1 | MPEPolyTriFlag_ConstrainedEdge2
};


static u32 MPEPolyEdgeLUT[] = {0, 1, 2, 0, 1, 2, 3};

internal_static
u8* MPE_PolyPushRaw(MPEPolyAllocator* Allocator, umm Size)
{
  u8* Result = 0;
  if (Allocator->Used + Size <= Allocator->Size)
  {
    Result = Allocator->Memory + Allocator->Used;
    Allocator->Used += Size;
  }
  else
  {
    MPE_InvalidCodePath;
  }
  return Result;
}


/*
  Formula to calculate signed area
  Positive if CCW
  Negative if CW
  0 if collinear
  A[P1,P2,P3]  =  (x1*y2 - y1*x2) + (x2*y3 - y2*x3) + (x3*y1 - y3*x1)
               =  (x1-x3)*(y2-y3) - (y1-y3)*(x2-x3)
 */

internal_static
uxx MPE_PolyOrient2D(MPEPolyPoint* PointA, MPEPolyPoint* PointB, MPEPolyPoint* PointC)
{
  uxx Result;
  MPE_Assert(PointA != PointB && PointA != PointC && PointB != PointC);
  poly_float DeltaLeft = (PointA->X - PointC->X) * (PointB->Y - PointC->Y);
  poly_float DeltaRight = (PointA->Y - PointC->Y) * (PointB->X - PointC->X);
  poly_float Value = DeltaLeft - DeltaRight;
  if (Value > -MPE_POLY2TRI_EPSILON && Value < MPE_POLY2TRI_EPSILON)
  {
    Result = MPEPolyOrientation_Collinear;
  }
  else if (Value > 0)
  {
    Result = MPEPolyOrientation_CCW;
  }
  else
  {
    Result = MPEPolyOrientation_CW;
  }
  return Result;
}

internal_static
bxx MPE_PolyInScanArea(MPEPolyPoint* PointA, MPEPolyPoint* PointB, MPEPolyPoint* PointC, MPEPolyPoint* PointD)
{
  MPE_Assert(PointA != PointB && PointA != PointC && PointA != PointD &&
             PointB != PointC && PointB != PointD &&
             PointC != PointD);
  poly_float oadb = (PointA->X - PointB->X) * (PointD->Y - PointB->Y) - (PointD->X - PointB->X) * (PointA->Y - PointB->Y);
  bxx Result;
  if (oadb >= -MPE_POLY2TRI_EPSILON)
  {
    Result = MPE_FALSE;
  }
  else
  {
    poly_float oadc = (PointA->X - PointC->X) * (PointD->Y - PointC->Y) - (PointD->X - PointC->X) * (PointA->Y - PointC->Y);
    if (oadc <= MPE_POLY2TRI_EPSILON)
    {
      Result = MPE_FALSE;
    }
    else
    {
      Result = MPE_TRUE;
    }
  }
  return Result;
}

internal_static MPE_INLINE
bxx MPE_PolyContainsPoint(MPEPolyTriangle* Triangle, MPEPolyPoint* Point)
{
  MPE_Assert(Point);
  bxx Result = Point == Triangle->Points[0] || Point == Triangle->Points[1] || Point == Triangle->Points[2];
  return Result;
}

internal_static MPE_INLINE
bxx MPE_PolyContainsPoints(MPEPolyTriangle* Triangle, MPEPolyPoint* Point, MPEPolyPoint* OtherPoint)
{
  bxx Result = MPE_PolyContainsPoint(Triangle, Point) && MPE_PolyContainsPoint(Triangle, OtherPoint);
  return Result;
}

internal_static
void MPE_PolySwapNeighbors(MPEPolyTriangle* Triangle, MPEPolyTriangle* OldNeighbor, MPEPolyTriangle* NewNeighbor)
{
  for(u32 NeighborIndex = 0;; ++NeighborIndex)
  {
    MPE_Assert(NeighborIndex < 3);
    if (Triangle->Neighbors[NeighborIndex] == OldNeighbor)
    {
      Triangle->Neighbors[NeighborIndex] = NewNeighbor;
      break;
    }
  }
}

internal_static
void MPE_PolyMarkNeighborTri(MPEPolyTriangle* Triangle, MPEPolyTriangle* Neighbor)
{
  for (u32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
  {
    u32 NextEdgeIndex = MPEPolyEdgeLUT[EdgeIndex+1];
    for (u32 NeighborIndex = 0; NeighborIndex < 3; ++NeighborIndex)
    {
      u32 NextNeighborIndex = MPEPolyEdgeLUT[NeighborIndex+1];
      if ((Triangle->Points[EdgeIndex] == Neighbor->Points[NeighborIndex] &&
          Triangle->Points[NextEdgeIndex] == Neighbor->Points[NextNeighborIndex]) ||
         (Triangle->Points[EdgeIndex] == Neighbor->Points[NextNeighborIndex] &&
               Triangle->Points[NextEdgeIndex] == Neighbor->Points[NeighborIndex]))
      {
        Triangle->Neighbors[MPEPolyEdgeLUT[NextEdgeIndex+1]] = Neighbor;
        Neighbor->Neighbors[MPEPolyEdgeLUT[NextNeighborIndex+1]] = Triangle;
        return;
      }
    }
  }
}

internal_static MPE_INLINE
void MPE_ClearDelunayEdges(MPEPolyTriangle* Triangle)
{
  Triangle->Flags &= ~(uxx)MPEPolyTriFlag_DelaunayEdgeMask;
}

internal_static
MPEPolyPoint* MPE_PointCW(MPEPolyTriangle* Triangle, MPEPolyPoint* Point, i32* PointIndex)
{
  MPEPolyPoint* Result;
  if (Point == Triangle->Points[0])
  {
    Result = Triangle->Points[2];
    *PointIndex = 2;
  }
  else if (Point == Triangle->Points[1])
  {
    Result = Triangle->Points[0];
    *PointIndex = 0;
  }
  else if (Point == Triangle->Points[2])
  {
    Result = Triangle->Points[1];
    *PointIndex = 1;
  }
  else
  {
    MPE_InvalidCodePath;
    Result = 0;
    *PointIndex = -1;
  }
  return Result;
}

internal_static
MPEPolyTriangle* MPE_NeighborCCW(MPEPolyTriangle* Triangle, MPEPolyPoint* Point)
{
  MPEPolyTriangle* Result;
  if (Point == Triangle->Points[0])
  {
    Result = Triangle->Neighbors[2];
  }
  else if (Point == Triangle->Points[1])
  {
    Result = Triangle->Neighbors[0];
  }
  else
  {
    MPE_Assert(Point == Triangle->Points[2]);
    Result = Triangle->Neighbors[1];
  }
  return Result;
}

#define FLAG(Tri, Name) (Tri->Flags & MPEPolyTriFlag_##Name) == MPEPolyTriFlag_## Name

internal_static
void MPE_SetAdjacentEdges(MPEPolyTriangle* Triangle, MPEPolyPoint* Point, MPEPolyEdges Edges, bxx Clockwise)
{
  uxx Mask;
  uxx NewFlags;
  if (Point == Triangle->Points[0])
  {
    if (Clockwise)
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge1 | MPEPolyTriFlag_DelaunayEdge1;
      NewFlags = (Edges.ConstrainedCW & MPEPolyTriFlag_ConstrainedEdge1) |
                 (Edges.DelaunayCW & MPEPolyTriFlag_DelaunayEdge1);
    }
    else
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge2 | MPEPolyTriFlag_DelaunayEdge2;
      NewFlags = (Edges.ConstrainedCCW & MPEPolyTriFlag_ConstrainedEdge2) |
                 (Edges.DelaunayCCW & MPEPolyTriFlag_DelaunayEdge2);
    }
  }
  else if (Point == Triangle->Points[1])
  {
    if (Clockwise)
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge2 | MPEPolyTriFlag_DelaunayEdge2;
      NewFlags = (Edges.ConstrainedCW & MPEPolyTriFlag_ConstrainedEdge2) |
                 (Edges.DelaunayCW & MPEPolyTriFlag_DelaunayEdge2);
    }
    else
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge0 | MPEPolyTriFlag_DelaunayEdge0;
      NewFlags = (Edges.ConstrainedCCW & MPEPolyTriFlag_ConstrainedEdge0) |
                 (Edges.DelaunayCCW & MPEPolyTriFlag_DelaunayEdge0);
    }
  }
  else
  {
    if (Clockwise)
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge0 | MPEPolyTriFlag_DelaunayEdge0;
      NewFlags = (Edges.ConstrainedCW & MPEPolyTriFlag_ConstrainedEdge0) |
                 (Edges.DelaunayCW & MPEPolyTriFlag_DelaunayEdge0);
    }
    else
    {
      Mask = MPEPolyTriFlag_ConstrainedEdge1 | MPEPolyTriFlag_DelaunayEdge1;
      NewFlags = (Edges.ConstrainedCCW & MPEPolyTriFlag_ConstrainedEdge1) |
                 (Edges.DelaunayCCW & MPEPolyTriFlag_DelaunayEdge1);
    }
  }
  Triangle->Flags = (Triangle->Flags & ~Mask) | NewFlags;
}


internal_static
uxx MPE_GetConstrainedEdgeCW(MPEPolyTriangle* Triangle, MPEPolyPoint* Point)
{
  uxx Result;
  if (Point == Triangle->Points[0])
  {
    Result = Triangle->Flags & MPEPolyTriFlag_ConstrainedEdge1;
  }
  else if (Point == Triangle->Points[1])
  {
    Result = Triangle->Flags & MPEPolyTriFlag_ConstrainedEdge2;
  }
  else
  {
    MPE_Assert(Point == Triangle->Points[2]);
    Result = Triangle->Flags & MPEPolyTriFlag_ConstrainedEdge0;
  }
  return Result;
}

internal_static MPE_INLINE
MPEPolyTriangle* MPE_PolyNeighborAcross(MPEPolyTriangle* Triangle, MPEPolyPoint* OppositePoint)
{
  MPEPolyTriangle* Result;
  if (OppositePoint == Triangle->Points[0])
  {
    Result = Triangle->Neighbors[0];
  }
  else if (OppositePoint == Triangle->Points[1])
  {
    Result = Triangle->Neighbors[1];
  }
  else
  {
    MPE_Assert(OppositePoint == Triangle->Points[2]);
    Result = Triangle->Neighbors[2];
  }
  return Result;
}


internal_static MPE_INLINE
void MPE_PolyLegalizePoint(MPEPolyTriangle* Triangle, MPEPolyPoint* Point)
{
  Triangle->Points[1] = Triangle->Points[0];
  Triangle->Points[0] = Triangle->Points[2];
  Triangle->Points[2] = Point;
}

internal_static
i32 MPE_PolyPointIndex(MPEPolyTriangle* Triangle, MPEPolyPoint* Point)
{
  i32 Result;
  if (Point == Triangle->Points[0])
  {
    Result = 0;
  }
  else if (Point == Triangle->Points[1])
  {
    Result = 1;
  }
  else if (Point == Triangle->Points[2])
  {
    Result = 2;
  }
  else
  {
    Result = -1;
    MPE_InvalidCodePath;
  }
  return Result;
}

internal_static
i32 MPE_EdgeIndex(MPEPolyTriangle* Triangle, MPEPolyPoint* P1, MPEPolyPoint* P2)
{
  i32 Result = -1;
  if (Triangle->Points[0] == P1)
  {
    if (Triangle->Points[1] == P2)
    {
      Result = 2;
    }
    else if (Triangle->Points[2] == P2)
    {
      Result = 1;
    }
  }
  else if (Triangle->Points[1] == P1)
  {
    if (Triangle->Points[2] == P2)
    {
      Result = 0;
    }
    else if (Triangle->Points[0] == P2)
    {
      Result = 2;
    }
  }
  else if (Triangle->Points[2] == P1)
  {
    if (Triangle->Points[0] == P2)
    {
      Result = 1;
    }
    else if (Triangle->Points[1] == P2)
    {
      Result = 0;
    }
  }
  return Result;
}


internal_static MPE_INLINE
void MPE_PolyMarkConstrainedEdgeIndex(MPEPolyTriangle* Triangle, i32 Index)
{
  MPE_Assert(Index >= 0);
  Triangle->Flags |= (uxx)MPEPolyTriFlag_ConstrainedEdge0 << (u32)Index;
}

internal_static MPE_INLINE
void MPE_PolyMarkDelaunayEdge(MPEPolyTriangle* Triangle, i32 Index)
{
  MPE_Assert(Index >= 0);
  Triangle->Flags |= (uxx)MPEPolyTriFlag_DelaunayEdge0 << (u32)Index;
}

internal_static
void MPE_PolyMarkConstrainedEdgePoints(MPEPolyTriangle* Triangle, MPEPolyPoint* Point, MPEPolyPoint* OtherPoint)
{
  if ((OtherPoint == Triangle->Points[0] && Point == Triangle->Points[1]) ||
      (OtherPoint == Triangle->Points[1] && Point == Triangle->Points[0]))
  {
    MPE_PolyMarkConstrainedEdgeIndex(Triangle, 2);
  }
  else if ((OtherPoint == Triangle->Points[0] && Point == Triangle->Points[2]) ||
           (OtherPoint == Triangle->Points[2] && Point == Triangle->Points[0]))
  {
    MPE_PolyMarkConstrainedEdgeIndex(Triangle, 1);
  }
  else if ((OtherPoint == Triangle->Points[1] && Point == Triangle->Points[2]) ||
           (OtherPoint == Triangle->Points[2] && Point == Triangle->Points[1]))
  {
    MPE_PolyMarkConstrainedEdgeIndex(Triangle, 0);
  }
}

internal_static MPE_INLINE
void MPE_PolyMarkConstrainedEdge(MPEPolyTriangle* Triangle, MPEPolyEdge* Edge)
{
  MPE_PolyMarkConstrainedEdgePoints(Triangle, Edge->P, Edge->Q);
}

internal_static MPE_INLINE
MPEPolyNode* MPE_PolyNode(MPEPolyContext* PolyContext, MPEPolyPoint* Point, MPEPolyTriangle* Triangle)
{
  MPEPolyNode* Result = PolyContext->Nodes + PolyContext->NodeCount++;
  Result->Point = Point;
  Result->Triangle = Triangle;
  Result->Value = Point->X;
  return Result;
}

internal_static
MPEPolyPoint* MPE_PolyPushPoint(MPEPolyContext* PolyContext)
{
  MPE_Assert(PolyContext->MaxPointCount > PolyContext->PointPoolCount);
  MPEPolyPoint* Result = PolyContext->PointsPool + PolyContext->PointPoolCount++;
  return Result;
}

internal_static
MPEPolyPoint* MPE_PolyPushPointArray(MPEPolyContext* PolyContext, u32 Count)
{
  MPE_Assert(PolyContext->MaxPointCount > (PolyContext->PointPoolCount+Count));
  MPEPolyPoint* Result = PolyContext->PointsPool + PolyContext->PointPoolCount;
  PolyContext->PointPoolCount += Count;
  return Result;
}


internal_static MPE_INLINE
MPEPolyTriangle* MPE_PushTriangle(MPEPolyContext* PolyContext, MPEPolyPoint* A, MPEPolyPoint* B, MPEPolyPoint* C)
{
  MPEPolyTriangle* Result = PolyContext->TrianglePool + PolyContext->TrianglePoolCount++;
  Result->Points[0] = A;
  Result->Points[1] = B;
  Result->Points[2] = C;
  return Result;
}

internal_static MPE_INLINE
MPEPolyNode* MPE_LocatePoint(MPEPolyContext* PolyContext, MPEPolyPoint* Point)
{
  poly_float PX = Point->X;
  MPEPolyNode* Node = PolyContext->SearchNode;
  poly_float NX = Node->Point->X;

  if (PX < NX)
  {
    while ((Node = Node->Prev))
    {
      if (Point == Node->Point)
      {
        break;
      }
    }
  }
  else if ((PX - NX) < MPE_POLY2TRI_EPSILON)
  {
    if (Point != Node->Point)
    {
      // We might have two nodes with same X Value for a short time
      if (Point == Node->Prev->Point)
      {
        Node = Node->Prev;
      }
      else
      {
        MPE_Assert(Point == Node->Next->Point);
        Node = Node->Next;
      }
    }
  }
  else
  {
    while ((Node = Node->Next))
    {
      if (Point == Node->Point)
      {
        break;
      }
    }
  }
  if (Node)
  {
    PolyContext->SearchNode = Node;
  }
  return Node;
}

internal_static
bxx MPE_IsEdgeSideOfTriangle(MPEPolyTriangle* Triangle, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ)
{
  i32 EdgeIndex = MPE_EdgeIndex(Triangle, EdgeP, EdgeQ);
  bxx Result;
  if (EdgeIndex != -1)
  {
    MPE_PolyMarkConstrainedEdgeIndex(Triangle, EdgeIndex);
    MPEPolyTriangle* NeighborTriangle = Triangle->Neighbors[EdgeIndex];
    if (NeighborTriangle)
    {
      MPE_PolyMarkConstrainedEdgePoints(NeighborTriangle, EdgeP, EdgeQ);
    }
    Result = MPE_TRUE;
  }
  else
  {
    Result = MPE_FALSE;
  }
  return Result;
}

internal_static
bxx MPE_AngleExceeds90Degrees(MPEPolyPoint* Origin, MPEPolyPoint* PointA, MPEPolyPoint* PointB)
{
  poly_float AX = PointA->X - Origin->X;
  poly_float AY = PointA->Y - Origin->Y;
  poly_float BX = PointB->X - Origin->X;
  poly_float BY = PointB->Y - Origin->Y;
  poly_float DotProduct = AX * BX + AY * BY;
  bxx Result = DotProduct < 0;
  return Result;
}

internal_static
bxx MPE_AngleExceedsPlus90DegreesOrIsNegative(MPEPolyPoint* Origin, MPEPolyPoint* PointA, MPEPolyPoint* PointB)
{
  poly_float AX = PointA->X - Origin->X;
  poly_float AY = PointA->Y - Origin->Y;
  poly_float BX = PointB->X - Origin->X;
  poly_float BY = PointB->Y - Origin->Y;
  poly_float DotProduct = AX * BX + AY * BY;
  poly_float Direction = AX * BY - AY * BX;
  bxx Result = DotProduct < 0 || Direction < 0;
  return Result;
}


internal_static
bxx MPE_LargeHole_DontFill(MPEPolyNode* Node)
{
  bxx Result;
  MPEPolyNode* NextNode = Node->Next;
  MPEPolyNode* PrevNode = Node->Prev;
  if (!MPE_AngleExceeds90Degrees(Node->Point, NextNode->Point, PrevNode->Point))
  {
    Result = MPE_FALSE;
  }
  else
  {
    // Check additional points on front.
    MPEPolyNode* Next2Node = NextNode->Next;
    // "..Plus.." because only want angles on same side as MPEPolyPoint being added.
    if (Next2Node && !MPE_AngleExceedsPlus90DegreesOrIsNegative(Node->Point, Next2Node->Point, PrevNode->Point))
    {
      Result = MPE_FALSE;
    }
    else
    {
      MPEPolyNode* Prev2Node = PrevNode->Prev;
      // "..Plus.." because only want angles on same side as MPEPolyPoint being added.
      if (Prev2Node && !MPE_AngleExceedsPlus90DegreesOrIsNegative(Node->Point, NextNode->Point, Prev2Node->Point))
      {
        Result = MPE_FALSE;
      }
      else
      {
        Result = MPE_TRUE;
      }
    }
  }
  return Result;
}

internal_static MPE_INLINE
bxx MPE_PolyShouldFillBasin(MPEPolyNode* Node)
{
  poly_float AX = Node->Point->X;
  poly_float AY = Node->Point->Y;
  poly_float BX = Node->Next->Next->Point->X;
  poly_float BY = Node->Next->Next->Point->Y;
  poly_float DotProduct = AX * BX + AY * BY;
  poly_float Direction = AX * BY - AY * BX;
  bxx Result = DotProduct < 0 || Direction > 0;
  return Result;
}

/**
  * Requirement:
  * 1. a,b and c form a triangle.
  * 2. a and d is know to be on opposite side of bc
  *                a
  *                +
  *               / \
  *              /   \
  *            b/     \c
  *            +-------+
  *           /    d    \
  *          /           \
  * Fact: d has to be in area B to have a chance to be inside the circle formed by
  *  a,b and c
  *  d is outside B if MPE_PolyOrient2D(a,b,d) or MPE_PolyOrient2D(c,a,d) is CW
  *  This preknowledge gives us a way to optimize the incircle test
  */

internal_static
bxx MPE_PolyInCircle(MPEPolyPoint* PointA, MPEPolyPoint* PointB, MPEPolyPoint* pc, MPEPolyPoint* pd)
{
  bxx Result;
  poly_float adx = PointA->X - pd->X;
  poly_float ady = PointA->Y - pd->Y;
  poly_float bdx = PointB->X - pd->X;
  poly_float bdy = PointB->Y - pd->Y;

  poly_float adxbdy = adx * bdy;
  poly_float bdxady = bdx * ady;
  poly_float oabd = adxbdy - bdxady;

  if (oabd <= 0)
  {
    Result = MPE_FALSE;
  }
  else
  {
    poly_float cdx = pc->X - pd->X;
    poly_float cdy = pc->Y - pd->Y;

    poly_float cdxady = cdx * ady;
    poly_float adxcdy = adx * cdy;
    poly_float ocad = cdxady - adxcdy;

    if (ocad <= 0)
    {
      Result = MPE_FALSE;
    }
    else
    {
      poly_float bdxcdy = bdx * cdy;
      poly_float cdxbdy = cdx * bdy;

      poly_float alift = adx * adx + ady * ady;
      poly_float blift = bdx * bdx + bdy * bdy;
      poly_float clift = cdx * cdx + cdy * cdy;

      poly_float det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;
      Result = det > 0;
    }
  }
  return Result;
}

/**
  * Rotates a triangle pair one vertex CW
  *       n2                    n2
  *  P +-----+             P +-----+
  *    | t  /|               |\  t |
  *    |   / |               | \   |
  *  n1|  /  |n3           n1|  \  |n3
  *    | /   |    after CW   |   \ |
  *    |/ oT |               | oT \|
  *    +-----+ oP            +-----+
  *       n4                    n4
  */
internal_static
void MPE_PolyRotateTrianglePair(MPEPolyTriangle* Triangle, MPEPolyPoint* Point, i32 PointIndex, MPEPolyTriangle* OtherTriangle,
                                MPEPolyPoint* OtherPoint, i32 OtherPointIndex)
{
  MPE_Assert(PointIndex >= 0 && OtherPointIndex >= 0);
  uxx Flags = Triangle->Flags;
  uxx OtherFlags = OtherTriangle->Flags;
  MPEPolyEdges AdjacentEdges;
  MPEPolyEdges OtherAdjacentEdges;

  u32 RotateAmount = MPEPolyEdgeLUT[PointIndex+1];
  u32 OtherRotateAmount = MPEPolyEdgeLUT[OtherPointIndex+1];

  MPEPolyTriangle* Neighbor1 = Triangle->Neighbors[MPEPolyEdgeLUT[1+RotateAmount]];
  MPEPolyTriangle* Neighbor2 = Triangle->Neighbors[RotateAmount];
  MPEPolyTriangle* Neighbor3 = OtherTriangle->Neighbors[MPEPolyEdgeLUT[1+OtherRotateAmount]];
  MPEPolyTriangle* Neighbor4 = OtherTriangle->Neighbors[OtherRotateAmount];

  Triangle->Neighbors[RotateAmount] = Neighbor3;
  Triangle->Neighbors[MPEPolyEdgeLUT[RotateAmount+1]] = Neighbor2;
  Triangle->Neighbors[MPEPolyEdgeLUT[RotateAmount+2]] = OtherTriangle;
  OtherTriangle->Neighbors[OtherRotateAmount] = Neighbor1;
  OtherTriangle->Neighbors[MPEPolyEdgeLUT[OtherRotateAmount+1]] = Neighbor4;
  OtherTriangle->Neighbors[MPEPolyEdgeLUT[OtherRotateAmount+2]] = Triangle;

  if (Neighbor1)
  {
    MPE_PolySwapNeighbors(Neighbor1, Triangle, OtherTriangle);
  }
  if (Neighbor3)
  {
    MPE_PolySwapNeighbors(Neighbor3, OtherTriangle, Triangle);
  }

  Triangle->Points[RotateAmount] = Triangle->Points[MPEPolyEdgeLUT[2+RotateAmount]];
  Triangle->Points[MPEPolyEdgeLUT[2+RotateAmount]] = Triangle->Points[MPEPolyEdgeLUT[1+RotateAmount]];
  Triangle->Points[MPEPolyEdgeLUT[1+RotateAmount]] = OtherPoint;
  OtherTriangle->Points[OtherRotateAmount] = OtherTriangle->Points[MPEPolyEdgeLUT[2+OtherRotateAmount]];
  OtherTriangle->Points[MPEPolyEdgeLUT[2+OtherRotateAmount]] = OtherTriangle->Points[MPEPolyEdgeLUT[1+OtherRotateAmount]];
  OtherTriangle->Points[MPEPolyEdgeLUT[1+OtherRotateAmount]] = Point;

  AdjacentEdges.ConstrainedCW = Flags & ((uxx)MPEPolyTriFlag_ConstrainedEdge0 << ((RotateAmount))) ? 0xFF : 0;
  AdjacentEdges.ConstrainedCCW = Flags & ((uxx)MPEPolyTriFlag_ConstrainedEdge0 << (MPEPolyEdgeLUT[RotateAmount+1])) ? 0xFF : 0;
  AdjacentEdges.DelaunayCW = Flags & ((uxx)MPEPolyTriFlag_DelaunayEdge0 << ((RotateAmount))) ? 0xFF : 0;
  AdjacentEdges.DelaunayCCW = Flags & ((uxx)MPEPolyTriFlag_DelaunayEdge0 << (MPEPolyEdgeLUT[RotateAmount+1])) ? 0xFF : 0;

  OtherAdjacentEdges.ConstrainedCW = OtherFlags & ((uxx)MPEPolyTriFlag_ConstrainedEdge0 << (MPEPolyEdgeLUT[OtherRotateAmount])) ? 0xFF : 0;
  OtherAdjacentEdges.ConstrainedCCW = OtherFlags & ((uxx)MPEPolyTriFlag_ConstrainedEdge0 << (MPEPolyEdgeLUT[OtherRotateAmount+1])) ? 0xFF : 0;
  OtherAdjacentEdges.DelaunayCW = OtherFlags & ((uxx)MPEPolyTriFlag_DelaunayEdge0 << (MPEPolyEdgeLUT[OtherRotateAmount])) ? 0xFF : 0;
  OtherAdjacentEdges.DelaunayCCW = OtherFlags & ((uxx)MPEPolyTriFlag_DelaunayEdge0 << (MPEPolyEdgeLUT[OtherRotateAmount+1])) ? 0xFF : 0;

  MPE_SetAdjacentEdges(Triangle, Point, AdjacentEdges, MPE_TRUE);
  MPE_SetAdjacentEdges(Triangle, OtherPoint, OtherAdjacentEdges, MPE_FALSE);
  MPE_SetAdjacentEdges(OtherTriangle, Point, AdjacentEdges, MPE_FALSE);
  MPE_SetAdjacentEdges(OtherTriangle, OtherPoint, OtherAdjacentEdges, MPE_TRUE);
}

internal_static
bxx MPE_PolyIsShallow(MPEPolyContext* PolyContext, MPEPolyNode* Node)
{
  poly_float Height;
  if (PolyContext->Basin.LeftHighest)
  {
    Height = PolyContext->Basin.LeftNode->Point->Y - Node->Point->Y;
  }
  else
  {
    Height = PolyContext->Basin.RightNode->Point->Y - Node->Point->Y;
  }

  bxx Result = PolyContext->Basin.Width > Height;
  return Result;
}

internal_static
bxx MPE_PolyLegalize(MPEPolyContext* PolyContext, MPEPolyTriangle* Triangle);

internal_static
void MPE_PolyMapTriangleToNodes(MPEPolyContext* PolyContext, MPEPolyTriangle* Triangle);

internal_static
void MPE_PolyFill(MPEPolyContext* PolyContext, MPEPolyNode* Node)
{
  MPEPolyTriangle* Triangle = MPE_PushTriangle(PolyContext, Node->Prev->Point, Node->Point, Node->Next->Point);

  // TODO: should copy the ConstrainedEdge Value from neighbor Triangles
  //       for now ConstrainedEdge values are copied during the legalize
  // static int count;
  // printf("POLY %i\n", count++);
  MPE_PolyMarkNeighborTri(Triangle, Node->Prev->Triangle);
  MPE_PolyMarkNeighborTri(Triangle, Node->Triangle);

  // Update the advancing front
  Node->Prev->Next = Node->Next;
  Node->Next->Prev = Node->Prev;

  // If it was legalized the Triangle has already been mapped
  if (!MPE_PolyLegalize(PolyContext, Triangle))
  {
    MPE_PolyMapTriangleToNodes(PolyContext, Triangle);
  }
}


internal_static
void MPE_FillBasinReq(MPEPolyContext* PolyContext, MPEPolyNode* Node)
{
  // if shallow stop filling
  if (MPE_PolyIsShallow(PolyContext, Node))
  {
    return;
  }

  MPE_PolyFill(PolyContext, Node);

  if (Node->Prev == PolyContext->Basin.LeftNode && Node->Next == PolyContext->Basin.RightNode)
  {
    return;
  }
  else if (Node->Prev == PolyContext->Basin.LeftNode)
  {
    uxx Orientation = MPE_PolyOrient2D(Node->Point, Node->Next->Point, Node->Next->Next->Point);
    if (Orientation == MPEPolyOrientation_CW)
    {
      return;
    }
    Node = Node->Next;
  }
  else if (Node->Next == PolyContext->Basin.RightNode)
  {
    uxx Orientation = MPE_PolyOrient2D(Node->Point, Node->Prev->Point, Node->Prev->Prev->Point);
    if (Orientation == MPEPolyOrientation_CCW)
    {
      return;
    }
    Node = Node->Prev;
  }
  else
  {
    // Continue with the neighbor Node with lowest Y Value
    if (Node->Prev->Point->Y < Node->Next->Point->Y)
    {
      Node = Node->Prev;
    }
    else
    {
      Node = Node->Next;
    }
  }

  MPE_FillBasinReq(PolyContext, Node);
}

internal_static
void MPE_FillBasin(MPEPolyContext* PolyContext, MPEPolyNode* Node)
{
  if (MPE_PolyOrient2D(Node->Point, Node->Next->Point, Node->Next->Next->Point) == MPEPolyOrientation_CCW)
  {
    PolyContext->Basin.LeftNode = Node->Next->Next;
  }
  else
  {
    PolyContext->Basin.LeftNode = Node->Next;
  }

  // Find the bottom and Right Node
  PolyContext->Basin.BottomNode = PolyContext->Basin.LeftNode;
  while (PolyContext->Basin.BottomNode->Next &&
         PolyContext->Basin.BottomNode->Point->Y >=
             PolyContext->Basin.BottomNode->Next->Point->Y)
  {
    PolyContext->Basin.BottomNode = PolyContext->Basin.BottomNode->Next;
  }
  if (PolyContext->Basin.BottomNode != PolyContext->Basin.LeftNode)
  {
    PolyContext->Basin.RightNode = PolyContext->Basin.BottomNode;
    while (PolyContext->Basin.RightNode->Next &&
           PolyContext->Basin.RightNode->Point->Y < PolyContext->Basin.RightNode->Next->Point->Y)
    {
      PolyContext->Basin.RightNode = PolyContext->Basin.RightNode->Next;
    }
    if (PolyContext->Basin.RightNode != PolyContext->Basin.BottomNode)
    {
      PolyContext->Basin.Width =
        PolyContext->Basin.RightNode->Point->X - PolyContext->Basin.LeftNode->Point->X;
      PolyContext->Basin.LeftHighest =
        PolyContext->Basin.LeftNode->Point->Y > PolyContext->Basin.RightNode->Point->Y;

      MPE_FillBasinReq(PolyContext, PolyContext->Basin.BottomNode);
    }
  }
}

internal_static
void MPE_FillRightConcaveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node);

internal_static
void MPE_FillRightConvexEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node);

internal_static
void MPE_FillLeftBelowEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node);

internal_static
void MPE_FillLeftConcaveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node);

internal_static
void MPE_FillLeftConvexEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node);

internal_static
void MPE_FillRightBelowEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  if (Node->Point->X < Edge->P->X)
  {
    if (MPE_PolyOrient2D(Node->Point, Node->Next->Point, Node->Next->Next->Point) == MPEPolyOrientation_CCW)
    {
      // Concave
      MPE_FillRightConcaveEdgeEvent(PolyContext, Edge, Node);
    }
    else
    {
      // Convex
      MPE_FillRightConvexEdgeEvent(PolyContext, Edge, Node);
      // Retry this one
      MPE_FillRightBelowEdgeEvent(PolyContext, Edge, Node);
    }
  }
}

internal_static
void MPE_FillRightAboveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  while (Node->Next->Point->X < Edge->P->X)
  {
    // Check if Next Node is below the Edge
    if (MPE_PolyOrient2D(Edge->Q, Node->Next->Point, Edge->P) == MPEPolyOrientation_CCW)
    {
      MPE_FillRightBelowEdgeEvent(PolyContext, Edge, Node);
    }
    else
    {
      Node = Node->Next;
    }
  }
}

internal_static
void MPE_FillRightConcaveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  MPE_PolyFill(PolyContext, Node->Next);
  if (Node->Next->Point != Edge->P)
  {
    // Next above or below Edge?
    if (MPE_PolyOrient2D(Edge->Q, Node->Next->Point, Edge->P) == MPEPolyOrientation_CCW)
    {
      // Below
      if (MPE_PolyOrient2D(Node->Point, Node->Next->Point, Node->Next->Next->Point) == MPEPolyOrientation_CCW)
      {
        // Next is concave
        MPE_FillRightConcaveEdgeEvent(PolyContext, Edge, Node);
      }
      else
      {
        // Next is convex
      }
    }
  }
}

internal_static
void MPE_FillRightConvexEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  // Next concave or convex?
  if (MPE_PolyOrient2D(Node->Next->Point, Node->Next->Next->Point,
               Node->Next->Next->Next->Point) == MPEPolyOrientation_CCW)
  {
    // Concave
    MPE_FillRightConcaveEdgeEvent(PolyContext, Edge, Node->Next);
  }
  else
  {
    // Convex
    // Next above or below Edge?
    if (MPE_PolyOrient2D(Edge->Q, Node->Next->Next->Point, Edge->P) == MPEPolyOrientation_CCW)
    {
      // Below
      MPE_FillRightConvexEdgeEvent(PolyContext, Edge, Node->Next);
    }
    else
    {
      // Above
    }
  }
}

internal_static
void MPE_FillLeftAboveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  while (Node->Prev->Point->X > Edge->P->X)
  {
    // Check if Next Node is below the Edge
    if (MPE_PolyOrient2D(Edge->Q, Node->Prev->Point, Edge->P) == MPEPolyOrientation_CW)
    {
      MPE_FillLeftBelowEdgeEvent(PolyContext, Edge, Node);
    }
    else
    {
      Node = Node->Prev;
    }
  }
}

internal_static
void MPE_FillEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  if (PolyContext->EdgeEvent.Right)
  {
    MPE_FillRightAboveEdgeEvent(PolyContext, Edge, Node);
  }
  else
  {
    MPE_FillLeftAboveEdgeEvent(PolyContext, Edge, Node);
  }
}

internal_static
void MPE_FillLeftBelowEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  if (Node->Point->X > Edge->P->X)
  {
    if (MPE_PolyOrient2D(Node->Point, Node->Prev->Point, Node->Prev->Prev->Point) == MPEPolyOrientation_CW)
    {
      // Concave
      MPE_FillLeftConcaveEdgeEvent(PolyContext, Edge, Node);
    }
    else
    {
      // Convex
      MPE_FillLeftConvexEdgeEvent(PolyContext, Edge, Node);
      // Retry this one
      MPE_FillLeftBelowEdgeEvent(PolyContext, Edge, Node);
    }
  }
}

internal_static
void MPE_FillLeftConvexEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  // Next concave or convex?
  if (MPE_PolyOrient2D(Node->Prev->Point, Node->Prev->Prev->Point,
                   Node->Prev->Prev->Prev->Point) == MPEPolyOrientation_CW)
  {
    // Concave
    MPE_FillLeftConcaveEdgeEvent(PolyContext, Edge, Node->Prev);
  }
  else
  {
    // Convex
    // Next above or below Edge?
    if (MPE_PolyOrient2D(Edge->Q, Node->Prev->Prev->Point, Edge->P) == MPEPolyOrientation_CW)
    {
      // Below
      MPE_FillLeftConvexEdgeEvent(PolyContext, Edge, Node->Prev);
    }
    else
    {
      // Above
    }
  }
}

internal_static
void MPE_FillLeftConcaveEdgeEvent(MPEPolyContext* PolyContext, MPEPolyEdge* Edge, MPEPolyNode* Node)
{
  MPE_PolyFill(PolyContext, Node->Prev);
  if (Node->Prev->Point != Edge->P)
  {
    // Next above or below Edge?
    if (MPE_PolyOrient2D(Edge->Q, Node->Prev->Point, Edge->P) == MPEPolyOrientation_CW)
    {
      // Below
      if (MPE_PolyOrient2D(Node->Point, Node->Prev->Point, Node->Prev->Prev->Point) == MPEPolyOrientation_CW)
      {
        // Next is concave
        MPE_FillLeftConcaveEdgeEvent(PolyContext, Edge, Node);
      }
      else
      {
        // Next is convex
      }
    }
  }
}

internal_static
MPEPolyTriangle* MPE_NextFlipTriangle(MPEPolyContext* PolyContext, uxx Orientation, MPEPolyTriangle* Triangle,
                                  MPEPolyTriangle* OtherTriangle, MPEPolyPoint* Point, i32 PointIndex,
                                  i32 OtherPointIndex, i32* NewPointIndex)
{
  MPE_Assert(PointIndex >= 0 && OtherPointIndex >= 0);
  MPEPolyTriangle* Result;
  if (Orientation == MPEPolyOrientation_CCW)
  {
    // OtherTriangle is not crossing Edge after flip
    // i32 EdgeIndex = MPE_EdgeIndex(OtherTriangle, Point, OtherPoint);
    i32 EdgeIndex = OtherPointIndex;
    // MPE_Assert(EdgeIndex == OtherPointIndex);
    MPE_Assert(EdgeIndex != -1);
    MPE_PolyMarkDelaunayEdge(OtherTriangle, EdgeIndex);
    MPE_PolyLegalize(PolyContext, OtherTriangle);
    MPE_ClearDelunayEdges(OtherTriangle);
    Result = Triangle;
  }
  else
  {
    // Triangle is not crossing Edge after flip
    // i32 EdgeIndex = PointIndex - OtherPointIndex;
    // if (EdgeIndex < 0)
    // {
    //   EdgeIndex = -EdgeIndex;
    // }
    // i32 EdgeIndex = MPE_EdgeIndex(Triangle, Point, OtherPoint);
    i32 EdgeIndex = PointIndex;
    MPE_Assert(EdgeIndex == PointIndex);
    MPE_Assert(EdgeIndex != -1);
    MPE_PolyMarkDelaunayEdge(Triangle, EdgeIndex);
    MPE_PolyLegalize(PolyContext, Triangle);
    MPE_ClearDelunayEdges(Triangle);
    Result = OtherTriangle;
  }
  *NewPointIndex = MPE_PolyPointIndex(Result, Point);

  // MPE_Assert(*NewPointIndex >= 0);
  MPE_Assert(Result->Points[*NewPointIndex] == Point);
  return Result;
}

internal_static MPE_INLINE
i32 MPE_NextFlipPoint(MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, MPEPolyPoint* OtherPoint, i32 OtherPointIndex)
{
  MPE_Assert(OtherPoint != EdgeP);
  uxx Orientation = MPE_PolyOrient2D(EdgeQ, OtherPoint, EdgeP);
  i32 Result;
  if (Orientation == MPEPolyOrientation_CW)
  {
    Result = (i32)MPEPolyEdgeLUT[OtherPointIndex+1];
  }
  else
  {
    MPE_Assert(Orientation == MPEPolyOrientation_CCW);
    Result = (i32)MPEPolyEdgeLUT[OtherPointIndex+2];
  }
  return Result;
}


internal_static
void MPE_FlipScanEdgeEvent(MPEPolyContext* PolyContext, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, i32 EdgeQIndex,
                              MPEPolyTriangle* FlipTriangle, MPEPolyTriangle* Triangle, i32 PointIndex);

internal_static
void MPE_EdgeEventPoints(MPEPolyContext* PolyContext, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, i32 EdgeQIndex,
                         MPEPolyTriangle* Triangle, MPEPolyPoint* Point, i32 PointIndex);

internal_static
void MPE_FlipEdgeEvent(MPEPolyContext* PolyContext, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, i32 EdgeQIndex, MPEPolyTriangle* Triangle,
                       MPEPolyPoint* Point, i32 PointIndex)
{
  MPE_Assert(PointIndex >= 0 && EdgeQIndex >= 0);
  MPE_Assert(Triangle->Points[PointIndex] == Point);
  MPEPolyTriangle* OtherTriangle = MPE_PolyNeighborAcross(Triangle, Point);
  MPE_Assert(OtherTriangle);
  i32 OtherPointIndex;
  MPEPolyPoint* CWPoint = Triangle->Points[MPEPolyEdgeLUT[PointIndex+2]];
  MPEPolyPoint* CCWPoint = Triangle->Points[MPEPolyEdgeLUT[PointIndex+1]];
  MPEPolyPoint* OtherPoint = MPE_PointCW(OtherTriangle, CWPoint, &OtherPointIndex);

  if (MPE_PolyInScanArea(Point, CCWPoint, CWPoint, OtherPoint))
  {
    // Lets rotate shared Edge one vertex CW
    MPE_PolyRotateTrianglePair(Triangle, Point, PointIndex, OtherTriangle, OtherPoint, OtherPointIndex);
    MPE_PolyMapTriangleToNodes(PolyContext, Triangle);
    MPE_PolyMapTriangleToNodes(PolyContext, OtherTriangle);

    if (Point == EdgeQ && OtherPoint == EdgeP)
    {
      if (EdgeQ == PolyContext->EdgeEvent.ConstrainedEdge->Q &&
          EdgeP == PolyContext->EdgeEvent.ConstrainedEdge->P)
      {
        MPE_PolyMarkConstrainedEdgePoints(Triangle, EdgeP, EdgeQ);
        MPE_PolyMarkConstrainedEdgePoints(OtherTriangle, EdgeP, EdgeQ);
        MPE_PolyLegalize(PolyContext, Triangle);
        MPE_PolyLegalize(PolyContext, OtherTriangle);
      }
      else
      {
        // XXX: I think one of the Triangles should be legalized here?
      }
    }
    else
    {
      MPE_Assert(OtherPoint != EdgeP && OtherPoint != EdgeQ && OtherPoint != Point);

      uxx Orientation = MPE_PolyOrient2D(EdgeQ, OtherPoint, EdgeP);
      i32 NewPointIndex;
      Triangle = MPE_NextFlipTriangle(PolyContext, Orientation, Triangle, OtherTriangle, Point, PointIndex, OtherPointIndex, &NewPointIndex);
      MPE_FlipEdgeEvent(PolyContext, EdgeP, EdgeQ, EdgeQIndex, Triangle, Point, NewPointIndex);
    }
  }
  else
  {
    // MPE_BREAK;
    MPE_Assert(OtherPoint != EdgeP);
    i32 NewPointIndex = MPE_NextFlipPoint(EdgeP, EdgeQ, OtherPoint, OtherPointIndex);
    EdgeQIndex = MPE_PolyPointIndex(Triangle, EdgeQ);
    MPE_FlipScanEdgeEvent(PolyContext, EdgeP, EdgeQ, EdgeQIndex, Triangle, OtherTriangle, NewPointIndex);
    PointIndex = MPE_PolyPointIndex(Triangle, Point);
    MPE_EdgeEventPoints(PolyContext, EdgeP, EdgeQ, EdgeQIndex, Triangle, Point, PointIndex);
  }
}

internal_static
void MPE_FlipScanEdgeEvent(MPEPolyContext* PolyContext, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, i32 EdgeQIndex,
                              MPEPolyTriangle* FlipTriangle, MPEPolyTriangle* Triangle, i32 PointIndex)
{
  MPE_Assert(PointIndex >= 0 && EdgeQIndex >= 0);
  MPE_Assert(EdgeQ == FlipTriangle->Points[EdgeQIndex]);
  MPEPolyTriangle* OtherTriangle = Triangle->Neighbors[PointIndex];
  i32 OtherPointIndex;
  MPEPolyPoint* CWPoint = Triangle->Points[MPEPolyEdgeLUT[PointIndex+2]];
  // MPEPolyPoint* CCWPoint = Triangle->Points[MPEPolyEdgeLUT[PointIndex+1]];
  MPEPolyPoint* FlipCWPoint = FlipTriangle->Points[MPEPolyEdgeLUT[EdgeQIndex+2]];
  MPEPolyPoint* FlipCCWPoint = FlipTriangle->Points[MPEPolyEdgeLUT[EdgeQIndex+1]];
  MPEPolyPoint* OtherPoint = MPE_PointCW(OtherTriangle, CWPoint, &OtherPointIndex);

  if (MPE_PolyInScanArea(EdgeQ, FlipCCWPoint, FlipCWPoint, OtherPoint))
  {
    // flip with new edge OtherPoint->EdgeQ
    MPE_FlipEdgeEvent(PolyContext, EdgeQ, OtherPoint, PointIndex, OtherTriangle, OtherPoint, OtherPointIndex);
    // TODO: Actually I just figured out that it should be possible to
    //       improve this by getting the Next OtherTriangle and op before the the above
    //       flip and continue the flipScanEdgeEvent here
    // set new OtherTriangle and op here and uxx back to inScanArea test
    // also need to set a new FlipTriangle first
    // Turns out at first glance that this is somewhat complicated
    // so it will have to wait.
  }
  else
  {
    i32 NewPointIndex = MPE_NextFlipPoint(EdgeP, EdgeQ, OtherPoint, OtherPointIndex);
    MPE_FlipScanEdgeEvent(PolyContext, EdgeP, EdgeQ, EdgeQIndex, FlipTriangle, OtherTriangle, NewPointIndex);
  }
}

internal_static
void MPE_PolyMapTriangleToNodes(MPEPolyContext* PolyContext, MPEPolyTriangle* Triangle)
{
  for (uxx TriangleIndex = 0; TriangleIndex < 3; ++TriangleIndex)
  {
    if (!Triangle->Neighbors[TriangleIndex])
    {
      MPEPolyPoint* NextPoint = Triangle->Points[MPEPolyEdgeLUT[TriangleIndex+2]];
      MPEPolyNode* Node = MPE_LocatePoint(PolyContext, NextPoint);
      if (Node)
      {
        Node->Triangle = Triangle;
      }
    }
  }
}

internal_static
void MPE_PolyFinalizeTriangles(MPEPolyContext* PolyContext, MPEPolyTriangle* StartTriangle)
{
  PolyContext->Triangles = MPE_PolyPush(PolyContext->Allocator, MPEPolyTriangle*);

  u32 TriangleCount = 0;
  PolyContext->TempTriangles[TriangleCount++] = StartTriangle;
  while (TriangleCount)
  {
    MPEPolyTriangle* TempTriangle = PolyContext->TempTriangles[--TriangleCount];
    if (!(TempTriangle->Flags & MPEPolyTriFlag_IsInterior))
    {
      TempTriangle->Flags |= MPEPolyTriFlag_IsInterior;
      PolyContext->Triangles[PolyContext->TriangleCount++] = TempTriangle;

      for (uxx NeighborIndex = 0; NeighborIndex < 3; ++NeighborIndex)
      {
        uxx BitMask = (uxx)MPEPolyTriFlag_ConstrainedEdge0 << NeighborIndex;
        if (!(TempTriangle->Flags & BitMask))
        {
          if (TempTriangle->Neighbors[NeighborIndex])
          {
            PolyContext->TempTriangles[TriangleCount++] = TempTriangle->Neighbors[NeighborIndex];
          }
        }
      }
    }
  }
}

internal_static
bxx MPE_PolyLegalize(MPEPolyContext* PolyContext, MPEPolyTriangle* Triangle)
{
  bxx Result = MPE_FALSE;
  // To legalize a Triangle we start by finding if any of the three edges
  // violate the Delaunay condition
  for (u32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
  {
    if (Triangle->Flags & ((uxx)MPEPolyTriFlag_DelaunayEdge0 << EdgeIndex))
    {
      continue;
    }

    MPEPolyTriangle* OtherTriangle = Triangle->Neighbors[EdgeIndex];

    if (OtherTriangle)
    {
      MPEPolyPoint* Point = Triangle->Points[EdgeIndex];
      i32 OtherPointIndex;
      MPEPolyPoint* CWPoint = Triangle->Points[MPEPolyEdgeLUT[EdgeIndex+2]];
      MPEPolyPoint* OtherPoint = MPE_PointCW(OtherTriangle, CWPoint, &OtherPointIndex);
      // If this is a Constrained Edge or a Delaunay Edge(only during recursive
      // legalization)
      // then we should not try to legalize
      uxx CombinedMask =  (((uxx)MPEPolyTriFlag_ConstrainedEdge0 << OtherPointIndex) |
                           ((uxx)MPEPolyTriFlag_DelaunayEdge0 << OtherPointIndex));
      if (OtherTriangle->Flags & CombinedMask)
      {
        uxx Mask = (uxx)MPEPolyTriFlag_ConstrainedEdge0 << EdgeIndex;
        uxx NewFlag = (OtherTriangle->Flags & ((uxx)MPEPolyTriFlag_ConstrainedEdge0 << OtherPointIndex)) ? Mask : 0;
        Triangle->Flags = (Triangle->Flags & ~Mask) | NewFlag;
        continue;
      }

      MPEPolyPoint* CCWPoint = Triangle->Points[MPEPolyEdgeLUT[EdgeIndex+1]];
      bxx IsInside = MPE_PolyInCircle(Point, CCWPoint, CWPoint, OtherPoint);
      if (IsInside)
      {
        // Lets mark this shared edge as Delaunay
        Triangle->Flags |= (uxx)MPEPolyTriFlag_DelaunayEdge0 << EdgeIndex;
        OtherTriangle->Flags |= (uxx)MPEPolyTriFlag_DelaunayEdge0 << OtherPointIndex;

        // Lets rotate shared edge one vertex CW to legalize it
        MPE_PolyRotateTrianglePair(Triangle, Point, (i32)EdgeIndex, OtherTriangle, OtherPoint, OtherPointIndex);

        // We now got one valid Delaunay Edge shared by two Triangles
        // This gives us 4 new edges to check for Delaunay

        // Make sure that Triangle to Node mapping is done only one time for a
        // specific Triangle
        bxx NotLegalized = !MPE_PolyLegalize(PolyContext, Triangle);
        if (NotLegalized)
        {
          MPE_PolyMapTriangleToNodes(PolyContext, Triangle);
        }

        NotLegalized = !MPE_PolyLegalize(PolyContext, OtherTriangle);
        if (NotLegalized)
        {
          MPE_PolyMapTriangleToNodes(PolyContext, OtherTriangle);
        }

        // Reset the Delaunay edges, since they only are valid Delaunay edges
        // until we add a new Triangle or MPEPolyPoint.
        // XXX: need to think about this. Can these edges be tried after we
        //      return to previous recursive level?
        Triangle->Flags &= ~(uxx)MPEPolyTriFlag_DelaunayEdge0 << EdgeIndex;
        OtherTriangle->Flags &= ~(uxx)MPEPolyTriFlag_DelaunayEdge0 << OtherPointIndex;

        // If Triangle have been legalized no need to check the other edges
        // since
        // the recursive legalization will handles those so we can end here.
        Result = MPE_TRUE;
        break;
      }
    }
  }
  return Result;
}

internal_static
void MPE_EdgeEventPoints(MPEPolyContext* PolyContext, MPEPolyPoint* EdgeP, MPEPolyPoint* EdgeQ, i32 EdgeQIndex,
                         MPEPolyTriangle* Triangle, MPEPolyPoint* Point, i32 PointIndex)
{
  if (MPE_IsEdgeSideOfTriangle(Triangle, EdgeP, EdgeQ))
  {
    return;
  }
  MPE_Assert(PointIndex >= 0 && EdgeQIndex >= 0);
  MPE_Assert(Triangle->Points[PointIndex] == Point);
  i32 Point1Index = (i32)MPEPolyEdgeLUT[PointIndex+1];
  MPEPolyPoint* Point1 = Triangle->Points[Point1Index];
  uxx Orientation1 = MPE_PolyOrient2D(EdgeQ, Point1, EdgeP);
  if (Orientation1 == MPEPolyOrientation_Collinear)
  {
    if (MPE_PolyContainsPoints(Triangle, EdgeQ, Point1))
    {
      MPE_PolyMarkConstrainedEdgePoints(Triangle, EdgeQ, Point1);
      // We are modifying the maybe it would be better to
      // not change the given and just keep a variable for the new
      //
      PolyContext->EdgeEvent.ConstrainedEdge->Q = Point1;
      Triangle = MPE_PolyNeighborAcross(Triangle, Point);
      MPE_EdgeEventPoints(PolyContext, EdgeP, Point1, Point1Index, Triangle, Point1, Point1Index);
    }
    else
    {
      MPE_InvalidCodePath;
    }
    return;
  }

  i32 Point2Index = (i32)MPEPolyEdgeLUT[PointIndex+2];
  MPEPolyPoint* Point2 = Triangle->Points[Point2Index];
  uxx Orientation2 = MPE_PolyOrient2D(EdgeQ, Point2, EdgeP);
  if (Orientation2 == MPEPolyOrientation_Collinear)
  {
    if (MPE_PolyContainsPoints(Triangle, EdgeQ, Point2))
    {
      MPE_PolyMarkConstrainedEdgePoints(Triangle, EdgeQ, Point2);
      // We are modifying the maybe it would be better to
      // not change the given and just keep a variable for the new
      //
      PolyContext->EdgeEvent.ConstrainedEdge->Q = Point2;
      Triangle = MPE_PolyNeighborAcross(Triangle, Point);
      MPE_EdgeEventPoints(PolyContext, EdgeP, Point2, Point2Index, Triangle, Point2, Point2Index);
    }
    else
    {
      MPE_InvalidCodePath;
    }
    return;
  }

  if (Orientation1 == Orientation2)
  {
    // Need to decide if we are rotating CW or CCW to get to a Triangle
    // that will cross edge
    MPEPolyTriangle* NewTriangle;
    i32 NewPointIndex = (i32)MPEPolyEdgeLUT[PointIndex+(i32)Orientation1+1];
    NewTriangle = Triangle->Neighbors[NewPointIndex];
    MPE_Assert(NewTriangle);
    NewPointIndex = MPE_PolyPointIndex(NewTriangle, Point);
    MPE_Assert(NewPointIndex == MPE_PolyPointIndex(NewTriangle, Point));
    MPE_EdgeEventPoints(PolyContext, EdgeP, EdgeQ, EdgeQIndex, NewTriangle, Point, NewPointIndex);
  }
  else
  {
    // This Triangle crosses so lets flippin start!
    MPE_FlipEdgeEvent(PolyContext, EdgeP, EdgeQ, EdgeQIndex, Triangle, Point, PointIndex);
  }
}

internal_static
void MPE_PolyAddEdge(MPEPolyContext* PolyContext)
{
  MPEPolyPoint* Points = PolyContext->PointsPool + PolyContext->PointCount;
  u32 PointCount = PolyContext->PointPoolCount - PolyContext->PointCount;
  MPE_Assert(PointCount && PointCount < PolyContext->MaxPointCount);
  for (uxx PointIndex = 0; PointIndex < PointCount; ++PointIndex)
  {
    uxx NextPointIndex = PointIndex < PointCount - 1 ? PointIndex + 1 : 0;
    MPEPolyPoint* A = Points + PointIndex;
    MPEPolyPoint* B = Points + NextPointIndex;
    PolyContext->Points[PolyContext->PointCount++] = A;
    MPEPolyEdge* Edge = MPE_PolyPush(PolyContext->Allocator, MPEPolyEdge);

    if ((A->Y > B->Y) ||
        ((B->Y - A->Y) < MPE_POLY2TRI_EPSILON && (A->X > B->X)))
    {
      Edge->Q = A;
      Edge->P = B;
    }
    else
    {
      Edge->P = A;
      Edge->Q = B;
    }

    MPE_Assert(fabsf(A->Y - B->Y) > MPE_POLY2TRI_EPSILON ||
               fabsf(A->X - B->X) > MPE_POLY2TRI_EPSILON);

    if (Edge->Q->FirstEdge)
    {
      MPEPolyEdge* LastEdge = Edge->Q->FirstEdge;
      while(LastEdge && LastEdge->Next)
      {
        LastEdge = LastEdge->Next;
      }
      LastEdge->Next = Edge;
    }
    else
    {
      Edge->Q->FirstEdge = Edge;
    }
  }
}


internal_static MPE_INLINE
void MPE_PolyAddHole(MPEPolyContext* PolyContext)
{
  MPE_PolyAddEdge(PolyContext);
}

internal_static MPE_INLINE
void MPE_PolyAddPoints(MPEPolyContext* PolyContext, MPEPolyPoint* Points, u32 PointCount)
{
  for (uxx PointIndex = 0; PointIndex < PointCount; ++PointIndex)
  {
    MPEPolyPoint* Point = Points + PointIndex;
    PolyContext->Points[PolyContext->PointCount++] = Point;
  }
}


//SECTION: Sort

#define MPE_POLY_COMPARE(A, B) \
  (((A)->Y < (B)->Y) || (((A)->Y - (B)->Y) < MPE_POLY2TRI_EPSILON && ((A)->X < (B)->X)))

#ifdef MPE_POLY2TRI_USE_CUSTOM_SORT
internal_static
void MPE_PolySort(MPEPolyPoint** Points, MPEPolyPoint** Temp, u32 Count)
{
  MPE_Assert(Count);
  if (Count <= 1)
  {

  }
  else if (Count == 2)
  {
    MPEPolyPoint* A = Points[0];
    MPEPolyPoint* B = Points[1];
    if ((A->Y > B->Y) ||
        ((B->Y - A->Y) < MPE_POLY2TRI_EPSILON && A->X >= B->X))
    {
      MPEPolyPoint* Swap = A;
      Points[0] = B;
      Points[1] = Swap;
    }
  }
  else
  {
    u32 Half0 = Count / 2;
    u32 Half1 = Count - Half0;

    MPEPolyPoint** InHalf0 = Points;
    MPEPolyPoint** InHalf1 = Points + Half0;
    MPEPolyPoint** End = Points + Count;

    MPE_PolySort(InHalf0, Temp, Half0);
    MPE_PolySort(InHalf1, Temp, Half1);

    MPEPolyPoint** ReadHalf0 = InHalf0;
    MPEPolyPoint** ReadHalf1 = InHalf1;

    //NOTE: Don't merge if two halves are already sorted
    if (InHalf1[-1]->Y > InHalf1[0]->Y)
    {
      MPEPolyPoint** Out = Temp;
      for (u32 WriteIndex = 0; WriteIndex < Count; ++WriteIndex)
      {
        if(ReadHalf0 == InHalf1)
        {
          *Out++ = *ReadHalf1++;
        }
        else if (ReadHalf1 == End)
        {
          *Out++ = *ReadHalf0++;
        }
        else
        {
          MPEPolyPoint* A = ReadHalf0[0];
          MPEPolyPoint* B = ReadHalf1[0];
          if (MPE_POLY_COMPARE(A, B))
          {
            *Out++ = *ReadHalf0++;
          }
          else
          {
            *Out++ = *ReadHalf1++;
          }
        }
      }
      MPE_MemoryCopy(Points, Temp, sizeof(*Points) * Count);
    }
  }
}
#else
internal_static
i32 MPE_PolySortCompare(MPEPolyPoint** A, MPEPolyPoint** B)
{
  i32 Result;
  if ((*A)->Y < (*B)->Y)
  {
    Result = -1;
  }
  else if (((*A)->Y - (*B)->Y) < MPE_POLY2TRI_EPSILON)
  {
    if ((*A)->X < (*B)->X)
    {
      Result = -1;
    }
    else
    {
      Result = 1;
    }
  }
  else
  {
    Result = 1;
  }
  return Result;
}
#endif

internal_static
void MPE_PolyTriangulate(MPEPolyContext* PolyContext)
{
  MPE_Assert(PolyContext->PointCount);
  u32 PointCount = PolyContext->PointCount;
  //NOTE: Init triangulation
  {
    poly_float Xmax = PolyContext->Points[0]->X;
    poly_float Xmin = PolyContext->Points[0]->X;
    poly_float Ymax = PolyContext->Points[0]->Y;
    poly_float Ymin = PolyContext->Points[0]->Y;
    //TODO: Calculate min and max during sort
    for (uxx PointIndex = 0; PointIndex < PolyContext->PointCount; ++PointIndex)
    {
      MPEPolyPoint* Point = PolyContext->Points[PointIndex];
      if (Point->X > Xmax)
      {
        Xmax = Point->X;
      }
      if (Point->X < Xmin)
      {
        Xmin = Point->X;
      }
      if (Point->Y > Ymax)
      {
        Ymax = Point->Y;
      }
      if (Point->Y < Ymin)
      {
        Ymin = Point->Y;
      }
    }
#define MPE_INITIAL_TRIANGLE_FACTOR  (poly_float)0.3f
    poly_float Dx = MPE_INITIAL_TRIANGLE_FACTOR * (Xmax - Xmin);
    poly_float Dy = MPE_INITIAL_TRIANGLE_FACTOR * (Ymax - Ymin);
    MPEPolyPoint* Head = MPE_PolyPush(PolyContext->Allocator, MPEPolyPoint);
    Head->X = Xmax + Dx;
    Head->Y = Ymin - Dy;
    MPEPolyPoint* Tail = MPE_PolyPush(PolyContext->Allocator, MPEPolyPoint);
    Tail->X = Xmin - Dx;
    Tail->Y = Ymin - Dy;
    PolyContext->HeadPoint = Head;
    PolyContext->TailPoint = Tail;
  }

  //NOTE: Sort points
  {
#ifdef MPE_POLY2TRI_USE_CUSTOM_SORT
    umm BeforeRollback = PolyContext->Allocator.Used;
    MPEPolyPoint** Temp = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyPoint*, PointCount);
    MPE_PolySort(PolyContext->Points, Temp, PointCount);
    MPE_MemorySet(PolyContext->Allocator.Memory+BeforeRollback, 0, PolyContext->Allocator.Used - BeforeRollback);
    PolyContext->Allocator.Used = BeforeRollback;
#else
    qsort(PolyContext->Points, PointCount, sizeof(MPEPolyPoint*), (int(*)(const void*, const void*))MPE_PolySortCompare);
#endif
  }

  //NOTE: Create advancing front
  {
    MPEPolyTriangle* Triangle = MPE_PushTriangle(PolyContext, PolyContext->Points[0], PolyContext->TailPoint, PolyContext->HeadPoint);

    MPEPolyNode* Head = MPE_PolyNode(PolyContext, Triangle->Points[1], Triangle);
    MPEPolyNode* Middle = MPE_PolyNode(PolyContext, Triangle->Points[0], Triangle);
    MPEPolyNode* Tail = MPE_PolyNode(PolyContext, Triangle->Points[2], 0);

    PolyContext->HeadNode = PolyContext->SearchNode = Head;
    PolyContext->TailNode = Tail;
    Head->Next = Middle;
    Middle->Next = Tail;
    Middle->Prev = Head;
    Tail->Prev = Middle;
  }

  //NOTE: Sweep points
  {
    for (uxx PointIndex = 1; PointIndex < PolyContext->PointCount; ++PointIndex)
    {
      MPEPolyPoint* Point = PolyContext->Points[PointIndex];
      MPEPolyNode* Node;
      MPEPolyNode* NewNode;
      {
        Node = PolyContext->SearchNode;
        if (Point->X < Node->Value)
        {
          while ((Node = Node->Prev))
          {
            if (Point->X >= Node->Value)
            {
              PolyContext->SearchNode = Node;
              break;
            }
          }
        }
        else
        {
          while ((Node = Node->Next))
          {
            if (Point->X < Node->Value)
            {
              PolyContext->SearchNode = Node->Prev;
              Node = Node->Prev;
              break;
            }
          }
        }
      }

      //NOTE: New front triangle
      {
        MPEPolyTriangle* NewTriangle = MPE_PushTriangle(PolyContext, Point, Node->Point, Node->Next->Point);
        MPE_PolyMarkNeighborTri(NewTriangle, Node->Triangle);

        NewNode = MPE_PolyNode(PolyContext, Point, 0);
        NewNode->Next = Node->Next;
        NewNode->Prev = Node;
        Node->Next->Prev = NewNode;
        Node->Next = NewNode;

        if (!MPE_PolyLegalize(PolyContext, NewTriangle))
        {
          MPE_PolyMapTriangleToNodes(PolyContext, NewTriangle);
        }
        MPE_Assert(NewNode->Triangle == NewTriangle);

        // Only need to check +epsilon since MPEPolyPoint never have smaller
        // X Value than Node due to how we fetch nodes from the front
        if (Point->X <= Node->Point->X + MPE_POLY2TRI_EPSILON)
        {
          MPE_PolyFill(PolyContext, Node);
        }

        //NOTE: FillAdvancingFront
        {
          // Fill Right holes
          MPEPolyNode* NextNode = NewNode->Next;

          while (NextNode->Next)
          {
            // if HoleAngle exceeds 90 degrees then break.
            if (MPE_LargeHole_DontFill(NextNode))
            {
              break;
            }
            MPE_PolyFill(PolyContext, NextNode);
            NextNode = NextNode->Next;
          }


          // Fill left holes
          NextNode = NewNode->Prev;

          while (NextNode->Prev)
          {
            // if HoleAngle exceeds 90 degrees then break.
            if (MPE_LargeHole_DontFill(NextNode))
            {
              break;
            }
            MPE_PolyFill(PolyContext, NextNode);
            NextNode = NextNode->Prev;
          }

          // Fill Right basins
          if (NewNode->Next && NewNode->Next->Next)
          {
            if (MPE_PolyShouldFillBasin(NewNode))
            {
              MPE_FillBasin(PolyContext, NewNode);
            }
          }
        }
      }
      for (MPEPolyEdge* Edge = Point->FirstEdge; Edge; Edge = Edge->Next)
      {
        //NOTE: Edge event
        PolyContext->EdgeEvent.ConstrainedEdge = Edge;
        PolyContext->EdgeEvent.Right = (Edge->P->X > Edge->Q->X);

        if (!MPE_IsEdgeSideOfTriangle(NewNode->Triangle, Edge->P, Edge->Q))
        {
          // For now we will do all needed filling
          // TODO: integrate with flip process might give some better performance
          //       but for now this avoid the issue with cases that needs both flips and
          //       fills
          MPE_FillEdgeEvent(PolyContext, Edge, NewNode);
          i32 EdgeQIndex = MPE_PolyPointIndex(NewNode->Triangle, Edge->Q);
          MPE_EdgeEventPoints(PolyContext, Edge->P, Edge->Q, EdgeQIndex, NewNode->Triangle, Edge->Q, EdgeQIndex);
        }
      }
    }
  }

  //NOTE: Finalize polygon
  {
    MPEPolyTriangle* Triangle = PolyContext->HeadNode->Next->Triangle;
    MPEPolyPoint* Point = PolyContext->HeadNode->Next->Point;
    while (!MPE_GetConstrainedEdgeCW(Triangle, Point))
    {
      Triangle = MPE_NeighborCCW(Triangle, Point);
    }
    MPE_PolyFinalizeTriangles(PolyContext, Triangle);
  }
}

internal_static
b32 MPE_PolyInitContext(MPEPolyContext* PolyContext, void* Memory, u32 MaxPointCount)
{
  if (Memory && MaxPointCount)
  {
    umm MemorySize = MPE_PolyMemoryRequired(MaxPointCount);
    MaxPointCount = MaxPointCount + MPE_POLY2TRI_POINT_COUNT_EPSILON;
    u32 MaxTrianglePoolCount = MaxPointCount*2;

    MPE_MemorySet(PolyContext, 0, sizeof(*PolyContext));
    // Align to 8 bytes
    umm AlignmentOffset = 0;
    u8* MemoryBytes = (u8*)Memory;
    umm MemoryAddress = (umm)(MemoryBytes);
    if(MemoryAddress & 0x7)
    {
      AlignmentOffset = (0x8) - (MemoryAddress & 0x7);
    }
    MPE_Assert(AlignmentOffset == 0);
    PolyContext->Memory = Memory;
    PolyContext->Allocator.Size = MemorySize-AlignmentOffset;
    PolyContext->Allocator.Memory = (u8*)Memory + AlignmentOffset;

    PolyContext->MaxPointCount = MaxPointCount;
    PolyContext->PointsPool = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyPoint, MaxPointCount);
    PolyContext->Points = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyPoint*, MaxPointCount);
    PolyContext->TrianglePool = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyTriangle, MaxTrianglePoolCount);
    PolyContext->TempTriangles = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyTriangle*, MaxTrianglePoolCount);
    PolyContext->Nodes = MPE_PolyPushArray(PolyContext->Allocator, MPEPolyNode, MaxPointCount);
    PolyContext->Valid = MPE_TRUE;
  }
  else
  {
    PolyContext->Valid = MPE_FALSE;
  }

  return PolyContext->Valid;
}

internal_static
umm MPE_PolyMemoryRequired(u32 MaxPointCount)
{
  MaxPointCount += MPE_POLY2TRI_POINT_COUNT_EPSILON;
  umm EstimatedSizePerPoint = sizeof(MPEPolyEdge) +
                              sizeof(MPEPolyPoint) +
                              sizeof(MPEPolyPoint*) +
                              sizeof(MPEPolyPoint*) +
                              sizeof(MPEPolyNode) +
                              sizeof(MPEPolyTriangle) * 2 +
                              sizeof(MPEPolyTriangle*) * 2 +
                              sizeof(MPEPolyTriangle*) * 2;
  umm Result = EstimatedSizePerPoint * MaxPointCount + 0x8;
  return Result;
}

#endif // MPE_POLY2TRI_IMPLEMENTATION
