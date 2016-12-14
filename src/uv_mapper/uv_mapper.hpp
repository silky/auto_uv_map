
#include <vector>

/*
  Automatically UV maps an input mesh with Harmonic Mapping.

  inVertices: The vertex positions of the input mesh, stored as a list of three-dimensional vectors.
  inFaces: The triangle indices of the input mesh, stored in counter-clockwise order.

  outVertices: The vertex positions of the output mesh.
  outFaces: The triangle indices of the output mesh.
  outUvs: The UV coordinates of the output mesh.
  outUvEdges: If non-null, the function will output the UV-coordinate edges of the UV mapping.
  These are useful for visualizing the mapping. Stored as a list of two-dimensional vectors. Where every pair of vectors is one line.

 */
void uvMap(
    const std::vector<float>& inVertices,
    const std::vector<int>& inFaces,

    std::vector<float>& outVertices,
    std::vector<int>& outFaces,
    std::vector<float>& outUvs,
    std::vector<float>* outUvEdges
    );
