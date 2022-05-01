#include "pch.h"
#include <vector>
#include "CarsTypes.h"
#include <fstream>
using namespace std;

float getData(RichBitStream& bs, BYTE dataType, float factor);
int getAttrData(RichBitStream& bs, BYTE attrType);

noesisModel_t* Model_GCG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi)
{


	void* pgctx = rapi->rpgCreateContext();



	RichBitStream bs = RichBitStream(fileBuffer, bufferLen);
	bs.SetFlags(BITSTREAMFL_BIGENDIAN);

	bs.SetOffset(0);

	int magic = bs.ReadInt();
	int version = bs.ReadInt();
	int numBones = bs.ReadInt();
	xngBone_t* srcBones = (numBones > 0) ? (xngBone_t*)(fileBuffer + 12) : NULL;
	modelBone_t* bones = NULL;

	//convert bones
	if (srcBones)
	{
		bones = rapi->Noesis_AllocBones(numBones);
		for (int i = 0; i < numBones; i++)
		{
			xngBone_t* srcBone = srcBones + i;
			modelBone_t* dstBone = bones + i;
			srcBone->mat.ChangeEndian();
			dstBone->mat = srcBone->mat.ToMat43().m;
			dstBone->index = i;
			int parentID = (srcBone->parentIdx >> 24) | (((srcBone->parentIdx >> 8)) & 0xff00);
			dstBone->eData.parent = (parentID >= 0) ? bones + parentID : NULL;
			memcpy(dstBone->name, srcBone->name, sizeof(dstBone->name) - 1);
			dstBone->name[sizeof(dstBone->name) - 1] = 0;
			//dstBone->userIndex = i;
		}
		rapi->rpgMultiplyBones(bones, numBones);
		rapi->rpgSetExData_Bones(bones, numBones);

	}
	bs.SetOffset(12 + numBones * 224);
	int numNames = bs.ReadInt();
	int nameOfs = bs.GetOffset();
	xngMeshName_t* srcMeshNames = (numNames > 0) ? (xngMeshName_t*)(fileBuffer + nameOfs) : NULL;
	CArrayList<char*> meshNames;
	if (srcMeshNames)
	{
		for (int i = 0; i < numNames; i++)
		{
			xngMeshName_s* name = srcMeshNames + i;
			meshNames.Append(name->meshName);
		}
	}
	bs.SetOffset(nameOfs + numNames * 64);

	BYTE numLOD = bs.ReadByte();
	BYTE SkinAnimatesFlag = bs.ReadByte();
	BYTE hasWeightsFlag = bs.ReadByte();
	BYTE unused2 = bs.ReadByte();
	

	BOOL Animates = FALSE;
	if (numBones > 1) Animates = TRUE;
	BOOL ComplexMatrixPalettes = FALSE;
	if (numBones > MAX_BONES) ComplexMatrixPalettes = TRUE;


	unsigned short numWeights = 0;
	vector<skinWeights_t> weights;

	if (hasWeightsFlag)
	{
		BYTE temp[2];
		bs.ReadBytes(temp,2);
		numWeights = SWAP16(*(unsigned short*)&temp);
		//weights = (skinWeights_t*)(fileBuffer + bs.GetOffset());
		//bs.SetOffset(bs.GetOffset() + numWeights * sizeof(skinWeights_t));

		
		for (int i = 0; i < numWeights; i++)
		{
			skinWeights_t sw;
			BYTE TempShort[2];
			bs.ReadBytes(TempShort, 2);
			short boneID = SWAP16(*(unsigned short*)&TempShort);
			sw.boneID = boneID;
			sw.weight = bs.ReadFloat();
			weights.push_back(sw);
			
		}
	}

	

	CArrayList<noesisModel_t*> models;

	for (int lodIndex = 0; lodIndex < numLOD; lodIndex++)
	{
		float AutoLODValue = bs.ReadFloat();
		int numMesh = bs.ReadInt();
		for (int meshIndex = 0; meshIndex < numMesh; meshIndex++)
		{
			int meshID = bs.ReadInt();
			BYTE vertexType = bs.ReadByte();

			int HasNormal = vertexType & GCG_HAS_NORMAL;
			int HasColor = (vertexType & GCG_HAS_NORMAL) ? 0 : 1;
			int HasUV1 = (vertexType & GCG_HAS_UV1) ? 1 : 0;
			int HasUV2 = (vertexType & GCG_HAS_UV2) ? 1 : 0;
			int HasSkin = (vertexType & GCG_HAS_SKIN) ? 1 : 0;
			int HasUnk = (vertexType & GCG_HAS_UNK) ? 1 : 0;

			BYTE vertAttrType = bs.ReadByte();	//1 = GX_DIRECT, 2 = GX_INDEX8, 3 = GX_INDEX16
			BYTE vertDataType = bs.ReadByte();	//0 = GX_U8, 1 = GX_S8, 2 = GX_U16, 3 = GX_S16, 4 = GX_F32, 5 =  GX_U32
			BYTE XYZFracBits = bs.ReadByte();	//XYZQuantizationScale
			BYTE normalAttrType;
			BYTE normalDataType;
			BYTE colorAttrType;
			BYTE colorDataType;

			if (HasNormal && HasSkin)
			{
				normalAttrType = bs.ReadByte();
				normalDataType = bs.ReadByte();
			}
			if (HasColor) {
				colorAttrType = bs.ReadByte();
				colorDataType = bs.ReadByte();
			}
			BYTE uvAttrType = bs.ReadByte();
			BYTE uvDataType = bs.ReadByte();
			BYTE TexFracBits = bs.ReadByte();

			float XYZQuantizationScale = (float)(1 << XYZFracBits); // vertex position scale
			float TextureQuantizationScale = (float)(1 << TexFracBits);// uv scale

			BYTE TempShort[2];
			bs.ReadBytes(TempShort, 2);

			unsigned short numVert = SWAP16(*(unsigned short*)&TempShort);
			BYTE vertexStride = bs.ReadByte();
			unsigned short numNormal;
			BYTE normalStride;
			unsigned short numVColor;
			BYTE colorStride;

			
			if (HasNormal && HasSkin)
			{

				bs.ReadBytes(TempShort, 2);
				numNormal = SWAP16(*(unsigned short*)&TempShort);
				normalStride = bs.ReadByte();
				
			}
			if (HasColor)
			{
				bs.ReadBytes(TempShort, 2);
				numVColor = SWAP16(*(unsigned short*)&TempShort);
				colorStride = bs.ReadByte();
			}
			bs.ReadBytes(TempShort, 2);

			unsigned short numUV = SWAP16(*(unsigned short*)&TempShort);
			BYTE uvStrider = bs.ReadByte();

			int faceChunkSize = bs.ReadInt();

			float* vertBuffer = new float[numVert * 3];
			for (int i = 0; i < numVert; i++)
			{
				vertBuffer[i * 3 + 0] = getData(bs, vertDataType, XYZQuantizationScale);
				vertBuffer[i * 3 + 1] = getData(bs, vertDataType, XYZQuantizationScale);
				vertBuffer[i * 3 + 2] = getData(bs, vertDataType, XYZQuantizationScale);
			}
			float* normalBuffer = NULL;
			if (HasNormal && HasSkin)
			{
				float normalScale = 1.0f;
				if (normalDataType == 1)
				{
					normalScale = 64.0f;
				}
				else if (normalDataType == 4)
				{
					normalScale = 1.0f;
				}
				else
				{
					bs.SetOffset(bs.GetOffset() + numNormal * normalStride);
					rapi->LogOutput("Unsupport normals data type: %d.\n", normalDataType);
				}
				normalBuffer = new float[numNormal * 3];
				for (int i = 0; i < numNormal; i++)
				{
					float x = getData(bs, normalDataType, normalScale);
					float y = getData(bs, normalDataType, normalScale);
					float z = getData(bs, normalDataType, normalScale);

					normalBuffer[i * 3 + 0] = x;
					normalBuffer[i * 3 + 1] = y;
					normalBuffer[i * 3 + 2] = z;
				}
			}
			BYTE* colorBuffer = NULL;
			if (HasColor)
			{
				if (colorDataType == 5)
				{
					colorBuffer = new BYTE[4 * numVColor];
					for (int j = 0; j < numVColor; j++)
					{
						colorBuffer[j * 4 + 0] = bs.ReadByte();	//R
						colorBuffer[j * 4 + 1] = bs.ReadByte();	//G
						colorBuffer[j * 4 + 2] = bs.ReadByte();	//B
						colorBuffer[j * 4 + 3] = bs.ReadByte();	//A					
					}
				}
				else
				{
					bs.SetOffset(bs.GetOffset() + numVert * colorStride);
					rapi->LogOutput("Unsupport vertex color data type: %d.\n", colorDataType);
				}
			}
			float* uv1Buffer = HasUV1 ? new float[numUV * 2] : NULL;
			if (HasUV1)
			{
				for (int j = 0; j < numUV; j++)
				{
					uv1Buffer[j * 2 + 0] = getData(bs, uvDataType, TextureQuantizationScale);
					uv1Buffer[j * 2 + 1] = getData(bs, uvDataType, TextureQuantizationScale);
				}
			}
			/*
			float* uv2Buffer = HasUV2 ? new float[numUV * 2] : NULL;
			if (HasUV2)
			{
				for (int j = 0; j < numUV; j++)
				{
					uv2Buffer[j * 2 + 0] = getData(bs, uvDataType, TextureQuantizationScale);
					uv2Buffer[j * 2 + 1] = getData(bs, uvDataType, TextureQuantizationScale);
				}
			}*/
			unsigned short numSkinData = 0;
			int faceChunkOffset = bs.GetOffset();

			skinBoneWeightIndex_t* skinList = NULL;
			if (HasSkin)
			{
				bs.ReadBytes(TempShort, 2);
				numSkinData = SWAP16(*(unsigned short*)&TempShort);
				faceChunkOffset = bs.GetOffset() + numSkinData * sizeof(skinBoneWeightIndex_t);
				skinList = (skinBoneWeightIndex_t*)(fileBuffer + bs.GetOffset());
				bs.SetOffset(bs.GetOffset() + numSkinData * sizeof(skinBoneWeightIndex_t));
				vector<GcgSkinBoneInfo_t*> boneInfoList;
				int *offsetList = new int[numSkinData];
				for (int j = 0; j < numSkinData; j++)
				{
					skinBoneWeightIndex_t *skinIndex = skinList + j;
					skinIndex->swap();
					bs.SetOffset(faceChunkOffset + skinIndex->matrixOffset-8);
					GcgSkinBoneInfo_t *boneInfo = (GcgSkinBoneInfo_t*)(fileBuffer + bs.GetOffset());
					if (boneInfo->matrixType == 0) bs.SetOffset(bs.GetOffset() + 56);
					if (boneInfo->matrixType == 4) bs.SetOffset(bs.GetOffset() + 44);
					offsetList[j] = bs.GetOffset();


					boneInfoList.push_back(boneInfo);
				}
				BOOL hasFace = 0;
				int maxIndex = numSkinData - 1;
				vector<short> skinPalettes;
				int skinSplitIndex = 0;

				for (int j = 0; j < numSkinData; j++) {

					skinPalettes.push_back(j);
					if (j > 0)
					{
						hasFace = boneInfoList[j]->matrixDataIDOffset < boneInfoList[j - 1]->matrixDataIDOffset;
					}
					if (hasFace || (j == maxIndex))
					{
						if (j != maxIndex) {
							bs.SetOffset(offsetList[j - 1]);
						}
						else {
							bs.SetOffset(offsetList[j]);
						}
						BYTE check = bs.ReadByte();
						if ((check & 0x98) != 0x98)
						{
							bs.SetOffset(bs.GetOffset() + 43);
						}
						else {
							bs.SetOffset(bs.GetOffset() - 1);
						}
						BYTE drawType = bs.ReadByte();//0x9F & 0x98 triangle strip
						bs.ReadBytes(TempShort, 2);
						unsigned short numFaceIndex = SWAP16(*(unsigned short*)&TempShort); // number of vertex
						
						if (drawType & GX_DRAW_TRIANGLE_STRIP)
						{

							skinSplitIndex += 1;

							float* newVertBuffer = new float[numFaceIndex * 3];
							float* newNormalBuffer = HasNormal ? new float[numFaceIndex * 3] : NULL;
							BYTE* newColorBuffer = HasColor ? new BYTE[numFaceIndex * 4] : NULL;
							float* newUV1Buffer = HasUV1 ? new float[numFaceIndex * 2] : NULL;
							//float* newUV2Buffer = HasUV2 ? new float[numFaceIndex * 2] : NULL;
							int* newBoneIDBuffer = HasSkin ? new int[numFaceIndex * 4] : NULL;
							float* newWeightsBuffer = HasSkin ? new float[numFaceIndex * 4] : NULL;
							vector<unsigned short> faceBuffer;
							
							int* specVertIDList = new int[numFaceIndex];
							int FaceDirection = 1;
							int f1 = 0, f2 = 1, f3 = 2;
							for (int f = 0; f < numFaceIndex; f++)
							{
								BYTE skinPaletteID = bs.ReadByte();
								float wgt[4] = { 0,0,0,0 };
								short bid[4] = { 0,0,0,0 };
								skinBoneWeightIndex_t* skinInfo = skinList + skinPalettes[(skinPaletteID / 3)];
								for (int w = 0; w < 4; w++)
								{
									short wid = skinInfo->weightIDs[w];
									if (wid != -1)
									{
										skinWeights_t skinWeight = weights[wid];
										wgt[w] = skinWeight.weight;
										bid[w] = skinWeight.boneID;
									}
								}
								for (int w = 0; w < 4; w++)
								{
									newBoneIDBuffer[f * 4 + w] = bid[w];
									newWeightsBuffer[f * 4 + w] = wgt[w];
								}

								RichVec3 vertex =  RichVec3();
								int vertexID = getAttrData(bs, vertAttrType);
								for (int v = 0; v < 3; v++)
								{
									newVertBuffer[f * 3 + v] = vertBuffer[vertexID * 3 + v];
									vertex.v[v] = vertBuffer[vertexID * 3 + v];
								}
								specVertIDList[f] = vertexID;

								if (HasNormal) {
									int normalID = getAttrData(bs, normalAttrType);

									for (int v = 0; v < 3; v++)
									{
										newNormalBuffer[f * 3 + v] = normalBuffer[normalID * 3 + v];
									}

								}
								else {
									int colorID = getAttrData(bs, colorAttrType);
									for (int v = 0; v < 4; v++)
									{
										newColorBuffer[f * 4 + v] = colorBuffer[colorID * 4 + v];
									}
								}
								if (HasUV1) {
									int uv1ID = getAttrData(bs, uvAttrType);
									for (int v = 0; v < 2; v++)
									{
										newUV1Buffer[f * 2 + v] = uv1Buffer[uv1ID * 2 + v];
									}
								}
								if (HasUV2) {
									int uv2ID = getAttrData(bs, uvAttrType);
								}
								if (f > 1)
								{
									f3 = f;
									int v1 = specVertIDList[f];
									int v2 = specVertIDList[f - 2];
									int v3 = specVertIDList[f - 1];
									if ((v1 != v2) && (v2 != v3) && (v1 != v3))
									{
										if (FaceDirection > 0)
										{
											faceBuffer.push_back(f1);
											faceBuffer.push_back(f2);
											faceBuffer.push_back(f3);
										}
										else
										{
											faceBuffer.push_back(f2);
											faceBuffer.push_back(f1);
											faceBuffer.push_back(f3);
										}
									}
									f1 = f2;
									f2 = f3;
									FaceDirection *= -1;
								}
							}

							rapi->rpgBindPositionBufferSafe(newVertBuffer, RPGEODATA_FLOAT, 12,numFaceIndex * 3 *sizeof(float));
							if (HasNormal) rapi->rpgBindNormalBufferSafe(newNormalBuffer, RPGEODATA_FLOAT, 12, numFaceIndex * 3 * sizeof(float));
							if (HasUV1) rapi->rpgBindUV1BufferSafe(newUV1Buffer, RPGEODATA_FLOAT, 8, numFaceIndex * 2 * sizeof(float));
							//if (HasUV2) rapi->rpgBindUV2BufferSafe(newUV2Buffer, RPGEODATA_FLOAT, 8, numFaceIndex * 2 * sizeof(float));
							if (HasSkin) rapi->rpgBindBoneIndexBufferSafe(newBoneIDBuffer, RPGEODATA_INT, 16, 4, numFaceIndex * 4 * sizeof(int));
							if (HasSkin) rapi->rpgBindBoneWeightBufferSafe(newWeightsBuffer, RPGEODATA_FLOAT, 16, 4, numFaceIndex * 4 * sizeof(float));
							if (HasColor) rapi->rpgBindColorBufferSafe(newColorBuffer, RPGEODATA_UBYTE, 4, 4, numFaceIndex * 4);
							char meshName[64];
							//sprintf_s(meshName, "LOD%i_Surf%i_%s_%d", lodIndex, meshID, meshNames[meshID], skinSplitIndex);
							sprintf_s(meshName, "LOD%i_Surf%i_%s", lodIndex, meshID, meshNames[meshID]);
							rapi->rpgSetName(meshName);
							rapi->rpgSetMaterial(meshNames[meshID]);
							rapi->rpgCommitTriangles(faceBuffer.data(), RPGEODATA_USHORT, faceBuffer.size(), RPGEO_TRIANGLE, true);
							rapi->rpgClearBufferBinds();


						}
						
						skinPalettes.clear();
						skinPalettes.push_back(j);
					}
				}
			}

			if (!HasSkin)
			{
				
				BYTE drawType = bs.ReadByte();//0x98 triangle strip
				bs.ReadBytes(TempShort, 2);
				unsigned short numFaceIndex = SWAP16(*(unsigned short*)&TempShort);

				if (drawType & GX_DRAW_TRIANGLE_STRIP)
				{
					float* newVertBuffer = new float[numFaceIndex * 3];
					float* newNormalBuffer = HasNormal ? new float[numFaceIndex * 3] : NULL;
					BYTE* newColorBuffer = HasColor ? new BYTE[numFaceIndex * 4] : NULL;
					float* newUV1Buffer = HasUV1 ? new float[numFaceIndex * 2] : NULL;
					//float* newUV2Buffer = HasUV2 ? new float[numFaceIndex * 2] : NULL;

					vector<unsigned short> faceBuffer;

					int* specVertIDList = new int[numFaceIndex];
					int FaceDirection = 1;
					int f1 = 0, f2 = 1, f3 = 2;
					for (int f = 0; f < numFaceIndex; f++)
					{

						int vertexID = getAttrData(bs, vertAttrType);
						for (int v = 0; v < 3; v++)
						{
							newVertBuffer[f * 3 + v] = vertBuffer[vertexID * 3 + v];
						}
						specVertIDList[f] = vertexID;

						if (HasNormal) {
							normalAttrType = 2;							
							int normalID = getAttrData(bs, normalAttrType);
							for (int v = 0; v < 3; v++)
							{
								newNormalBuffer[f * 3 + v] = 0.0f;
							}
							
						}
						else {
							int colorID = getAttrData(bs, colorAttrType);
							for (int v = 0; v < 4; v++)
							{
								newColorBuffer[f * 4 + v] = colorBuffer[colorID * 4 + v];
							}
						}
						if (HasUV1) {
							int uv1ID = getAttrData(bs, uvAttrType);
							for (int v = 0; v < 2; v++)
							{
								newUV1Buffer[f * 2 + v] = uv1Buffer[uv1ID * 2 + v];
							}
						}
						if (HasUV2) {
							int uv2ID = getAttrData(bs, uvAttrType);
						}
						if (f > 1)
						{
							f3 = f;
							int v1 = specVertIDList[f];
							int v2 = specVertIDList[f - 2];
							int v3 = specVertIDList[f - 1];
							if ((v1 != v2) && (v2 != v3) && (v1 != v3))
							{
							if (FaceDirection > 0)
							{
								faceBuffer.push_back(f1);
								faceBuffer.push_back(f2);
								faceBuffer.push_back(f3);
							}
							else
							{
								faceBuffer.push_back(f2);
								faceBuffer.push_back(f1);
								faceBuffer.push_back(f3);
							}
							}
							f1 = f2;
							f2 = f3;
							FaceDirection *= -1;
						}
					}

					rapi->rpgBindPositionBufferSafe(newVertBuffer, RPGEODATA_FLOAT, 12, numFaceIndex * 3 * sizeof(float));
					//if (HasNormal)
					//{
						//rapi->rpgBindNormalBufferSafe(newNormalBuffer, RPGEODATA_FLOAT, 12, numFaceIndex * 3 * sizeof(float));
					//}
						
					if (HasUV1) rapi->rpgBindUV1BufferSafe(newUV1Buffer, RPGEODATA_FLOAT, 8, numFaceIndex * 2 * sizeof(float));
					//if (HasUV2) rapi->rpgBindUV2BufferSafe(newUV2Buffer, RPGEODATA_FLOAT, 8, numFaceIndex * 2 * sizeof(float));					
					if (HasColor) rapi->rpgBindColorBufferSafe(newColorBuffer, RPGEODATA_UBYTE, 4, 4, numFaceIndex * 4);
					char meshName[64];					
					sprintf_s(meshName, "LOD%i_Surf%i_%s", lodIndex, meshID, meshNames[meshID]);
					rapi->rpgSetName(meshName);
					rapi->rpgSetMaterial(meshNames[meshID]);
					rapi->rpgCommitTriangles(faceBuffer.data(), RPGEODATA_USHORT, faceBuffer.size(), RPGEO_TRIANGLE, true);
					rapi->rpgClearBufferBinds();

				}
			}
			
			int faceChunkEndOffset = faceChunkOffset + faceChunkSize; 
			bs.SetOffset(faceChunkEndOffset);

			delete[] vertBuffer;
			delete[] normalBuffer;
			delete[] colorBuffer;
			delete[] uv1Buffer;
			//delete[] uv2Buffer;
			

		}
		
		//rapi->rpgOptimize();
		//smNrmParm_t *nrmParameter = new smNrmParm_t[1];		
		//nrmParameter[0].flags = SMNRMPARM_PLANESPACEUVS;
		//nrmParameter[0].flags = SMNRMPARM_PLANESPACEUVS_IFNONE;
		//rapi->rpgSmoothNormals(nrmParameter);
		noesisModel_t* mdl = rapi->rpgConstructModel();
		if (mdl) models.Append(mdl);
		rapi->rpgReset();
	}
	
	
	numMdl = models.Num();
	noesisModel_t* mdls = rapi->Noesis_ModelsFromList(models, numMdl);
	rapi->rpgDestroyContext(pgctx);
	weights.~vector();
	models.~CArrayList();
	bs.~RichBitStream();

	
	return mdls;
}
//binary stream must be a reference(&) not a copy
float getData(RichBitStream& bs, BYTE dataType, float factor)
{
	float value{};
	BYTE TempShort[2];
	switch (dataType)
	{
		case 0: value = bs.ReadByte() / factor; break;
		case 1: value = (char)(bs.ReadByte()) / factor; break;
		case 2: bs.ReadBytes(TempShort, 2);
				value = SWAP16(*(unsigned short*)&TempShort) / factor;
			break;
		case 3:	bs.ReadBytes(TempShort, 2);
				value = (short)SWAP16(*(unsigned short*)&TempShort) / factor;
			break;
		case 4: value = bs.ReadFloat() / factor;
			break;
		default:
		break;
	}
	return value;
}

//binary stream must be a reference(&) not a copy
int getAttrData(RichBitStream& bs, BYTE attrType)
{
	int value{};
	BYTE TempShort[2];
	switch (attrType)
	{
	case 2: value = bs.ReadByte(); break;
	case 3: bs.ReadBytes(TempShort, 2);
		value = SWAP16(*(unsigned short*)&TempShort);
		break;
	default:
		break;
	}
	return value;
}