#include "pch.h"
#include "textStreamRead.h"
#include <string>
#include <fstream>
#include <ostream>
#include <iostream>
using namespace std;

//load it (note that you don't need to worry about validation here, if it was done in the Check function)
noesisModel_t* Model_SLT_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi)
{
	void* pgctx = rapi->rpgCreateContext();

	char* tempName = (char*)"carstemp.txt";
	ofstream tempFile(tempName,ios::binary);
	tempFile.write((char*)fileBuffer, bufferLen);
	tempFile.close();
	
	textStreamRead reader;
	reader.SetCurrentFile(tempName);

	reader.ReadParameterBlock2("Components");
	int hasPos = 0,hasNormal = 0, hasUV1 = 0, hasUV2 = 0, hasUV3 = 0, hasUV4 = 0, hasWeights = 0, Animates = 0;
	int hasColor = 0;

	reader.GetParameter("XYZ", &hasPos);
	reader.GetParameter("Normals", &hasNormal);
	reader.GetParameter("UV1",  &hasUV1);
	reader.GetParameter("UV2", &hasUV2);
	reader.GetParameter("UV3", &hasUV3);
	reader.GetParameter("UV4", &hasUV4);
	reader.GetParameter("Weights",  &hasWeights);
	reader.GetParameter("Animates",  &Animates);

	reader.ReadParameterBlock2("Materials");
	int numMaterials;
	reader.GetParameter("NumberOfMaterials",&numMaterials);

	vector<string> materialNames;

	for (int i = 0; i < numMaterials; i++)
	{
		char materialBlock[64];
		char matrialName[64];
		sprintf_s(materialBlock, "Material - %i", i);
		reader.ReadParameterBlock2(materialBlock);
		reader.GetParameter("MaterialName", matrialName);
		materialNames.push_back(matrialName);

	}


	reader.ReadDataBlock2("Object Hierarchy");
	int numBones = reader.currentBlockDataLineValues.size();

	modelBone_t* bones = (numBones > 0) ? rapi->Noesis_AllocBones(numBones) : NULL;

		
	
	for (int i = 0; i < numBones; i++)
	{
		modelBone_t* dstBone = bones + i;

		RichMat43 mat;			
		float m[12];
		for (int j = 0; j < 12; j++)
		{				
			reader.GetData(i, j + 2, &m[j]);
			//reader.GetData(i, j + 2, (float*)&dstBone->mat+j);
			//reader.GetData(i, j + 2, (float*)&mat + j);
		}
		mat.m.x1[0] = m[0];
		mat.m.x1[1] = m[1];
		mat.m.x1[2] = m[2];

		mat.m.x2[0] = m[3];
		mat.m.x2[1] = m[4];
		mat.m.x2[2] = m[5];

		mat.m.x3[0] = m[6];
		mat.m.x3[1] = m[7];
		mat.m.x3[2] = m[8];

		mat.m.o[0] = m[9];
		mat.m.o[1] = m[10];
		mat.m.o[2] = m[11];

		//mat = *(RichMat43*)m;
			
		dstBone->mat = mat.m;

		reader.GetData(i, 0, dstBone->name);
		dstBone->index = i;
		char parentName[128];
		reader.GetData(i, 1, parentName);
		char temp[128];

		int parentID = -1;
		
		for (int j = 0; j < numBones; j++)
		{
				
			reader.GetData(j, 0, temp);
			if (strcmp(parentName,temp) == 0)
			{
				parentID = j;
				break;
			}
			
		}
		dstBone->eData.parent = (parentID >= 0) ? bones + parentID : NULL;
		

	}

	if (numBones)
	{
		rapi->rpgMultiplyBones(bones, numBones);
		rapi->rpgSetExData_Bones(bones, numBones);
	}
	
	

	reader.ReadParameterBlock2("LOD Information");
	int NumberOfLOD = 0;
	reader.GetParameter("NumberOfLOD", &NumberOfLOD);
	
	char tempString[256];
		
	CArrayList<noesisModel_t*> models;
	for (int lodIndex = 0; lodIndex < NumberOfLOD; lodIndex++)
	{
		sprintf_s(tempString, "LOD %i", lodIndex);
		reader.ReadParameterBlock2(tempString);
		int NumberOfSurfaces = 0;
		reader.GetParameter("NumberOfSurfaces", &NumberOfSurfaces);
		for (int meshIndex = 0; meshIndex < NumberOfSurfaces; meshIndex++)
		{
			sprintf_s(tempString, "LOD %i - Surface %i", lodIndex,meshIndex);
			reader.ReadParameterBlock2(tempString);
			int numVerts, numFace,materialID;
			reader.GetParameter("NumberOfVertices", &numVerts);
			reader.GetParameter("NumberOfFaces", &numFace);
			reader.GetParameter("Material#0", &materialID);

			sprintf_s(tempString, "LOD %i - Surface %i - Vertices", lodIndex, meshIndex);
			reader.ReadDataBlock2(tempString);
			RichVec3 vertex, normal;
			RichVec2 uv1, uv2, uv3, uv4;
			float cr = 1.0, cg = 1.0, cb = 1.0, ca = 1.0;
			BYTE* vertBuffer = hasPos ? (new BYTE[sizeof(RichVec3) * numVerts]) : NULL;
			BYTE* normalBuffer = hasNormal ? (new BYTE[sizeof(RichVec3) * numVerts]) : NULL;
			BYTE* uv1Buffer = hasUV1 ? (new BYTE[sizeof(RichVec2) * numVerts]) : NULL;
			BYTE* uv2Buffer = hasUV2 ? (new BYTE[sizeof(RichVec2) * numVerts]) : NULL;
			BYTE* uv3Buffer = hasUV3 ? (new BYTE[sizeof(RichVec2) * numVerts]) : NULL;
			BYTE* uv4Buffer = hasUV4 ? (new BYTE[sizeof(RichVec2) * numVerts]) : NULL;
			BYTE* colorBuffer = new BYTE[4 * numVerts];
			hasColor = 0;
			for (int i = 0; i < numVerts; i++)
			{
				if (hasPos)
				{
					reader.GetData(i, 0, &vertex.v[0]);
					reader.GetData(i, 1, &vertex.v[1]);
					reader.GetData(i, 2, &vertex.v[2]);
					*(RichVec3*)(vertBuffer + i * 12) = vertex;
				}

				if (hasNormal)
				{
					reader.GetData(i, 3, &normal.v[0]);
					reader.GetData(i, 4, &normal.v[1]);
					reader.GetData(i, 5, &normal.v[2]);
					*(RichVec3*)(normalBuffer + i * 12) = normal;
				}
				if (hasUV1)
				{
					reader.GetData(i, 6, &uv1.v[0]);
					reader.GetData(i, 7, &uv1.v[1]);
					*(RichVec2*)(uv1Buffer + i * 8) = uv1;
				}
				if (hasUV2)
				{
					reader.GetData(i, 9, &uv2.v[0]);
					reader.GetData(i, 10, &uv2.v[1]);
					*(RichVec2*)(uv2Buffer + i * 8) = uv2;
				}
				if (hasUV3)
				{
					reader.GetData(i, 12, &uv3.v[0]);
					reader.GetData(i, 13, &uv3.v[1]);
					*(RichVec2*)(uv3Buffer + i * 8) = uv3;
				}
				if (hasUV4)
				{
					reader.GetData(i, 15, &uv4.v[0]);
					reader.GetData(i, 16, &uv4.v[1]);
					*(RichVec2*)(uv4Buffer + i * 8) = uv4;
				}
				if (reader.currentBlockDataLineValues[i].size() == 22)
				{
					reader.GetData(i, 18, &cr);
					reader.GetData(i, 19, &cg);
					reader.GetData(i, 20, &cb);
					reader.GetData(i, 21, &ca);
				}
				else if (reader.currentBlockDataLineValues[i].size() == 16)
				{
					reader.GetData(i, 12, &cr);
					reader.GetData(i, 13, &cg);
					reader.GetData(i, 14, &cb);
					reader.GetData(i, 15, &ca);
				}
				colorBuffer[i * 4] = (BYTE)(cr * 255.0);
				colorBuffer[i * 4 + 1] = (BYTE)(cg * 255.0);
				colorBuffer[i * 4 + 2] = (BYTE)(cb * 255.0);
				colorBuffer[i * 4 + 3] = (BYTE)(ca * 255.0);
				if (hasColor == 0)
				{
					if (cr != 1.0f) hasColor = 1;
					if (cg != 1.0f) hasColor = 1;
					if (cb != 1.0f) hasColor = 1;
					if (ca != 1.0f) hasColor = 1;
				}

			}

			BYTE* boneIDBuffer = hasWeights ? (new BYTE[4 * numVerts]) : NULL;
			float* weightsBuffer = hasWeights ? (new float[4 * numVerts]) : NULL;

			if (hasWeights)
			{
				sprintf_s(tempString, "LOD %i - Surface %i - Skinning", lodIndex, meshIndex);
				reader.ReadDataBlock2(tempString);

				unsigned int boneID1, boneID2, boneID3, boneID4;

				for (int i = 0; i < numVerts; i++)
				{
					reader.GetData(i, 0, &boneID1);
					reader.GetData(i, 1, &boneID2);
					reader.GetData(i, 2, &boneID3);
					reader.GetData(i, 3, &boneID4);
					boneIDBuffer[i * 4] = boneID1;
					boneIDBuffer[i * 4 + 1] = boneID2;
					boneIDBuffer[i * 4 + 2] = boneID3;
					boneIDBuffer[i * 4 + 3] = boneID4;
					reader.GetData(i, 4, &weightsBuffer[i * 4]);
					reader.GetData(i, 5, &weightsBuffer[i * 4 + 1]);
					reader.GetData(i, 6, &weightsBuffer[i * 4 + 2]);
					reader.GetData(i, 7, &weightsBuffer[i * 4 + 3]);
				}
			}


			for (int i = 0; i < numVerts; i++)
			{
				if (hasPos) rapi->rpgBindPositionBuffer(vertBuffer, RPGEODATA_FLOAT, 12);
				if (hasNormal) rapi->rpgBindNormalBuffer(normalBuffer, RPGEODATA_FLOAT, 12);
				if (hasUV1) rapi->rpgBindUV1Buffer(uv1Buffer, RPGEODATA_FLOAT, 8);
				if (hasUV2) rapi->rpgBindUV2Buffer(uv2Buffer, RPGEODATA_FLOAT, 8);
				if (hasUV3) rapi->rpgBindUVXBuffer(uv3Buffer, RPGEODATA_FLOAT, 8, 2, 2);
				if (hasUV4) rapi->rpgBindUVXBuffer(uv4Buffer, RPGEODATA_FLOAT, 8, 3, 2);
				if (hasWeights) rapi->rpgBindBoneIndexBuffer(boneIDBuffer, RPGEODATA_UBYTE, 4, 4);
				if (hasWeights) rapi->rpgBindBoneWeightBuffer(weightsBuffer, RPGEODATA_FLOAT, 16, 4);
			}

			sprintf_s(tempString, "LOD %i - Surface %i - Faces", lodIndex, meshIndex);
			reader.ReadDataBlock2(tempString);

			unsigned short* faceBuffer = new unsigned short[numFace * 3];
			unsigned int f1, f2, f3;
			for (int i = 0; i < numFace; i++)
			{
				reader.GetData(i, 0, &f1);
				reader.GetData(i, 1, &f2);
				reader.GetData(i, 2, &f3);
				faceBuffer[i * 3] = f1;
				faceBuffer[i * 3 + 1] = f2;
				faceBuffer[i * 3 + 2] = f3;
			}


			if (hasColor) rapi->rpgBindColorBufferSafe(colorBuffer, RPGEODATA_UBYTE, 4, 4, numVerts * 4);
			char meshName[128];
			sprintf_s(meshName, "LOD%d_Surf%d_%s",lodIndex, meshIndex, materialNames[materialID].c_str());
			rapi->rpgSetName(meshName);
			rapi->rpgSetMaterial((char*)materialNames[materialID].c_str());
			rapi->rpgCommitTriangles(faceBuffer, RPGEODATA_USHORT, numFace * 3, RPGEO_TRIANGLE, false);
			rapi->rpgClearBufferBinds();
		}
		noesisModel_t* mdl = rapi->rpgConstructModel();
		if (mdl)
		{
			models.Append(mdl);
		}
		rapi->rpgReset();

	}

	numMdl = models.Num();
	noesisModel_t* mdls = rapi->Noesis_ModelsFromList(models, numMdl);

	rapi->rpgDestroyContext(pgctx);
	return mdls;
	

}