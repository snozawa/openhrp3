/*
 * Copyright (c) 2008, AIST, the University of Tokyo and General Robotix Inc.
 * All rights reserved. This program is made available under the terms of the
 * Eclipse Public License v1.0 which accompanies this distribution, and is
 * available at http://www.eclipse.org/legal/epl-v10.html
 * Contributors:
 * National Institute of Advanced Industrial Science and Technology (AIST)
 * General Robotix Inc. 
 */

/*!
  @file TriangleMeshShaper.cpp
  @author Shin'ichiro Nakaoka
*/

#include "TriangleMeshShaper.h"

#include <iostream>
#include <math.h>

using namespace std;
using namespace boost;
using namespace OpenHRP;


namespace {
    const double PI = 3.14159265358979323846;
}


TriangleMeshShaper::TriangleMeshShaper()
{
    divisionNumber = 20;
    isNormalGenerationMode = true;
}


/*!
  @if jp
  プリミティブ形状のメッシュ化時における分割数の指定。

  デフォルトの分割数は20としている。
  @endif
*/
void TriangleMeshShaper::setDivisionNumber(int n)
{
    divisionNumber = n;
}


/*!
  @if jp
  整形時に法線も生成するかどうかを指定する
  @endif
*/
void TriangleMeshShaper::setNormalGenerationMode(bool on)
{
    isNormalGenerationMode = on;
}


/*!
  @if jp
  変換後のShapeノードに対して、変換前のノードが持っていたGeometryNodeを返す。

  例えば、元のGeometryがプリミティブ形式だった場合、プリミティブの種類やパラメータを知ることが出来る。
  @endif
*/
VrmlGeometryPtr TriangleMeshShaper::getOriginalGeometry(VrmlShapePtr shapeNode)
{
    VrmlGeometryPtr originalGeometryNode;
    ShapeToGeometryMap::iterator p = shapeToOriginalGeometryMap.find(shapeNode);
    if(p != shapeToOriginalGeometryMap.end()){
        originalGeometryNode = p->second;
    }
    return originalGeometryNode;
}


/*!
  @if jp
  整形処理 (ModelNodeSet内のShape)

  引数で与えられたノードをトップとするシーングラフ中の Shape ノードを
  IndexedFaceSet形式においてすべてのポリゴンを三角形とした統一的な幾何形状形式に変換する.
  統一幾何形状形式に変換できないノードは削除される。

  @return 変換後のトップノード。
  トップノードがShapeノードのときに統一幾何形状形式へと変換出来なかった場合は無効なポインタを返す。
  @endif
*/
VrmlNodePtr TriangleMeshShaper::apply(VrmlNodePtr topNode)
{
    int numInconvertibleNodes = 0;
    bool resultOfTopNode = traverseShapeNodes(topNode.get(), 0, 0);

    return resultOfTopNode ? topNode : VrmlNodePtr();
}


bool TriangleMeshShaper::traverseShapeNodes(VrmlNode* node, AbstractVrmlGroup* parentNode, int indexInParent)
{
    bool result = true;

    if(node->isCategoryOf(PROTO_INSTANCE_NODE)){
        VrmlProtoInstance* protoInstance = static_cast<VrmlProtoInstance*>(node);
        if(protoInstance->actualNode){
            traverseShapeNodes(protoInstance->actualNode.get(), parentNode, indexInParent);
        }

    } else if(node->isCategoryOf(GROUPING_NODE)){
        AbstractVrmlGroup* group = static_cast<AbstractVrmlGroup*>(node);
        int numChildren = group->countChildren();
        for(int i = 0; i < numChildren; i++){
            traverseShapeNodes(group->getChild(i), group, i);
        }

    } else if(node->isCategoryOf(SHAPE_NODE)){
        VrmlShape* shapeNode = static_cast<VrmlShape*>(node);
        result = convertShapeNode(shapeNode);
        if(!result){
            if(parentNode){
                putMessage("Node is inconvertible and removed from the scene graph");
                parentNode->removeChild(indexInParent);
            }
        }
    }

    return result;
}


bool TriangleMeshShaper::convertShapeNode(VrmlShape* shapeNode)
{
    bool result = false;
    
    VrmlGeometry* geometry = shapeNode->geometry.get();
    VrmlIndexedFaceSetPtr triangleMesh;

    if(VrmlIndexedFaceSet* faceSet = dynamic_cast<VrmlIndexedFaceSet*>(geometry)){
        if(faceSet->coord){
            result = convertIndexedFaceSet(faceSet);
            triangleMesh = faceSet;
        }

    } else {
    
        triangleMesh = new VrmlIndexedFaceSet();
        triangleMesh->coord = new VrmlCoordinate();
        
        if(VrmlBox* box = dynamic_cast<VrmlBox*>(geometry)){
            result = convertBox(box, triangleMesh);
            
        } else if(VrmlCone* cone = dynamic_cast<VrmlCone*>(geometry)){
            result = convertCone(cone, triangleMesh);
            
        } else if(VrmlCylinder* cylinder = dynamic_cast<VrmlCylinder*>(geometry)){
            result = convertCylinder(cylinder, triangleMesh);
            
        } else if(VrmlSphere* sphere = dynamic_cast<VrmlSphere*>(geometry)){
            result = convertSphere(sphere, triangleMesh);
            
        } else if(VrmlElevationGrid* elevationGrid = dynamic_cast<VrmlElevationGrid*>(geometry)){
            result = convertElevationGrid(elevationGrid, triangleMesh);
            
        } else if(VrmlExtrusion* extrusion = dynamic_cast<VrmlExtrusion*>(geometry)){
            result = convertExtrusion(extrusion, triangleMesh);
        }

        if(result){
            shapeToOriginalGeometryMap[shapeNode] = geometry;
            shapeNode->geometry = triangleMesh;
        }
    }
    
    if(result && !triangleMesh->normal && isNormalGenerationMode){
        generateNormals(triangleMesh);
    }

    return result;
}


/**
   \todo local vector variables should be member variables to increase the performance
*/
bool TriangleMeshShaper::convertIndexedFaceSet(VrmlIndexedFaceSet* faceSet)
{
    MFVec3f& vertices = faceSet->coord->point;
    int numVertices = vertices.size();

    MFInt32& indices = faceSet->coordIndex;
    const MFInt32 orgIndices = indices;
    indices.clear();

    const int numOrgIndices = orgIndices.size();

    int orgFaceIndex = 0;
    int newFaceIndex = 0;
    int polygonTopIndexPosition = 0;

    indexPositionMap.clear();
    faceIndexMap.clear();

    for(int i=0; i < numOrgIndices; ++i){

        int index = orgIndices[i];

        if(index >= numVertices){
            putMessage("The coordIndex field has an index over "
                       "the size of the vertices in the coord field");
            return false;
        }

        if(index >= 0){
            polygon.push_back(index);

        } else {
            trianglesInPolygon.clear();
            int numTriangles = addTrianglesDividedFromPolygon(polygon, vertices, trianglesInPolygon);
            
            if(numTriangles > 0){
                // \todo ここで3頂点に重なりはないか、距離が短すぎないかなどのチェックを行った方がよい
                for(int j=0; j < numTriangles; ++j){
                    for(int k=0; k < 3; ++k){
                        int indexInPolygon = trianglesInPolygon[j*3 + k];
                        indices.push_back(polygon[indexInPolygon]);
                        indexPositionMap.push_back(polygonTopIndexPosition + indexInPolygon);
                    }
                    indices.push_back(-1);
                    indexPositionMap.push_back(-1);
                }
                
                for(int j=0; j < numTriangles; ++j){
                    faceIndexMap.push_back(orgFaceIndex);
                }
            }
            polygonTopIndexPosition = i + 1;
            orgFaceIndex++;
            polygon.clear();
        }
    }

    bool result = true;

    int numColors = faceSet->color ? faceSet->color->color.size() : 0;
    result &= checkAndRemapIndices
        (REMAP_COLOR, numColors, faceSet->colorIndex, faceSet->colorPerVertex, faceSet);

    int numNormals = faceSet->normal ? faceSet->normal->vector.size() : 0;
    result &= checkAndRemapIndices
        (REMAP_NORMAL, numNormals, faceSet->normalIndex, faceSet->normalPerVertex, faceSet);

    return (result && !indices.empty());
}


namespace {

    inline int addVertex(MFVec3f& vertices, const double x, const double y, const double z)
    {
        SFVec3f v;
        v[0] = x;
        v[1] = y;
        v[2] = z;
        vertices.push_back(v);
        return vertices.size() - 1;
    }

    inline void addTriangle(MFInt32& indices, int x, int y, int z)
    {
        indices.push_back(x);
        indices.push_back(y);
        indices.push_back(z);
        indices.push_back(-1);
    }

    inline void add3Elements(vector<int>& v, int x, int y, int z)
    {
        v.push_back(x);
        v.push_back(y);
        v.push_back(z);
    }
}


/*!
  @if jp
  @note 現時点では，分割対象のメッシュは三角形・四角形のメッシュのみに対応.
  @note 一般的な三角形分割のアルゴリズムについては "triangulation algorithm" や "polygon triangulation" で検索するべし
  @endif
*/
int TriangleMeshShaper::addTrianglesDividedFromPolygon
(const vector<int>& polygon, const MFVec3f& vertices, vector<int>& out_trianglesInPolygon)
{
    int numTriangles = -1;
    int numVertices = polygon.size();

    if(numVertices < 3){
        /// \todo define a proper exception
        putMessage("The number of a face is less than tree."); 
            
    } else if(numVertices == 3){
        numTriangles = 1;
        add3Elements(out_trianglesInPolygon, 0, 1, 2);

    } else if(numVertices == 4){
        numTriangles = 2;

        const SFVec3f& v = vertices[polygon[0]];

        Vector3 v0(vertices[polygon[0]].begin(), 3);
        Vector3 v1(vertices[polygon[1]].begin(), 3);
        Vector3 v2(vertices[polygon[2]].begin(), 3);
        Vector3 v3(vertices[polygon[3]].begin(), 3);
        
        double distance02 = norm2(v0 - v2);
        double distance13 = norm2(v1 - v3);

        // 対角線の長さが短い方で分割する
        if(distance02 < distance13){
            add3Elements(out_trianglesInPolygon, 0, 1, 2);
            add3Elements(out_trianglesInPolygon, 0, 2, 3);
        } else {
            add3Elements(out_trianglesInPolygon, 0, 1, 3);
            add3Elements(out_trianglesInPolygon, 1, 2, 3);
        }
    } else if(numTriangles >= 5){
        putMessage( "The number of vertex is 5 or more." );
    }

    return numTriangles;
}


template <class TArray>
bool TriangleMeshShaper::remapDirectMapObjectsPerFaces(TArray& values, const char* valueName)
{
    const TArray orgValues = values;
    int numOrgValues = orgValues.size();
    int numFaces = faceIndexMap.size();
    values.resize(numFaces);
    for(int i=0; i < numFaces; ++i){
        int faceIndex = faceIndexMap[i];
        if(faceIndex >= numOrgValues){
            putMessage(string("The number of ") + valueName + " is less than the number of faces.");
            return false;
        }
        values[i] = orgValues[faceIndex];
    }
    return true;
}


bool TriangleMeshShaper::checkAndRemapIndices
(RemapType type, int numElements, MFInt32& indices, bool perVertex, VrmlIndexedFaceSet* triangleMesh)
{
    const char* valueName = (type==REMAP_COLOR) ? "colors" : "normals";
    
    bool result = true;
    
    if(numElements == 0){
        if(!indices.empty()){
            putMessage(string("An IndexedFaceSet has no ") + valueName +
                       ", but it has a non-empty index field of " + valueName + ".");
            result = false;
        }

    } else {

        if(indices.empty()){
            if(perVertex){
                if(numElements < triangleMesh->coord->point.size()){
                    putMessage(string("The number of ") + valueName +
                               " is less than the number of vertices.");
                    result = false;
                }
            } else {
                if(type == REMAP_COLOR){
                    remapDirectMapObjectsPerFaces(triangleMesh->color->color, valueName);
                } else if(type == REMAP_NORMAL){
                    remapDirectMapObjectsPerFaces(triangleMesh->normal->vector, valueName);
                }
            }
        } else {
            const MFInt32 orgIndices = indices;

            if(perVertex){
                int numNewIndices = indexPositionMap.size();
                indices.resize(numNewIndices);
                for(int i=0; i < numNewIndices; ++i){
                    int orgPosition = indexPositionMap[i];
                    if(orgPosition < 0){
                        indices[i] = -1;
                    } else {
                        int index = orgIndices[orgPosition];
                        if(index < numElements){
                            indices[i] = index;
                        } else {
                            putError1(valueName);
                            result = false;
                        }
                    }
                }
            } else {
                int numNewIndices = faceIndexMap.size();
                indices.resize(numNewIndices);
                for(int i=0; i < numNewIndices; ++i){
                    int orgFaceIndex = faceIndexMap[i];
                    int index = orgIndices[orgFaceIndex];
                    if(index < numElements){
                        indices[i] = index;
                    } else {
                        putError1(valueName);
                        result = false;
                    }
                }
            }
        }
    }

    return result;
}


void TriangleMeshShaper::putError1(const char* valueName)
{
    putMessage(string("There is an index of ") + valueName +
               " beyond the size of " + valueName + ".");
}

    
bool TriangleMeshShaper::convertBox(VrmlBox* box, VrmlIndexedFaceSetPtr& triangleMesh)
{
    const double x = box->size[0] / 2.0;
    const double y = box->size[1] / 2.0;
    const double z = box->size[2] / 2.0;
    
    if(x < 0.0 || y < 0.0 || z < 0.0 ){
        putMessage("BOX : wrong value.");
        return false;
    }

    MFVec3f& vertices = triangleMesh->coord->point;
    vertices.reserve(8);
    
    static const double xsigns[] = { -1.0, -1.0, -1.0, -1.0,  1.0,  1.0,  1.0, 1.0 };
    static const double ysigns[] = { -1.0, -1.0,  1.0,  1.0, -1.0, -1.0,  1.0, 1.0 };
    static const double zsigns[] = { -1.0,  1.0, -1.0,  1.0, -1.0,  1.0, -1.0, 1.0 };

    for(int i=0; i < 8; ++i){
        addVertex(vertices, xsigns[i] * x, ysigns[i] * y, zsigns[i] * z);
    }
    
    static const int numTriangles = 12;
    static const int triangles[] =
        { 5, 7, 3,
          5, 3, 1,
          0, 2, 6,
          0, 6, 4,
          4, 6, 7,
          4, 7, 5,
          1, 3, 2,
          1, 2, 0,
          7, 6, 2,
          7, 2, 3,
          4, 5, 1,
          4, 1, 0,
        };

    MFInt32& indices = triangleMesh->coordIndex;
    indices.resize(numTriangles * 4);

    int di = 0;
    int si = 0;
    for(int i=0; i < numTriangles; i++){
        indices[di++] = triangles[si++];
        indices[di++] = triangles[si++];
        indices[di++] = triangles[si++];
        indices[di++] = -1;
    }

    return true;
}


bool TriangleMeshShaper::convertCone(VrmlCone* cone, VrmlIndexedFaceSetPtr& triangleMesh)
{
    const double radius = cone->bottomRadius;
    
    if(cone->height < 0.0 || radius < 0.0 ){
        putMessage( "CONE : wrong value." );
        return false;
    }

    MFVec3f& vertices = triangleMesh->coord->point;
    vertices.reserve(divisionNumber + 1);

    for(int i=0;  i < divisionNumber; ++i){
        const double angle = i * 2.0 * PI / divisionNumber;
        addVertex(vertices, radius * cos(angle), 0.0, radius * sin(angle));
    }

    const int topIndex = addVertex(vertices, 0.0, cone->height, 0.0);
    const int bottomCenterIndex = addVertex(vertices, 0.0, 0.0, 0.0);

    MFInt32& indices = triangleMesh->coordIndex;
    indices.reserve((divisionNumber * 2) * 4);
    const int offset = divisionNumber * 2;

    for(int i=0; i < divisionNumber; ++i){
        // side faces
        addTriangle(indices, topIndex, (i + 1) % divisionNumber, i);
        // bottom faces
        addTriangle(indices, bottomCenterIndex, i, (i + 1) % divisionNumber);
    }

    triangleMesh->creaseAngle = 3.14 / 2.0;

    return true;
}


bool TriangleMeshShaper::convertCylinder(VrmlCylinder* cylinder, VrmlIndexedFaceSetPtr& triangleMesh)
{
    if(cylinder->height < 0.0 || cylinder->radius < 0.0){
        putMessage("CYLINDER : wrong value.");
        return false;
    }

    MFVec3f& vertices = triangleMesh->coord->point;
    vertices.reserve(divisionNumber * 2 + 2);
    vertices.resize(divisionNumber * 2);

    const double y = cylinder->height / 2.0;

    for(int i=0 ; i < divisionNumber ; i++ ){
        const double angle = i * 2.0 * PI / divisionNumber;
        SFVec3f& vtop = vertices[i];
        SFVec3f& vbottom = vertices[i + divisionNumber];
        vtop[0] = vbottom[0] = cylinder->radius * cos(angle);
        vtop[2] = vbottom[2] = cylinder->radius * sin(angle);
        vtop[1]    =  y;
        vbottom[1] = -y;
    }

    const int topCenterIndex    = addVertex(vertices, 0.0,  y, 0.0);
    const int bottomCenterIndex = addVertex(vertices, 0.0, -y, 0.0);

    MFInt32& indices = triangleMesh->coordIndex;
    indices.reserve((divisionNumber * 4) * 4);

    for(int i=0; i < divisionNumber; ++i){
        // top face
        addTriangle(indices, topCenterIndex, (i+1) % divisionNumber, i);
        // side face (upward convex triangle)
        addTriangle(indices, i, ((i+1) % divisionNumber) + divisionNumber, i + divisionNumber);
        // side face (downward convex triangle)
        addTriangle(indices, i, (i+1) % divisionNumber, ((i + 1) % divisionNumber) + divisionNumber);
        // bottom face
        addTriangle(indices, bottomCenterIndex, i + divisionNumber, ((i+1) % divisionNumber) + divisionNumber);
    }

    triangleMesh->creaseAngle = 3.14 / 2.0;

    return true;
}


bool TriangleMeshShaper::convertSphere(VrmlSphere* sphere, VrmlIndexedFaceSetPtr& triangleMesh)
{
    const double r = sphere->radius;

    if(r < 0.0) {
        putMessage("SPHERE : wrong value.");
        return false;
    }

    const int vdn = divisionNumber;  // latitudinal division number
    const int hdn = divisionNumber;  // longitudinal division number
    
    MFVec3f& vertices = triangleMesh->coord->point;
    vertices.reserve((vdn - 1) * hdn + 2);

    for(int i=1; i < vdn; i++){ // latitudinal direction
        double tv = i * PI / vdn;
        for(int j=0; j < hdn; j++){ // longitudinal direction
            double th = j * 2.0 * PI / hdn;
            addVertex(vertices, r*sin(tv)*cos(th), r*cos(tv), r*sin(tv)*sin(th));
        }
    }
    
    const int topIndex    = addVertex(vertices, 0.0,  r, 0.0);
    const int bottomIndex = addVertex(vertices, 0.0, -r, 0.0);

    MFInt32& indices = triangleMesh->coordIndex;
    indices.reserve(vdn * hdn * 2 * 4);

    // top faces
    for(int i=0; i < hdn; ++i){
        addTriangle(indices, topIndex, (i+1) % hdn, i);
    }

    // side faces
    for(int i=0; i < vdn - 1; ++i){
        const int upper = i * hdn;
        const int lower = (i + 1) * hdn;
        for(int j=0; j < hdn; ++j) {
            // upward convex triangle
            addTriangle(indices, j + upper, ((j + 1) % hdn) + lower, j + lower);
            // downward convex triangle
            addTriangle(indices, j + upper, ((j + 1) % hdn) + upper, ((j + 1) % hdn) + lower);
        }
    }
    
    // bottom faces
    const int offset = (vdn - 2) * hdn;
    for(int i=0; i < hdn; ++i){
        addTriangle(indices, bottomIndex, (i % hdn) + offset, ((i+1) % hdn) + offset);
    }

    triangleMesh->creaseAngle = PI;

    return true;
}


/**
   \todo copy colors and color indices to triangleMesh
*/
bool TriangleMeshShaper::convertElevationGrid(VrmlElevationGrid* grid, VrmlIndexedFaceSetPtr& triangleMesh)
{
    if(grid->xDimension * grid->zDimension != static_cast<SFInt32>(grid->height.size())){
        putMessage("ELEVATIONGRID : wrong value.");
        return false;
    }

    MFVec3f& vertices = triangleMesh->coord->point;
    vertices.reserve(grid->zDimension * grid->xDimension);

    for(int z=0; z < grid->zDimension; z++){
        for(int x=0; x < grid->xDimension; x++ ){
            addVertex(vertices, x * grid->xSpacing, grid->height[z * grid->xDimension + x], z * grid->zSpacing);
        }
    }

    MFInt32& indices = triangleMesh->coordIndex;
    indices.reserve((grid->zDimension - 1) * (grid->xDimension - 1) * 2 * 4);

    for(int z=0; z < grid->zDimension - 1; ++z){
        const int current = z * grid->xDimension;
        const int next = (z + 1) * grid->xDimension;
        for(int x=0; x < grid->xDimension - 1; ++x){
            addTriangle(indices, x + current, x + next, (x + 1) + next);
            addTriangle(indices, x + current, (x + 1) + next, (x + 1) + current);
        }
    }

    triangleMesh->creaseAngle = grid->creaseAngle;

    return true;
}


/*!
  \todo implement this function
*/
bool TriangleMeshShaper::convertExtrusion(VrmlExtrusion* extrusion, VrmlIndexedFaceSetPtr& triangleMesh)
{
    return false;
}


void TriangleMeshShaper::generateNormals(VrmlIndexedFaceSetPtr& triangleMesh)
{
    triangleMesh->normal = new VrmlNormal();
    triangleMesh->normalPerVertex = (triangleMesh->creaseAngle > 0.0) ? true : false;

    calculateFaceNormals(triangleMesh);

    if(triangleMesh->normalPerVertex){
        calculateVertexNormals(triangleMesh);
    } else {
        setFaceNormals(triangleMesh);
    }
}


void TriangleMeshShaper::calculateFaceNormals(VrmlIndexedFaceSetPtr& triangleMesh)
{
    const MFVec3f& vertices = triangleMesh->coord->point;
    const int numVertices = vertices.size();

    const MFInt32& triangles = triangleMesh->coordIndex;
    const int numFaces = triangles.size() / 4;

    faceNormals.clear();

    if(triangleMesh->normalPerVertex){
        vertexIndexToNormalIndicesMap.clear();
        vertexIndexToNormalIndicesMap.resize(numVertices);
    }

    Vector3 v[3];
    
    for(int faceIndex=0; faceIndex < numFaces; ++faceIndex){

        for(int i=0; i < 3; ++i){
            int vertexIndex = triangles[faceIndex * 4 + i];
            const SFVec3f& vorg = vertices[vertexIndex];
            v[i] = vorg[0], vorg[1], vorg[2];
        }

        Vector3 normal(tvmet::normalize(tvmet::cross(v[1] - v[0], v[2] - v[0])));

        faceNormals.push_back(normal);

        if(triangleMesh->normalPerVertex){
            for(int i=0; i < 3; ++i){
                int vertexIndex = triangles[faceIndex * 4 + i];
                vector<int>& facesOfVertex = vertexIndexToNormalIndicesMap[vertexIndex];
                for(int j=0; j < facesOfVertex.size(); ++j){
                    const Vector3& otherNormal = faceNormals[facesOfVertex[j]];
                    const Vector3 d(otherNormal - normal);
                    // the same face is not appended
                    if(tvmet::dot(d, d) > numeric_limits<double>::epsilon()){
                        facesOfVertex.push_back(faceIndex);
                    }
                }
            }
        }
    }
}


void TriangleMeshShaper::calculateVertexNormals(VrmlIndexedFaceSetPtr& triangleMesh)
{
    const MFVec3f& vertices = triangleMesh->coord->point;
    const int numVertices = vertices.size();

    const MFInt32& triangles = triangleMesh->coordIndex;
    const int numFaces = triangles.size() / 4;

    MFVec3f& normals = triangleMesh->normal->vector;
    MFInt32& normalIndices = triangleMesh->normalIndex;
    normalIndices.clear();
    normalIndices.reserve(triangles.size());

    faceIndexToFaceNormalIndexMap.clear();
    faceIndexToFaceNormalIndexMap.resize(numFaces, -1);

    vertexIndexToNormalIndicesMap.clear();
    vertexIndexToNormalIndicesMap.resize(numVertices);

    const double cosCreaseAngle = cos(triangleMesh->creaseAngle);

    for(int faceIndex=0; faceIndex < numFaces; ++faceIndex){

        for(int i=0; i < 3; ++i){

            int vertexIndex = triangles[faceIndex * 4 + i];
            vector<int>& facesOfVertex = vertexIndexToNormalIndicesMap[vertexIndex];
            const Vector3& currentFaceNormal = faceNormals[faceIndex];
            Vector3 normalSum = currentFaceNormal;
            bool normalIsFaceNormal = true;

            // avarage normals of the faces whose crease angle is below the 'creaseAngle' variable
            for(int j=0; j < facesOfVertex.size(); ++j){
                int adjoingFaceIndex = facesOfVertex[j];
                const Vector3& adjoingFaceNormal = faceNormals[adjoingFaceIndex];
                double cosa = (tvmet::dot(currentFaceNormal, adjoingFaceNormal)
                               / (tvmet::norm2(currentFaceNormal) * tvmet::norm2(adjoingFaceNormal)));
                if(cosa > cosCreaseAngle){
                    normalSum += adjoingFaceNormal;
                    normalIsFaceNormal = false;
                }
            }

            vector<int>& normalIndicesOfVertex = vertexIndexToNormalIndicesMap[vertexIndex];

            if(normalIsFaceNormal){
                int faceNormalIndex = faceIndexToFaceNormalIndexMap[faceIndex];
                if(faceNormalIndex < 0){
                    SFVec3f n;
                    std::copy(currentFaceNormal.begin(), currentFaceNormal.end(), n.begin());
                    faceNormalIndex = normals.size();
                    normals.push_back(n);
                    faceIndexToFaceNormalIndexMap[faceIndex] = faceNormalIndex;
                    normalIndicesOfVertex.push_back(faceNormalIndex);
                }
                normalIndices.push_back(faceNormalIndex);

            } else {
                const Vector3 normal(normalize(normalSum));
                int normalIndex = -1;

                // find the same normal from the existing normals
                for(int j=0; j < normalIndicesOfVertex.size(); ++j){
                    int index = normalIndicesOfVertex[j];
                    const SFVec3f& norg = normals[index];
                    const Vector3 n(norg[0], norg[1], norg[2]);
                    if(tvmet::norm2(n - normal) <= numeric_limits<double>::epsilon()){
                        normalIndex = index;
                        break;
                    }
                }
                if(normalIndex < 0){
                    SFVec3f n;
                    std::copy(normal.begin(), normal.end(), n.begin());
                    normalIndex = normals.size();
                    normals.push_back(n);
                    normalIndicesOfVertex.push_back(normalIndex);
                }
                normalIndices.push_back(normalIndex);
            } 
        }
        normalIndices.push_back(-1);
    }
}


void TriangleMeshShaper::setFaceNormals(VrmlIndexedFaceSetPtr& triangleMesh)
{
    const MFInt32& triangles = triangleMesh->coordIndex;
    const int numFaces = triangles.size() / 4;

    MFVec3f& normals = triangleMesh->normal->vector;
    MFInt32& normalIndices = triangleMesh->normalIndex;
    normalIndices.clear();
    normalIndices.reserve(triangles.size());

    const int numVertices = triangleMesh->coord->point.size();
    vertexIndexToNormalIndicesMap.clear();
    vertexIndexToNormalIndicesMap.resize(numVertices);

    for(int faceIndex=0; faceIndex < numFaces; ++faceIndex){

        const Vector3& normal = faceNormals[faceIndex];
        int normalIndex = -1;

        // find the same normal from the existing normals
        for(int i=0; i < 3; ++i){
            int vertexIndex = triangles[faceIndex * 4 + i];
            vector<int>& normalIndicesOfVertex = vertexIndexToNormalIndicesMap[vertexIndex];
            for(int j=0; j < normalIndicesOfVertex.size(); ++j){
                int index = normalIndicesOfVertex[j];
                const SFVec3f& norg = normals[index];
                const Vector3 n(norg[0], norg[1], norg[2]);
                if(tvmet::norm2(n - normal) <= numeric_limits<double>::epsilon()){
                    normalIndex = index;
                    break;
                }
            }
            if(normalIndex >= 0){
                break;
            }
        }
        if(normalIndex < 0){
            SFVec3f n;
            std::copy(normal.begin(), normal.end(), n.begin());
            normalIndex = normals.size();
            normals.push_back(n);
            for(int i=0; i < 3; ++i){
                int vertexIndex = triangles[faceIndex * 4 + i];
                vector<int>& normalIndicesOfVertex = vertexIndexToNormalIndicesMap[vertexIndex];
                normalIndicesOfVertex.push_back(normalIndex);
            }
        }
        normalIndices.push_back(normalIndex);
    }
}
    

void TriangleMeshShaper::putMessage(const std::string& message)
{
    if(!sigMessage.empty()){
        sigMessage(message + "\n" );
    }
}