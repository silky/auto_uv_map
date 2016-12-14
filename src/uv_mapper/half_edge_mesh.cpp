#include "half_edge_mesh.hpp"

#include <map>
#include <set>
#include <vector>

#include <stdlib.h>

using std::vector;
using std::map;
using std::set;
using std::pair;

typedef pair<int, int> EdgeId;

EdgeId Sort(int i0, int i1) {
    return i0 < i1 ? EdgeId(i0, i1) : EdgeId(i1, i0);
}

HalfEdgeMesh::HalfEdgeMesh(
    const vector<vec3>& vertices,
    const vector<Tri>& faces) {

    map<pair<int, int>, HalfEdgeIter> addedHalfEdges;
    map<pair<int, int>, EdgeIter> addedEdges;
    map<int, VertexIter> addedVertices;

    // we basically construct the half edge mesh by iterating over all edges,
    // and constructing their corresponding half edge. Along the way, we link
    // these half edges to their corresponding faces, vertices and edges.
    for(Tri tri : faces) {
        FaceIter face = this->faces.insert(this->faces.end(), Face());

        // three half edges for every triangle.
        for(int iTri = 0; iTri < 3; iTri++) {
            // half edge indices.
            int i0 = tri.i[(iTri+0)%3];
            int i1 = tri.i[(iTri+1)%3];

            pair<int, int> heid(i0, i1); // id

            if(addedHalfEdges.count(heid) > 0) {
                printf("ERROR: Invalid mesh: duplicated half edge with indices (%d,%d)\n", i0, i1);
                exit(1);
            }
            HalfEdgeIter halfEdge = halfEdges.insert(halfEdges.end(), HalfEdge());
            halfEdge->twin = EndHalfEdges(); // twin is null by default.
            addedHalfEdges[heid] = halfEdge;

            //
            // link half edge with edge.
            //
            EdgeId eid = Sort(i0 ,i1);
            EdgeIter edge;
            if(addedEdges.count(eid) == 0) { // create edge on the fly.
                edge = edges.insert(edges.end(), Edge());
                edge->halfEdge = halfEdge;
                addedEdges[eid] = edge;
            }
            halfEdge->edge = edge;

            //
            // link half edge with face.
            //
            halfEdge->face = face;
            face->halfEdge = halfEdge;

            //
            // link half edge with vertex.
            //
            VertexIter vertex;
            if(addedVertices.count(i0) == 0) { // create vertex on fly if needed.
                vec3 p = vertices[i0];
                vertex = this->vertices.insert(this->vertices.end(), Vertex(p));
                addedVertices[i0] = vertex;
            } else {
                vertex = addedVertices[i0];
            }
            vertex->halfEdge = halfEdge;
            halfEdge->vertex = vertex;

            //
            // finally, attempt linking half edge with its twin.
            //
            pair<int, int> twinHeid(i1, i0); // id
            if(addedHalfEdges.count(twinHeid) > 0) {
                HalfEdgeIter twin = addedHalfEdges[twinHeid];
                twin->twin = halfEdge;
                halfEdge->twin = twin;
            }
        }

        //
        // Finally, link the half edges with their next half edge.
        //
        for(int iTri = 0; iTri < 3; iTri++) {

            // half edge indices.
            int i0 = tri.i[(iTri+0)%3];
            int i1 = tri.i[(iTri+1)%3];
            int i2 = tri.i[(iTri+2)%3];

            HalfEdgeIter halfEdge = addedHalfEdges[pair<int, int>(i0, i1)];
            HalfEdgeIter nextHalfEdge = addedHalfEdges[pair<int, int>(i1, i2)];

            halfEdge->next = nextHalfEdge;
        }
    }

    // assign id to every vertex.
    int id = 0;
    for(VertexIter it = BeginVertices(); it != EndVertices(); ++it) {
        it->id = id++;
    }
}

void HalfEdgeMesh::ToMesh(
    std::vector<vec3>& vertices,
    std::vector<Tri>& faces) {
    map< VertexIter, int > verticesMap;

    for(VertexIter it = BeginVertices(); it != EndVertices(); ++it) {
	vertices.push_back(it->p);
	verticesMap[it] = it->id;
    }

    for(FaceIter it = BeginFaces(); it != EndFaces(); ++it) {
	HalfEdgeIter halfEdge = it->halfEdge;

	Tri tri;
	int i = 0;

	do {
	    tri.i[i++] = verticesMap[halfEdge->vertex];
	    halfEdge = halfEdge->next;
	} while(halfEdge != it->halfEdge);

	faces.push_back(tri);
    }
}

bool HalfEdgeMesh::IsBoundary(HalfEdgeIter heit) {
    return heit->twin == EndHalfEdges();
}

bool HalfEdgeMesh::IsBoundary(EdgeIter eit) {
    return IsBoundary(eit->halfEdge);
}

HalfEdgeIter HalfEdgeMesh::GetNextBoundary(HalfEdgeIter heit) {
    // find vertex that heit points to.
    VertexIter to = heit->next->vertex;

    HalfEdgeIter halfEdge = to->halfEdge;
    HalfEdgeIter first = halfEdge;

    // search for next boundary. It will be an edge emanating from 'to'.
    do {
        if(IsBoundary(halfEdge)) {
            return halfEdge;
        }

        halfEdge = halfEdge->twin->next;

    } while(halfEdge != first);

    printf("ERROR: invalid mesh: found no next boundary edge\n");
    exit(1);
}

float Edge::GetLength() {
    return vec3::distance(halfEdge->vertex->p, halfEdge->twin->vertex->p);
}

float HalfEdge::GetLength() {
    return vec3::distance(vertex->p, twin->vertex->p);
}
