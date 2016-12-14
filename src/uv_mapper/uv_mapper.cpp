#include "uv_mapper.hpp"

#include "half_edge_mesh.hpp"
#include "vec.hpp"

#include "Eigen/Sparse"

#include <set>
#include <iostream>

#define M_PI 3.14159

using std::vector;
using std::set;

/*

           /\
         / u \
     b1/      \a1
     /     c   \
   / -----------\
   \            /
    \         /
   a2\      /b2
      \ v /
       \/

Compute the harmonic weight for the edge.

It is computed by the formula

(cot(u) + cot(v)) / 2

see the beautiful ASCII art above for an illustration.
*/

float HarmonicWeight(EdgeIter eit) {
    // length of edge shared by the two triangles.
    float c = eit->GetLength();

    HalfEdgeIter e = eit->halfEdge;
    float a1 = vec3::distance(e->next->vertex->p, e->next->next->vertex->p );
    float b1 = vec3::distance(e->next->next->vertex->p, e->next->next->next->vertex->p );

    e = eit->halfEdge->twin;
    float a2 = vec3::distance(e->next->vertex->p, e->next->next->vertex->p );
    float b2 = vec3::distance(e->next->next->vertex->p, e->next->next->next->vertex->p );

    // use the cosine law to compute cos(u) and cos(v)
    float cos_u = (b1*b1 + a1*a1 - c*c) / (2.0f * b1 * a1);
    float cos_v = (b2*b2 + a2*a2 - c*c) / (2.0f * b2 * a2);

    float sin_u = sqrt(1.0 - cos_u*cos_u);
    float sin_v = sqrt(1.0 - cos_v*cos_v);

    float weight;
    weight = (cos_u / sin_u + cos_v  / sin_v) * 0.5f;

    return weight;
}

void uvMap(
    const std::vector<float>& inVertices,
    const std::vector<int>& inFaces,

    std::vector<float>& outVertices,
    std::vector<int>& outFaces,
    std::vector<float>& outUvs,
    std::vector<float>* outUvEdges
    ) {

    vector<vec3> vVertices;
    for(int i = 0; i < inVertices.size(); i+=3) {
        vVertices.push_back(vec3(
                                inVertices[i+0],
                                inVertices[i+1],
                                inVertices[i+2]
                                ));
    }

    vector<Tri> vFaces;
    for(int i = 0; i < inFaces.size(); i+=3) {
        vFaces.push_back(Tri(
                             inFaces[i+0],
                             inFaces[i+1],
                             inFaces[i+2]
                             ));
    }

    // instead of a polygon soup, we use a half edge mesh.
    HalfEdgeMesh hem(vVertices, vFaces);

    // To now find the uv coordinates, we will create a system of
    // linear equations. The system is formulated with matrices and vectors,
    // and then we solve it with Eigen.

    // N is number of variables in the linear system. We want uv coordinates for every vertex
    // so one variable for every vertex.
    const int N = hem.NumVertices();

    //
    // Let us first find the boundary. So let us find the first boundary edge.
    //
    HalfEdgeIter firstBoundary = hem.EndHalfEdges();
    HalfEdgeIter currentBoundary = hem.EndHalfEdges();
    for(HalfEdgeIter heit = hem.BeginHalfEdges(); heit != hem.EndHalfEdges(); heit++) {
        if(hem.IsBoundary(heit)) {
            firstBoundary = heit;
            break;
        }
    }

    if(firstBoundary == hem.EndHalfEdges()) {
        printf("ERROR: found no boundary in mesh\n");
    }

    //
    // find the rest of the boundary by iterating over the boundary.
    // also, keep track of the cumulative edge length over the boundary.
    //
    vector<HalfEdgeIter> boundaryEdges;
    vector<VertexIter> boundaryVertices;
    set<int> boundarySet;
    vector<float> edgeLengths; // cumulative edge lengths
    float totalEdgeLength = 0;

    HalfEdgeIter previousBoundary = firstBoundary;
    currentBoundary = firstBoundary;
    do {
        boundaryEdges.push_back(currentBoundary);
        boundaryVertices.push_back(currentBoundary->vertex);
        boundarySet.insert(currentBoundary->vertex->id);

        // cumulative edge length of 'currentBoundary->vertex'
        edgeLengths.push_back(totalEdgeLength);
        currentBoundary = hem.GetNextBoundary(currentBoundary);

        totalEdgeLength += vec3::distance(
            previousBoundary->vertex->p, currentBoundary->vertex->p
            );
        previousBoundary = currentBoundary;
    } while(currentBoundary != firstBoundary);

    // Now let us formulate the linear system. We have two systems:
    // W * x = bx
    // W * y = by
    // one system for each of the two uv-coordinates.
    Eigen::VectorXd bx(N);
    Eigen::VectorXd by(N);

    // Here's bx and by:
    // for non-boundary vertices, we have
    // (bx[i], by[i]) = (0,0)
    // for boundary vertices, we project them onto a circle.
    // so for boundary vertices we have:
    // (bx[i], by[i]) = (cos(theta),sin(theta))
    for(int i = 0; i < N; i++) {
        bx[i] = 0.0f;
        by[i] = 0.0f;
    }
    for(int i = 0; i < boundaryVertices.size(); i++) {
        VertexIter vit = boundaryVertices[i];
        double theta = (edgeLengths[i]/totalEdgeLength)*2.0f*M_PI;
        bx[vit->id] = cos(theta);
        by[vit->id] = sin(theta);
    }

    typedef Eigen::Triplet<double> Triplet;

    // W is very sparse, so much can be saved by using a sparse matrix.
    typedef Eigen::SparseMatrix<double> SparseMatrix;
    SparseMatrix W(N, N);

    vector<Triplet> triplets;
    vector<double> diag; // diagonal values in W.
    diag.resize(N, 0);

    for(EdgeIter eit = hem.BeginEdges(); eit != hem.EndEdges(); eit++) {
        // The boundary vertices are fixed(they are projected on a circle),
        // so we do not need to compute any weights of the boundary edges.
        // So the boundary edges contribute very little to the final linear system.
        if(hem.IsBoundary(eit)) {
            continue;
        }

        VertexIter v0 = eit->halfEdge->vertex;
        VertexIter v1 = eit->halfEdge->twin->vertex;

        int i0 = v0->id;
        int i1 = v1->id;

        float weight = HarmonicWeight(eit);

        // if we instead set the weight to one, then we get uniform weights. But that sucks, though.
//        weight = 1.0;

        // We set the weights for non-boundary edges.
        // Note that the conditionals are very important!
        // If i is some boundary vertex, then we need to make sure
        // that in row i, (i,i) is one, and all other elements are zero.
        // Because in the linear system we should have
        // 1.0 * x[i] = bx[i]
        // (so also below for more explanations)
        if(boundarySet.count(i0) == 0) {
            triplets.push_back(Triplet(i0, i1, weight));
        }
        if(boundarySet.count(i1) == 0) {
            triplets.push_back(Triplet(i1, i0, weight));
        }

        diag[i0] -= weight;
        diag[i1] -= weight;
    }

    for (int i = 0; i < diag.size(); i++) {
        if(boundarySet.count(i) > 0) {
            // for boundary vertices, diagonal is one.
            // The result of this will be that the i:th equation(that is, row i) in the linear system becomes
            // 1.0 * x[i] = bx[i]
            // which is correct. Because the boundary vertices are fixed,
            // and thus we already know the value of x[i].
            triplets.push_back(Triplet(i, i, 1.0));
        } else {
            // for non-boundary vertices, the diagonal is the NEGATIVE sum
            // of all non-diagonal elements in row i.
            triplets.push_back(Triplet(i, i, diag[i]));
        }
    }

    // construct sparse matrix.
    W.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::SparseLU<SparseMatrix > solver;
    solver.compute(W);
    if(solver.info()!=Eigen::Success) {
        printf("ERROR: found no decomposition of sparse matrix\n");
        exit(1);
    }

    // now finally solve!
    Eigen::VectorXd x(N);
    Eigen::VectorXd y(N);
    x = solver.solve(bx);
    y = solver.solve(by);

    // output uvs.
    for(int i = 0; i < N; i++) {
        outUvs.push_back(x[i]);
        outUvs.push_back(y[i]);
    }

    // recover all the edges of the flattened, uv-mapped mesh(useful for visualization):
    if(outUvEdges) {
        for(EdgeIter eit = hem.BeginEdges(); eit != hem.EndEdges(); eit++) {
            int i0 = eit->halfEdge->vertex->id;
            int i1 = eit->halfEdge->next->vertex->id;

            outUvEdges->push_back(outUvs[i0 * 2 + 0]);
            outUvEdges->push_back(outUvs[i0 * 2 + 1]);

            outUvEdges->push_back(outUvs[i1 * 2 + 0]);
            outUvEdges->push_back(outUvs[i1 * 2 + 1]);


        }
    }

    vVertices.clear();
    vFaces.clear();
    hem.ToMesh(vVertices, vFaces);



    for(vec3 v: vVertices) {
        outVertices.push_back(v.x);
        outVertices.push_back(v.y);
        outVertices.push_back(v.z);
    }

    for(Tri tri: vFaces) {
        outFaces.push_back(tri.i[0]);
        outFaces.push_back(tri.i[1]);
        outFaces.push_back(tri.i[2]);
    }
}
