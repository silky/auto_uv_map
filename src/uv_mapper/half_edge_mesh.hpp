#pragma once

#include <list>
#include <vector>
#include "vec.hpp"

//
// This is a half-edge mesh implementation based on STL iterators.
//

class Edge;
class HalfEdge;
class Vertex;
class Face;

typedef std::list<HalfEdge>::iterator HalfEdgeIter;
typedef std::list<Edge>::iterator EdgeIter;
typedef std::list<Vertex>::iterator VertexIter;
typedef std::list<Face>::iterator FaceIter;

class Edge {
public:
    // one of the two half edges this edge is split into.
    HalfEdgeIter halfEdge;

    float GetLength();
};

class HalfEdge {
public:
    // the vertex at the root of this half edge.
    VertexIter vertex;

    // the half edge opposite to this half edge.
    // this half will be in an opposite face.
    // and this will be null if this half edge is part of the boundary.
    HalfEdgeIter twin;

    // the half edge that this half edge is pointing at, in the current face.
    HalfEdgeIter next;

    // the face this half edge belongs to.
    FaceIter face;

    // edge that contains this half edge.
    EdgeIter edge;

    float GetLength();
};

class Vertex {
public:
    // one of the half edges that is emanating from this vertex.
    HalfEdgeIter halfEdge;

    vec3 p;
    int id; // for convenience, we associate an id with every vertex.

    Vertex(const vec3& p) {
        this->p = p;
    }
};

class Face {
public:
    // The face that contains this half edge.
    HalfEdgeIter halfEdge;
};


inline bool operator<( const HalfEdgeIter& i, const HalfEdgeIter& j ) { return &*i < &*j; }
inline bool operator<( const   VertexIter& i, const   VertexIter& j ) { return &*i < &*j; }
inline bool operator<( const     FaceIter& i, const     FaceIter& j ) { return &*i < &*j; }


class HalfEdgeMesh {
public:
    HalfEdgeMesh(
        const std::vector<vec3>& vertices,
        const std::vector<Tri>& faces);

    // Convert a half edge mesh back to a polygon-soup mesh.
    void ToMesh(
        std::vector<vec3>& vertices,
        std::vector<Tri>& faces);

    bool IsBoundary(HalfEdgeIter heit);
    bool IsBoundary(EdgeIter eit);
    HalfEdgeIter GetNextBoundary(HalfEdgeIter heit);

    FaceIter BeginFaces() {return faces.begin();}
    FaceIter EndFaces() {return faces.end();}

    VertexIter BeginVertices() {return vertices.begin();}
    VertexIter EndVertices() {return vertices.end();}

    HalfEdgeIter BeginHalfEdges() {return halfEdges.begin();}
    HalfEdgeIter EndHalfEdges() {return halfEdges.end();}

    EdgeIter BeginEdges() {return edges.begin();}
    EdgeIter EndEdges() {return edges.end();}

    size_t NumVertices() { return vertices.size();}
    size_t NumEdges() { return edges.size();}
    size_t NumHalfEdges() { return halfEdges.size();}

private:

    std::list<HalfEdge> halfEdges;
    std::list<Edge> edges;
    std::list<Vertex> vertices;
    std::list<Face> faces;
};
