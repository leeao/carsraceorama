#include "pch.h"
#include <vector>
#include <string>
#include <algorithm>
using namespace std;
int getRootBoneIndex(modelBone_t*& bones, int numBones);
void sortBone(modelBone_t*& bones, int numBones, int rootBoneIndex, vector<string>& boneMapStr);
void Split(string str, char splitChar, vector<string>& strArray);
bool Model_SLT_Write(noesisModel_t* mdl, RichBitStream* outStream, noeRAPI_t* rapi)
{
	sharedModel_t* pmdl = rapi->rpgGetSharedModel(mdl,
		NMSHAREDFL_WANTGLOBALARRAY | //calculate giant flat vertex/triangle arrays
		NMSHAREDFL_FLATWEIGHTS | //create flat vertex weight arrays
		NMSHAREDFL_FLATWEIGHTS_FORCE4 | //force 4 weights per vert for the flat weight array data
		NMSHAREDFL_REVERSEWINDING //reverse the face winding (as per UT) - most formats will not want you to do this!
		);

	modelBone_t* bones = pmdl->bones;
	int numBones = pmdl->numBones;
	if (!bones || numBones <= 0)
	{ //create a placeholder bone
		numBones = 1;
		bones = rapi->Noesis_AllocBones(1);
		strncpy_s(bones->name, 32, "root", 32);
		bones->mat = g_identityMatrix;
	}

	int numMatrials = pmdl->matData->numMaterials;

	float minX = 0, minY = 0, minZ = 0;
	float maxX = 0, maxY = 0, maxZ = 0;

	RichVec3 pivotPoint(bones->mat.o);//root node

	vector<string> allLines;
	char tempStr[512];

	//Quantization Info Block
	sprintf_s(tempStr, "[Quantization Info]\r\n");
	allLines.push_back(string(tempStr));
	sprintf_s(tempStr, "%f,%f,%f,%f\r\n", 0.0f, 0.0f, 0.0f, 0.0f);
	allLines.push_back(string(tempStr));

	//Pivot Point Block
	sprintf_s(tempStr, "[Pivot Point]\r\n");
	allLines.push_back(string(tempStr));
	sprintf_s(tempStr, "%f,%f,%f\r\n", pivotPoint.v[0], pivotPoint.v[1], pivotPoint.v[2]);
	allLines.push_back(string(tempStr));

	
	int numLOD = 1;
	int numSurface = pmdl->numMeshes;

	int XYZ = 1, Normals = 1;
	
	int UV1 = 1, UV2 = 0, UV3 = 0, UV4 = 0;
	int MorphShader = 0, Tangents = 0, OrigXYZBits = 16, OrigSTBits = 16;
	int Animates = numBones > 1 ? 1 : 0;
	int Weights = pmdl->meshes->numWeights > 0 ? 1 : 0;
	if (Weights)
	{
		for (int i = 0; i < pmdl->numMeshes; i++)
		{
			sharedMesh_t* mesh = pmdl->meshes + i;
			if (!mesh->flatBoneIdx)
			{
				Weights = 0;
				rapi->LogOutput("Warning: mesh \"%s\" without bone skin weight data, the weight data will not be exported. ",mesh->name);
			}
		}
	}

	//Components Block
	sprintf_s(tempStr, "[Components]\r\n");
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "XYZ=%i\r\n", XYZ);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "Normals=%i\r\n", Normals);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "UV1=%i\r\n", UV1);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "UV2=%i\r\n", UV2);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "UV3=%i\r\n", UV3);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "UV4=%i\r\n", UV4);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "MorphShader=%i\r\n", MorphShader);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "Tangents=%i\r\n", Tangents);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "OrigXYZBits=%i\r\n", OrigXYZBits);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "OrigSTBits=%i\r\n", OrigSTBits);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "Weights=%i\r\n", Weights);
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "Animates=%i\r\n", Animates);
	allLines.push_back(string(tempStr));

	//Meterials Block
	sprintf_s(tempStr, "[Materials]\r\n");
	allLines.push_back(string(tempStr));

	sprintf_s(tempStr, "NumberOfMaterials=%i\r\n",numMatrials);
	allLines.push_back(string(tempStr));

	for (int i = 0; i < numMatrials; i++)
	{
		noesisMaterial_t* material = pmdl->matData->materials + i;
		sprintf_s(tempStr, "[Material - %i]\r\n",i);
		allLines.push_back(string(tempStr));
		sprintf_s(tempStr, "MaterialName=%s\r\n", material->name);
		allLines.push_back(string(tempStr));
		sprintf_s(tempStr, "HasAlpha=%i\r\n", 0);
		allLines.push_back(string(tempStr));
	}
	
	//Object Hierarchy Block
	sprintf_s(tempStr, "[Object Hierarchy]\r\n");
	allLines.push_back(string(tempStr));
	/*
	for (int i = 0; i < numBones; i++)
	{
		modelBone_t *bone = bones + i;
		RichMat43 mat(bone->mat);
		char* parentName = bone->parentName;
		if (bone->eData.parent)
		{
			RichMat43 parentMat(bone->eData.parent->mat);
			parentMat.Inverse();
			mat = mat * parentMat;
		}
		else {
			parentName = (char*)"NONE";
		}
		mat.Transpose();
		sprintf_s(tempStr, "%s,%s,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n",bone->name,parentName,mat.m.x1[0], mat.m.x1[1], mat.m.x1[2], mat.m.x2[0], mat.m.x2[1], mat.m.x2[2], mat.m.x3[0], mat.m.x3[1], mat.m.x3[2], mat.m.o[0], mat.m.o[1], mat.m.o[2]);
		allLines.push_back(string(tempStr));
		rapi->LogOutput("userIndex:%d\n", bone->userIndex);
	}
	*/
	
	int rootBoneIndex = getRootBoneIndex(bones, numBones);
	int* boneMap = new int[numBones];
	memset(boneMap, 0, sizeof(int) * numBones);//if not have weight just fill root index
	vector<string> boneMapStr;
	sortBone(bones, numBones, rootBoneIndex, boneMapStr);
	vector<int> boneMapCheck;
	for (int i = 0; i < boneMapStr.size(); i++)
	{
		vector<string> splitStr;
		Split(boneMapStr[i], ',', splitStr);
		int boneIndex = stoi(splitStr[1]);
		boneMap[boneIndex] = i;
		boneMapCheck.push_back(boneIndex);
		modelBone_t* bone = bones + boneIndex;
		RichMat43 mat(bone->mat);
		char* parentName = bone->parentName;
		if (bone->eData.parent)
		{
			RichMat43 parentMat(bone->eData.parent->mat);
			parentMat.Inverse();
			mat = mat * parentMat;
		}
		else {
			parentName = (char*)"NONE";
		}
		mat.Transpose();
		sprintf_s(tempStr, "%s,%s,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n", bone->name, parentName, mat.m.x1[0], mat.m.x1[1], mat.m.x1[2], mat.m.x2[0], mat.m.x2[1], mat.m.x2[2], mat.m.x3[0], mat.m.x3[1], mat.m.x3[2], mat.m.o[0], mat.m.o[1], mat.m.o[2]);
		allLines.push_back(string(tempStr));		
	}



	//LOD Information Block
	sprintf_s(tempStr, "[LOD Information]\r\n");
	allLines.push_back(string(tempStr));
	sprintf_s(tempStr, "NumberOfLOD=%i\r\n", numLOD);
	allLines.push_back(string(tempStr));

	//LOD Block
	for (int lodIndex = 0; lodIndex < numLOD; lodIndex++)
	{
		sprintf_s(tempStr, "[LOD %i]\r\n",lodIndex);
		allLines.push_back(string(tempStr));
		sprintf_s(tempStr, "NumberOfSurfaces=%i\r\n", numSurface);
		allLines.push_back(string(tempStr));

		for (int meshIndex = 0; meshIndex < numSurface; meshIndex++)
		{
			sharedMesh_t* mesh = pmdl->meshes + meshIndex;
			unsigned short numVerts = mesh->numVerts;
			unsigned short numFaces = mesh->numTris;
			int numMaterials = 1;
			int materialID = mesh->materialIdx;
			sprintf_s(tempStr, "[LOD %i - Surface %i]\r\n", lodIndex,meshIndex);
			allLines.push_back(string(tempStr));
			sprintf_s(tempStr, "NumberOfVertices=%d\r\n", numVerts);
			allLines.push_back(string(tempStr));
			sprintf_s(tempStr, "NumberOfFaces=%d\r\n", numFaces);
			allLines.push_back(string(tempStr));
			sprintf_s(tempStr, "NumberOfMaterials=%d\r\n", numMaterials);
			allLines.push_back(string(tempStr));
			sprintf_s(tempStr, "Material#0=%d\r\n", materialID);
			allLines.push_back(string(tempStr));

			sprintf_s(tempStr, "[LOD %i - Surface %i - Vertices]\r\n", lodIndex, meshIndex);
			allLines.push_back(string(tempStr));

			BOOL hasColor = mesh->colors ? TRUE : FALSE;
			modelRGBA_t colorPadding = { 1.0f,1.0f,1.0f,1.0f };
			
			for (int i = 0; i < numVerts; i++)
			{
				modelVert_t* vert = mesh->verts + i;
				modelVert_t* normal = mesh->normals + i;
				modelTexCoord_t* uv = mesh->uvs + i;
				modelRGBA_t* color = hasColor ? mesh->colors + i : &colorPadding;
				sprintf_s(tempStr, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n",
					vert->x, vert->y, vert->z,
					normal->x, normal->y, normal->z,
					uv->u, uv->v, 0.0f,
					0.0f, 1.0f, 0.0f,
					0.0f, 1.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 
					color->rgba[0], color->rgba[1], color->rgba[2], 
					color->rgba[3] );
				allLines.push_back(string(tempStr));

				if (vert->x < minX) minX = vert->x;
				if (vert->y < minY) minY = vert->y;
				if (vert->z < minZ) minZ = vert->z;
				if (vert->x > maxX) maxX = vert->x;
				if (vert->y > maxY) maxY = vert->y;
				if (vert->z > maxZ) maxZ = vert->z;
			}

			sprintf_s(tempStr, "[LOD %i - Surface %i - Faces]\r\n", lodIndex, meshIndex);
			allLines.push_back(string(tempStr));

			for (int i = 0; i < numFaces; i++)
			{
				modelTriFace_t* face = mesh->tris + i;
				sprintf_s(tempStr,"%d,%d,%d\r\n", face->a, face->c, face->b);
				allLines.push_back(string(tempStr));
			}

			sprintf_s(tempStr, "[LOD %i - Surface %i - Object Pointer List]\r\n", lodIndex, meshIndex);
			allLines.push_back(string(tempStr));
			for (int i = 0; i < numBones; i++)
			{
				if (i + 1 != numBones)
				{
					sprintf_s(tempStr, "%d,%d\r\n", 0, 0);
					allLines.push_back(string(tempStr));
				}
				else
				{
					sprintf_s(tempStr, "%d,%d\r\n", 0, numVerts);
					allLines.push_back(string(tempStr));
				}
			}
			if (Weights)
			{
				sprintf_s(tempStr, "[LOD %i - Surface %i - Skinning]\r\n", lodIndex, meshIndex);
				allLines.push_back(string(tempStr));

				for (int i = 0; i < numVerts; i++)
				{
					int boneIndex[4]{ 0,0,0,0 };
					float boneWeight[4]{ 0.0f,0.0f,0.0f,0.0f };
					for (int w = 0; w < mesh->numWeightsPerVert; w++)
					{
						boneIndex[w] = *(int*)(mesh->flatBoneIdx + i * mesh->numWeightsPerVert + w);
						boneWeight[w] = *(float*)(mesh->flatBoneWgt + i * mesh->numWeightsPerVert + w);
						if (boneMapStr.size() < numBones)
						{
							vector<int>::iterator foundBone;
							foundBone = find(boneMapCheck.begin(), boneMapCheck.end(), boneIndex[w]);
							if (foundBone == boneMapCheck.end())
							{
								rapi->LogOutput("Warning: Mesh: %s Vertex ID: %d is missing weights, auto mapped to root bone.\n",
									mesh->name, i);
							}

						}
						boneIndex[w] = boneMap[boneIndex[w]];//remap bone index


						
					}

					sprintf_s(tempStr, "%i,%i,%i,%i,%f,%f,%f,%f\r\n",
						boneIndex[0], boneIndex[1], boneIndex[2], boneIndex[3],
						boneWeight[0], boneWeight[1], boneWeight[2], boneWeight[3] );
					allLines.push_back(string(tempStr));
				}
			}


		}
	}

	float max = 0.0f;
	RichVec3 bmin{minX,minY,minZ}, bmax{maxX,maxY,maxZ};
	RichVec3 tempV = (bmax - bmin) * 0.5f;

	float quanGroupBBHalf = fmaxf(tempV.v[0], fmaxf(tempV.v[1], tempV.v[2]));
	//RichVec3 quanGroupBBCenter = bmin + tempV;
	RichVec3 quanGroupBBCenter = (bmin + bmax) * 0.5f;

	sprintf_s(tempStr, "%f,%f,%f,%f\r\n", quanGroupBBCenter.v[0], quanGroupBBCenter.v[1], quanGroupBBCenter.v[2], quanGroupBBHalf);
	allLines[1] = string(tempStr);


	for (int i = 0; i < allLines.size(); i++)
	{		
		outStream->WriteBytes(allLines[i].data(), allLines[i].size());
	}
	return true;

}
int getRootBoneIndex(modelBone_t*& bones, int numBones)
{
	for (int i = 0; i < numBones; i++)
	{
		modelBone_t* bone = bones + i;
		if (!bone->eData.parent)
		{
			for (int j = 0; j < numBones; j++)
			{
				modelBone_t* sbone = bones + j;
				{
					if (sbone->eData.parent)
					{
						if (string(sbone->parentName) == string(bone->name))
						{
							return bone->index;
						}
					}
				}
			}
		}
	}
	return 0;
}

void sortBone(modelBone_t*& bones, int numBones,int rootBoneIndex, vector<string>&boneMapStr)
{
	modelBone_t *rootBone = bones + rootBoneIndex;
	char tempStr[256];
	sprintf_s(tempStr, "%s,%i", rootBone->name, rootBone->index);
	if (boneMapStr.size() == 0) boneMapStr.push_back(string(tempStr)); //add root bone
	vector<string> subBones;

	for (int j = 0; j < numBones; j++)
	{
		modelBone_t* subBone = bones + j;
		{
			if (subBone->eData.parent)
			{
				if (string(subBone->parentName) == string(rootBone->name))
				{
					sprintf_s(tempStr, "%s,%i", subBone->name, subBone->index);
					subBones.push_back(string(tempStr));
				}
			}
		}
	}
	if (subBones.size() > 0)
	{
		sort(subBones.begin(), subBones.end(), [](string a, string b) {return a < b; });
		int *subBoneIDs = new int[subBones.size()];
		for (int i = 0; i < subBones.size(); i++)
		{
			vector<string> splitStr;
			Split(subBones[i], ',', splitStr);
			int subBoneIndex = stoi(splitStr[1]);
			subBoneIDs[i] = subBoneIndex;

			modelBone_t* subBone = bones + subBoneIndex;
			sprintf_s(tempStr, "%s,%i", subBone->name, subBone->index);
			boneMapStr.push_back(string(tempStr));
		}

		for (int i = 0; i < subBones.size(); i++)
		{
			sortBone(bones, numBones, subBoneIDs[i], boneMapStr);
		}
	}
	
	
}
void Split(string str, char splitChar, vector<string>& strArray)
{
	size_t pos = str.find(splitChar);//',' 
	size_t size = str.size();
	if (pos != str.npos)
	{
		string parametrName = str.substr(0, pos);
		string nextStr = str.substr(pos + 1, size - 1 - pos);
		strArray.push_back(parametrName);
		Split(nextStr, splitChar, strArray);

	}
	else
	{
		strArray.push_back(str);
	}
}