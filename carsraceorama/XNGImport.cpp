#include "pch.h"
#include <vector>
#include "CarsTypes.h"
using namespace std;

noesisModel_t* Model_XNG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi)
{
	void* pgctx = rapi->rpgCreateContext();

	xngHdr_t* hdr = (xngHdr_t*)fileBuffer;

	bool isBig = false;
	//rapi->rpgSetEndian(true);
	
	if (hdr->version == 0x5000000)
	{
		isBig = true;
	}

	//rapi->rpgSetOption(RPGOPT_BIGENDIAN, isBig);

	RichBitStream bs = RichBitStream(fileBuffer, bufferLen);
	if (isBig)
	{
		bs.SetFlags(BITSTREAMFL_BIGENDIAN);
	}


	

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
			if (isBig) srcBone->mat.ChangeEndian();
			
			dstBone->mat = srcBone->mat.ToMat43().m;
			dstBone->index = i;
			//int parentID = (srcBone->parentIdx >> 24) | (((srcBone->parentIdx >> 8)) & 0xff00);
			int parentID = srcBone->parentIdx;
			if (isBig) LITTLE_BIG_SWAP(parentID);
			dstBone->eData.parent = (parentID >= 0) ? bones + parentID : NULL;
			memcpy(dstBone->name, srcBone->name, sizeof(dstBone->name) - 1);
			dstBone->name[sizeof(dstBone->name) - 1] = 0;
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
	BYTE unused1 = bs.ReadByte();
	BYTE unused2 = bs.ReadByte();
	

	BOOL Animates = FALSE;
	if (numBones > 1) Animates = TRUE;
	BOOL ComplexMatrixPalettes = FALSE;
	if (numBones > MAX_BONES) ComplexMatrixPalettes = TRUE;

	BOOL hasWeights = (SkinAnimatesFlag & 2) == 2;




	//just skip all of lod data and get the skin bone matrix palettes data list
	int numSurface = 0;
	int meshTableOfs = bs.GetOffset();
	for (int lodIndex = 0; lodIndex < numLOD; lodIndex++)
	{
		float AutoLODValue = bs.ReadFloat();
		int numMeshes = bs.ReadInt();
		numSurface += numMeshes;
		for (int meshIndex = 0; meshIndex < numMeshes; meshIndex++)
		{
			int meshID = bs.ReadInt();
			int vertFormat = bs.ReadInt();

			bool hasPos = (vertFormat & 1) == 1;
			bool hasNormal = (vertFormat & 2) == 2;
			bool hasColor = (vertFormat & 8) == 8;
			bool hasUV = (vertFormat & 4) == 4;
			bool hasUV2 = (vertFormat & 0x10) == 0x10;
			bool hasTangent = (vertFormat & 0x20) == 0x20;
			bool hasSingleBone = (vertFormat & 0x40) == 0x40;//HAS_BONE_INDEX
			bool hasDeltaCpu = (vertFormat & 0x100) == 0x100;
			bool hasSkin = (vertFormat & 0x1000) == 0x1000; //HAS_WEIGHTS
			bool hasComp = (vertFormat & 0x2000) == 0x2000;
			bool hasUV3 = (vertFormat & 0x4000) == 0x4000;
			bool hasUV4 = (vertFormat & 0x8000) == 0x8000;

			//DXG XBOX
			char vertexFormats[16] = {
										 D3DVSDT_FLOAT3,
										 D3DVSDT_FLOAT3,
										 D3DVSDT_D3DCOLOR,
										 D3DVSDT_FLOAT2,
										 D3DVSDT_FLOAT1,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_FLOAT4,
										 D3DVSDT_FLOAT4,
										 D3DVSDT_FLOAT2,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE
			};

			if (hasComp)
			{
				bs.SetOffset(bs.GetOffset() + 32); 
			}

			BYTE numUV = bs.ReadByte();
			BYTE compressed = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);
			if (!memcmp(hdr->id, "dxg", 4) && hasComp)
			{
				bs.ReadBytes(vertexFormats, 16);
			}

			unsigned short tempUSHORT[2];

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*) & tempUSHORT);
			int numVerts = *(USHORT*)tempUSHORT;

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numFaceIndex = *(USHORT*)tempUSHORT;


			bs.SetOffset(bs.GetOffset() + numFaceIndex * 2);
			if (!memcmp(hdr->id,"xng",4) || !memcmp(hdr->id,"p3g",4))
			{
				if ((numFaceIndex % 2) == 1) bs.SetOffset(bs.GetOffset() + 2); // skip unused flag
				if (hasPos) bs.SetOffset(bs.GetOffset() + numVerts * 12);
				if (hasNormal) bs.SetOffset(bs.GetOffset() + numVerts * 12);
				if (hasColor) bs.SetOffset(bs.GetOffset() + numVerts * 4);
				if (hasUV)  bs.SetOffset(bs.GetOffset() + numVerts * 8);
				if (hasSingleBone) bs.SetOffset(bs.GetOffset() + numVerts * 4);
				if (hasSkin) bs.SetOffset(bs.GetOffset() + numVerts * 32);
				if (hasUV2) bs.SetOffset(bs.GetOffset() + numVerts * 8);
				if (hasUV3) bs.SetOffset(bs.GetOffset() + numVerts * 8);
				if (hasUV4) bs.SetOffset(bs.GetOffset() + numVerts * 8);
				if (hasTangent) bs.SetOffset(bs.GetOffset() + numVerts * 12);
			}
			if (!memcmp(hdr->id, "dxg", 4))
			{
				if (hasPos) {
					switch (vertexFormats[0])
					{
						case D3DVSDT_FLOAT3:bs.SetOffset(bs.GetOffset() + numVerts * 12); break;
						case D3DVSDT_SHORT3:bs.SetOffset(bs.GetOffset() + numVerts * 6); break;
						case D3DVSDT_PBYTE3:bs.SetOffset(bs.GetOffset() + numVerts * 3); break;
					}
				}
				if (hasNormal)
				{
					switch (vertexFormats[1])
					{
					case D3DVSDT_FLOAT3:bs.SetOffset(bs.GetOffset() + numVerts * 12); break;
					case D3DVSDT_SHORT3:bs.SetOffset(bs.GetOffset() + numVerts * 6); break;
					case D3DVSDT_PBYTE3:bs.SetOffset(bs.GetOffset() + numVerts * 3); break;
					}
				}
				if (hasColor) bs.SetOffset(bs.GetOffset() + numVerts * 4);
				if (hasUV)
				{
					switch (vertexFormats[3])
					{
					case D3DVSDT_FLOAT2:bs.SetOffset(bs.GetOffset() + numVerts * 8); break;
					case D3DVSDT_SHORT2:bs.SetOffset(bs.GetOffset() + numVerts * 4); break;
					case D3DVSDT_PBYTE2:bs.SetOffset(bs.GetOffset() + numVerts * 2); break;
					}
				}
				if (hasUV2) bs.SetOffset(bs.GetOffset() + numVerts * 8);
				if (hasSingleBone) bs.SetOffset(bs.GetOffset() + numVerts * 4);
				if (hasSkin) bs.SetOffset(bs.GetOffset() + numVerts * 32);
			}



		}
	}
	
	vector<vector<BYTE>> boneMaps;
	vector<BYTE> boneMapIndices;
	if (Animates)
	{
		int info = bs.ReadInt();
		short numBoneMaps = (short)(info & 0xffff);
		if (numBoneMaps > 0)
		{
			for (int id = 0; id < numBoneMaps; id++)
			{
				vector<BYTE> boneMap;
				int numBoneIDs = bs.ReadInt();
				for (int j = 0; j < numBoneIDs; j++)
				{
					BYTE boneID = bs.ReadByte();
					boneMap.push_back(boneID);
				}
				boneMaps.push_back(boneMap);
			}


			if ((numSurface > 1) && ComplexMatrixPalettes)//ComplexMatrixPalettes
			{
				for (int id = 0; id < numSurface; id++)
				{
					boneMapIndices.push_back(bs.ReadByte());
				}
			}
			else//Simple palettes
			{
				boneMapIndices.push_back(0);
			}
		}
	}




	int bidOfs = 0;
	bs.SetOffset(meshTableOfs);
	CArrayList<noesisModel_t*> models;

	for (int lodIndex = 0; lodIndex < numLOD; lodIndex++)
	{
		//rapi->rpgSetEndian(true);//*very important because use rapi.rpgReset() will diable this option.
		if (isBig) rapi->rpgSetOption(RPGOPT_BIGENDIAN, isBig);
		float AutoLODValue = bs.ReadFloat();
		int numMeshes = bs.ReadInt();
		for (int meshIndex = 0; meshIndex < numMeshes; meshIndex++)
		{

			int meshID = bs.ReadInt();
			int vertFormat = bs.ReadInt();

			bool hasPos = (vertFormat & 1) == 1;
			bool hasNormal = (vertFormat & 2) == 2;
			bool hasColor = (vertFormat & 8) == 8;
			bool hasUV = (vertFormat & 4) == 4;
			bool hasUV2 = (vertFormat & 0x10) == 0x10;
			bool hasTangent = (vertFormat & 0x20) == 0x20;
			bool hasSingleBone = (vertFormat & 0x40) == 0x40;//HAS_BONE_INDEX
			bool hasDeltaCpu = (vertFormat & 0x100) == 0x100;
			bool hasSkin = (vertFormat & 0x1000) == 0x1000; //HAS_WEIGHTS
			bool hasComp = (vertFormat & 0x2000) == 0x2000;
			bool hasUV3 = (vertFormat & 0x4000) == 0x4000;
			bool hasUV4 = (vertFormat & 0x8000) == 0x8000;

			//DXG XBOX
			char vertexFormats[16] = {
										 D3DVSDT_FLOAT3,
										 D3DVSDT_FLOAT3,
										 D3DVSDT_D3DCOLOR,
										 D3DVSDT_FLOAT2,
										 D3DVSDT_FLOAT1,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_FLOAT4,
										 D3DVSDT_FLOAT4,
										 D3DVSDT_FLOAT2,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE,
										 D3DVSDT_NONE
			};
			float vertexDecompScale = 1.0f;
			float normalDecompScale = 1.0f;
			float uvDecompScale = 1.0f;
			float uvDecompScale2 = 1.0f;
			float BoundingCubeCenterX = 0.0f;
			float BoundingCubeCenterY = 0.0f;
			float BoundingCubeCenterZ = 0.0f;
			int FixedInWorldSpace = 0;
			if (hasComp)
			{
				vertexDecompScale = bs.ReadFloat();// factor = 1.0f / this
				normalDecompScale = bs.ReadFloat();
				uvDecompScale = bs.ReadFloat();
				uvDecompScale2 = bs.ReadFloat();
				BoundingCubeCenterX = bs.ReadFloat();
				BoundingCubeCenterY = bs.ReadFloat();
				BoundingCubeCenterZ = bs.ReadFloat();
				FixedInWorldSpace = bs.ReadInt();
			}

			BYTE numUV = bs.ReadByte();
			BYTE compressed = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);

			if (!memcmp(hdr->id, "dxg", 4) && hasComp)
			{
				bs.ReadBytes(vertexFormats, 16);
			}

			unsigned short tempUSHORT[2];

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numVerts = *(USHORT*)tempUSHORT;

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numFaceIndex = *(USHORT*)tempUSHORT;



			//Face
			UINT16* faceAr = (UINT16*)(fileBuffer + bs.GetOffset());
			bs.SetOffset(bs.GetOffset() + numFaceIndex * 2);
			//unsigned short* faceBuffer = new unsigned short[numFaceIndex];

			vector<BYTE> boneMap;
			if (Animates)
			{
				int index = meshID;
				if (!ComplexMatrixPalettes)
				{
					index = 0;
				}
				for (int j = 0; j < boneMaps[boneMapIndices[index]].size(); j++)
				{
					boneMap.push_back(boneMaps[boneMapIndices[index]][j]);
				}
			}


			if (!memcmp(hdr->id, "xng", 4) || !memcmp(hdr->id, "p3g", 4))
			{
				if ((numFaceIndex % 2) == 1) bs.SetOffset(bs.GetOffset() + 2); // skip unused flag
				//Position
				float* posAr = hasPos ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				
				if (hasPos) 
				{
					bs.SetOffset(bs.GetOffset() + numVerts * 12);
					rapi->rpgBindPositionBufferSafe(posAr, RPGEODATA_FLOAT, 12, numVerts * 12);
				} 

				//Normals
				float* nrmAr = hasNormal ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasNormal) 
				{ 
					bs.SetOffset(bs.GetOffset() + numVerts * 12); 
					rapi->rpgBindNormalBufferSafe(nrmAr, RPGEODATA_FLOAT, 12, numVerts * 12);
				}

			}

			if (!memcmp(hdr->id, "dxg", 4))
			{				
				if (hasPos) {
					float* vertBuffer = hasPos ? (new float[sizeof(float) * numVerts * 3]) : NULL;
					switch (vertexFormats[0])
					{
					case D3DVSDT_FLOAT3:						
						memcpy_s(vertBuffer, numVerts * 12,(float*)(fileBuffer + bs.GetOffset()), numVerts * 12);
						bs.SetOffset(bs.GetOffset() + numVerts * 12);						
						break;
					case D3DVSDT_SHORT3:
						
						for (int i = 0; i < numVerts; i++)
						{
							short compX = *(short*)(fileBuffer + bs.GetOffset());
							short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);
							short compZ = *(short*)(fileBuffer + bs.GetOffset() + 4);
							if (isBig)
							{
								LITTLE_BIG_SWAP(compX); LITTLE_BIG_SWAP(compY); LITTLE_BIG_SWAP(compZ);
							}
							bs.SetOffset(bs.GetOffset() + 6);

							vertBuffer[i * 3 + 0] = (float)compX * vertexDecompScale + BoundingCubeCenterX;
							vertBuffer[i * 3 + 1] = (float)compY * vertexDecompScale + BoundingCubeCenterY;
							vertBuffer[i * 3 + 2] = (float)compZ * vertexDecompScale + BoundingCubeCenterZ;
						}

						break;
					case D3DVSDT_PBYTE3:
						for (int i = 0; i < numVerts; i++)
						{
							char compX = bs.ReadByte();
							char compY = bs.ReadByte();
							char compZ = bs.ReadByte();

							vertBuffer[i * 3 + 0] = (float)compX * vertexDecompScale + BoundingCubeCenterX;
							vertBuffer[i * 3 + 1] = (float)compY * vertexDecompScale + BoundingCubeCenterY;
							vertBuffer[i * 3 + 2] = (float)compZ * vertexDecompScale + BoundingCubeCenterZ;
						}
						break;
					}
					rapi->rpgBindPositionBufferSafe(vertBuffer, RPGEODATA_FLOAT, 12, numVerts * 12);
				}
				if (hasNormal)
				{
					float* normalBuffer = hasNormal ? (new float[sizeof(float) * numVerts * 3]) : NULL;
					switch (vertexFormats[1])
					{
					case D3DVSDT_FLOAT3:
						memcpy_s(normalBuffer, numVerts * 12, (float*)(fileBuffer + bs.GetOffset()), numVerts * 12);
						bs.SetOffset(bs.GetOffset() + numVerts * 12); 
						break;
					case D3DVSDT_SHORT3:
						for (int i = 0; i < numVerts; i++)
						{
							short compX = *(short*)(fileBuffer + bs.GetOffset());
							short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);
							short compZ = *(short*)(fileBuffer + bs.GetOffset() + 4);
							if (isBig)
							{
								LITTLE_BIG_SWAP(compX); LITTLE_BIG_SWAP(compY); LITTLE_BIG_SWAP(compZ);
							}
							bs.SetOffset(bs.GetOffset() + 6);

							normalBuffer[i * 3 + 0] = (float)compX * normalDecompScale;
							normalBuffer[i * 3 + 1] = (float)compY * normalDecompScale;
							normalBuffer[i * 3 + 2] = (float)compZ * normalDecompScale;
						}

						break;
					case D3DVSDT_PBYTE3:
						for (int i = 0; i < numVerts; i++)
						{
							char compX = bs.ReadByte();
							char compY = bs.ReadByte();
							char compZ = bs.ReadByte();

							normalBuffer[i * 3 + 0] = (float)compX * normalDecompScale;
							normalBuffer[i * 3 + 1] = (float)compY * normalDecompScale;
							normalBuffer[i * 3 + 2] = (float)compZ * normalDecompScale;
						}
						break;
					}
					rapi->rpgBindNormalBufferSafe(normalBuffer, RPGEODATA_FLOAT, 12, numVerts * 12);
				}
			}

			//Colors
			//BYTE* srcClrAr = hasColor ? (BYTE*)(fileBuffer + bs.GetOffset()) : NULL;
			//if (hasColor) bs.SetOffset(bs.GetOffset() + numVerts * 4);
			RichBitStream colorAr;
			if (hasColor)
			{

				for (int j = 0; j < numVerts; j++)
				{
					if (!memcmp(hdr->id, "xng", 4) || !memcmp(hdr->id, "p3g", 4))
					{
						BYTE A = bs.ReadByte();
						BYTE R = bs.ReadByte();
						BYTE G = bs.ReadByte();
						BYTE B = bs.ReadByte();
						colorAr.WriteByte(R); colorAr.WriteByte(G); colorAr.WriteByte(B); colorAr.WriteByte(A);
					}
					if (!memcmp(hdr->id, "dxg", 4))
					{
						BYTE B = bs.ReadByte();
						BYTE G = bs.ReadByte();
						BYTE R = bs.ReadByte();
						BYTE A = bs.ReadByte();
						colorAr.WriteByte(R); colorAr.WriteByte(G); colorAr.WriteByte(B); colorAr.WriteByte(A);
					}

				}
				rapi->rpgBindColorBufferSafe(colorAr.GetBuffer(), RPGEODATA_UBYTE, 4, 4, numVerts * 4);
			}
			if (!memcmp(hdr->id, "xng", 4) || !memcmp(hdr->id, "p3g", 4))
			{
				//UV1
				float* uvAr = (hasUV) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasUV)  bs.SetOffset(bs.GetOffset() + numVerts * 8);
				rapi->rpgBindUV1BufferSafe(uvAr, RPGEODATA_FLOAT, 8, numVerts * 8);
			}

			if (!memcmp(hdr->id, "dxg", 4))
			{
				//UV1
				if (hasUV)
				{
					float* uvBuffer = hasUV ? (new float[sizeof(float) * numVerts * 2]) : NULL;
					switch (vertexFormats[3])
					{
					case D3DVSDT_FLOAT2:
						memcpy_s(uvBuffer, numVerts * 8, (float*)(fileBuffer + bs.GetOffset()), numVerts * 8);
						bs.SetOffset(bs.GetOffset() + numVerts * 8); 
						break;
					case D3DVSDT_SHORT2:
						for (int i = 0; i < numVerts; i++)
						{
							short compX = *(short*)(fileBuffer + bs.GetOffset());
							short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);
							if (isBig)
							{
								LITTLE_BIG_SWAP(compX); LITTLE_BIG_SWAP(compY);
							}
							bs.SetOffset(bs.GetOffset() + 4);

							uvBuffer[i * 2 + 0] = (float)compX * uvDecompScale;
							uvBuffer[i * 2 + 1] = (float)compY * uvDecompScale;
						}
						break;
					case D3DVSDT_PBYTE2:
						for (int i = 0; i < numVerts; i++)
						{
							char compX = bs.ReadByte();
							char compY = bs.ReadByte();
							uvBuffer[i * 2 + 0] = (float)compX * uvDecompScale;
							uvBuffer[i * 2 + 1] = (float)compY * uvDecompScale;
						}
						break;
					}
					rapi->rpgBindUV1BufferSafe(uvBuffer, RPGEODATA_FLOAT, 8, numVerts * 8);
				}


				//UV2
				float* uv2Ar = (hasUV2) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasUV2)
				{
					bs.SetOffset(bs.GetOffset() + numVerts * 8);
					rapi->rpgBindUV2BufferSafe(uv2Ar, RPGEODATA_FLOAT, 8, numVerts * 8);
				}
			}

			//Single Bone 
			float* singleBones = (hasSingleBone) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			RichBitStream sbid;
			RichBitStream swgt;
			if (isBig) swgt.SetFlags(BITSTREAMFL_BIGENDIAN);
			if ((hasSingleBone) && (!hasSkin))
			{
				for (int j = 0; j < numVerts; j++)
				{
					if (Animates)
					{
						BYTE bone0 = boneMap[bs.ReadFloat()];
						sbid.WriteByte(bone0);
						swgt.WriteFloat(1.0f);
					}

					else {
						//NO Animates NO Skin - Have parent none static mesh
						BYTE bone0 = BYTE(bs.ReadFloat());
						sbid.WriteByte(bone0);
						swgt.WriteFloat(1.0f);
					}
				}
				rapi->rpgBindBoneIndexBufferSafe(sbid.GetBuffer(), RPGEODATA_UBYTE, 1, 1, numVerts);
				rapi->rpgBindBoneWeightBufferSafe(swgt.GetBuffer(), RPGEODATA_FLOAT, 4, 1, numVerts * 4);
			}
			else
			{
				if (hasSingleBone) bs.SetOffset(bs.GetOffset() + numVerts * 4);//if same time have 4 bones/weights and singel bone just skip single bone data
			}

			//Skin BoneIDs			
			RichBitStream bid;
			if (hasSkin)
			{
				for (int j = 0; j < numVerts; j++)
				{
					BYTE bone0 = boneMap[bs.ReadFloat()];
					BYTE bone1 = boneMap[bs.ReadFloat()];
					BYTE bone2 = boneMap[bs.ReadFloat()];
					BYTE bone3 = boneMap[bs.ReadFloat()];
					bid.WriteByte(bone0); bid.WriteByte(bone1); bid.WriteByte(bone2); bid.WriteByte(bone3);

				}
			}

			//Skin Bone Weights			
			float* bwgtAr = (hasSkin) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasSkin) bs.SetOffset(bs.GetOffset() + numVerts * 16);

			if (!memcmp(hdr->id, "xng", 4) || !memcmp(hdr->id, "p3g", 4))
			{
				//UV2
				float* uv2Ar = (hasUV2) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasUV2)
				{
					bs.SetOffset(bs.GetOffset() + numVerts * 8);
					rapi->rpgBindUV2BufferSafe(uv2Ar, RPGEODATA_FLOAT, 8, numVerts * 8);
				}
				//UV3
				float* uv3Ar = (hasUV3) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasUV3)
				{
					bs.SetOffset(bs.GetOffset() + numVerts * 8);
					rapi->rpgBindUVXBufferSafe(uv3Ar, RPGEODATA_FLOAT, 8, 2, 2, numVerts * 8);
				}

				//UV4
				float* uv4Ar = (hasUV4) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
				if (hasUV4)
				{
					bs.SetOffset(bs.GetOffset() + numVerts * 8);
					rapi->rpgBindUVXBufferSafe(uv3Ar, RPGEODATA_FLOAT, 8, 3, 2, numVerts * 8);
				}

				//Tangent			
				if (hasTangent) bs.SetOffset(bs.GetOffset() + numVerts * 12); //Just SKIP
			}



			/*
			for (int j = 0; j < numVerts; j++)
			{

				if (hasPos) rapi->rpgBindPositionBuffer(posAr, RPGEODATA_FLOAT, 12);
				if (hasNormal) rapi->rpgBindNormalBuffer(nrmAr, RPGEODATA_FLOAT, 12);
				if (hasUV) rapi->rpgBindUV1Buffer(uvAr, RPGEODATA_FLOAT, 8);
				if (hasUV2) rapi->rpgBindUV2Buffer(uv2Ar, RPGEODATA_FLOAT, 8);
				if (hasUV3) rapi->rpgBindUVXBuffer(uv3Ar, RPGEODATA_FLOAT, 8, 2, 2);
				if (hasUV4) rapi->rpgBindUVXBuffer(uv4Ar, RPGEODATA_FLOAT, 8, 3, 2);
			}
			*/
			//if (hasColor) rapi->rpgBindColorBufferSafe(colorAr.GetBuffer(), RPGEODATA_UBYTE, 4, 4, numVerts * 4);
			if (hasSkin)
			{
				rapi->rpgBindBoneIndexBufferSafe(bid.GetBuffer(), RPGEODATA_UBYTE, 4, 4, numVerts * 4);
				rapi->rpgBindBoneWeightBufferSafe(bwgtAr, RPGEODATA_FLOAT, 16, 4, numVerts * 16);
			}

			//rapi->rpgBindPositionBufferSafe(posAr, RPGEODATA_FLOAT, 12, numVerts * 12);
			//if (hasNormal) rapi->rpgBindNormalBufferSafe(nrmAr, RPGEODATA_FLOAT, 12, numVerts * 12);
			//if (hasUV) rapi->rpgBindUV1BufferSafe(uvAr, RPGEODATA_FLOAT, 8, numVerts * 12);


			char meshName[64];
			sprintf_s(meshName, "LOD%i_Surf%i_%s", lodIndex, meshID, meshNames[meshID]);
			rapi->rpgSetName(meshName);
			rapi->rpgSetMaterial(meshNames[meshID]);

			rapi->rpgCommitTriangles(faceAr, RPGEODATA_USHORT, numFaceIndex, RPGEO_TRIANGLE_STRIP, TRUE);
			rapi->rpgClearBufferBinds();

		}
		noesisModel_t* mdl = rapi->rpgConstructModel();
		if (mdl)
		{
			models.Append(mdl);
		}

		rapi->rpgReset();
	}
	bs.~RichBitStream();

	numMdl = models.Num();
	noesisModel_t* mdls = rapi->Noesis_ModelsFromList(models, numMdl);


	rapi->rpgDestroyContext(pgctx);
	models.Clear();
	return mdls;
}
