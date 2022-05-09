#include "pch.h"
#include <vector>
#include "CarsTypes.h"
using namespace std;

void skipVifPadding(RichBitStream& bs, VIF_UNPACK& vifcode);

noesisModel_t* Model_PSG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi)
{
	void* pgctx = rapi->rpgCreateContext();

	xngHdr_t* hdr = (xngHdr_t*)fileBuffer;
	RichBitStream bs = RichBitStream(fileBuffer, bufferLen);

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
			dstBone->mat = srcBone->mat.ToMat43().m;
			dstBone->index = i;			
			int parentID = srcBone->parentIdx;			
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


	float vertexDecompScale = bs.ReadFloat();
	float normalDecompScale = bs.ReadFloat();
	float uvDecompScale = bs.ReadFloat();
	float uvDecompScale2 = bs.ReadFloat();
	float BoundingCubeCenterX = bs.ReadFloat();
	float BoundingCubeCenterY = bs.ReadFloat();
	float BoundingCubeCenterZ = bs.ReadFloat();
	float FixedInWorldSpace = bs.ReadInt();
	

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

			BYTE numUV = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);

			int numVerts = *(USHORT*)(fileBuffer+bs.GetOffset());
			int numStrips = *(USHORT*)(fileBuffer + bs.GetOffset() + 2);
			bs.SetOffset(bs.GetOffset() + 4);
			int dmaChunkSize = bs.ReadInt();
			
			bs.SetOffset(bs.GetOffset() + dmaChunkSize);

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
		float AutoLODValue = bs.ReadFloat();
		int numMeshes = bs.ReadInt();
		for (int meshIndex = 0; meshIndex < numMeshes; meshIndex++)
		{

			int meshID = bs.ReadInt();
			int vertFormat = bs.ReadInt();

			bool hasPos = (vertFormat & 1) == 1;
			bool hasNormal = (vertFormat & 2) == 2;
			bool hasUV = (vertFormat & 4) == 4;
			bool hasColor = (vertFormat & 8) == 8;
			bool hasUV2 = (vertFormat & 0x10) == 0x10;
			bool hasTangent = (vertFormat & 0x20) == 0x20;
			bool hasSkin = (vertFormat & 0x1000) == 0x1000; //HAS_WEIGHTS
			bool hasDeltaShader = (vertFormat & HAS_DELTA_SHADER) == HAS_DELTA_SHADER;


			BYTE numUV = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);

			int totalVerts = *(USHORT*)(fileBuffer + bs.GetOffset());
			int numStrips = *(USHORT*)(fileBuffer + bs.GetOffset() + 2);
			bs.SetOffset(bs.GetOffset() + 4);
			int dmaChunkSize = bs.ReadInt();
			int nextOfs = bs.GetOffset() + dmaChunkSize;
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


			int stripIndex = 0;
			do {
				VIFcode* vifcode = (VIFcode*)(fileBuffer + bs.GetOffset());
				bs.SetOffset(bs.GetOffset() + 4);
				
				if (vifcode->CMD & VIF_UNPACK_FLAG)
				{
					int numVerts = 0;
					bs.SetOffset(bs.GetOffset() + 16);//skip GIFtag struct;

					BYTE* bidBuffer = NULL;
					vector<BYTE> faceBuffer;
					//Vertices
					if (hasPos) {

						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);
						
						//rapi->LogOutput("vertexOfs:0x%x\n", bs.GetOffset());

						numVerts = vifunpack->NUM;		
						int format = (vifunpack->vn << 2) | vifunpack->vl;
						bidBuffer = new BYTE[numVerts * 4];
						unsigned short* skipFlag = new unsigned short[numVerts];

						float* vertBuffer = hasPos ? (new float[sizeof(float) * numVerts * 3]) : NULL;
						switch (format)
						{
						case VIF_FORMAT_V4_32:
							for (int i = 0; i < numVerts; i++)
							{
								float compX = (float)bs.ReadInt();
								float compY = (float)bs.ReadInt();
								float compZ = (float)bs.ReadInt();
								int flag = bs.ReadInt();
								int index = (flag & 0x7fff) / 0x10 - 1 ;
								skipFlag[i] = flag & 0x8000;
								vertBuffer[i * 3 + 0] = compX * vertexDecompScale + BoundingCubeCenterX;
								vertBuffer[i * 3 + 1] = compY * vertexDecompScale + BoundingCubeCenterY;
								vertBuffer[i * 3 + 2] = compZ * vertexDecompScale + BoundingCubeCenterZ;
								if (hasSkin)
								{
									bidBuffer[i*4] = boneMap[index];
								}
							}
							break;
						case VIF_FORMAT_V4_16:

							for (int i = 0; i < numVerts; i++)
							{
								short compX = *(short*)(fileBuffer + bs.GetOffset());
								short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);
								short compZ = *(short*)(fileBuffer + bs.GetOffset() + 4);

								int flag = *(short*)(fileBuffer + bs.GetOffset() + 6) ;

								short index = (flag & 0x7fff) / 0x10 - 1;
								skipFlag[i] = flag & 0x8000;
								bs.SetOffset(bs.GetOffset() + 8);

								vertBuffer[i * 3 + 0] = (float)compX * vertexDecompScale + BoundingCubeCenterX;
								vertBuffer[i * 3 + 1] = (float)compY * vertexDecompScale + BoundingCubeCenterY;
								vertBuffer[i * 3 + 2] = (float)compZ * vertexDecompScale + BoundingCubeCenterZ;
								if (hasSkin)
								{
									bidBuffer[i * 4] = boneMap[index];
								}
							}

							break;
						case VIF_FORMAT_V4_8:
							for (int i = 0; i < numVerts; i++)
							{
								char compX = bs.ReadByte();
								char compY = bs.ReadByte();
								char compZ = bs.ReadByte();
								BYTE index = bs.ReadByte() / 0x10 - 1;
								vertBuffer[i * 3 + 0] = (float)compX * vertexDecompScale + BoundingCubeCenterX;
								vertBuffer[i * 3 + 1] = (float)compY * vertexDecompScale + BoundingCubeCenterY;
								vertBuffer[i * 3 + 2] = (float)compZ * vertexDecompScale + BoundingCubeCenterZ;
								if (hasSkin)
								{
									bidBuffer[i * 4] = boneMap[index];
								}
							}
							break;					
						}
						skipVifPadding(bs, *vifunpack);
						rapi->rpgBindPositionBufferSafe(vertBuffer, RPGEODATA_FLOAT, 12, numVerts * 12);

						int FaceDirection = 1;
						int f1 = 0, f2 = 1, f3 = 2;
						if (format == VIF_FORMAT_V4_8)
						{
							for (int i = 0; i < numVerts; i++)
							{
								if (i > 1)
								{
									f3 = i;
									RichVec3 v1(* (RichVec3*)&vertBuffer[i * 3]);
									RichVec3 v2(*(RichVec3*)&vertBuffer[(i - 2) * 3]);
									RichVec3 v3(*(RichVec3*)&vertBuffer[(i - 1) * 3]);
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
						}
						else
						{
							for (int i = 0; i < numVerts; i++)
							{
								
								
								f3 = i;

								if (skipFlag[i] != 0x8000)	// Make sure to break between the 2 triangle strips
								{
									RichVec3 v1(*(RichVec3*)&vertBuffer[i * 3]);
									RichVec3 v2(*(RichVec3*)&vertBuffer[(i - 2) * 3]);
									RichVec3 v3(*(RichVec3*)&vertBuffer[(i - 1) * 3]);
									if ((v1 != v2) && (v2 != v3) && (v1 != v3)) // cull degenerate triangles
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
									
								}

								f1 = f2;
								f2 = f3;
								FaceDirection *= -1;
								
							}

						}

					}
					//Normals
					if (hasNormal)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						int format = (vifunpack->vn << 2) | vifunpack->vl;
						float* normalBuffer = hasNormal ? (new float[sizeof(float) * numVerts * 3]) : NULL;
						switch (format)
						{
						case VIF_FORMAT_V3_32:
							memcpy_s(normalBuffer, numVerts * 12, (float*)(fileBuffer + bs.GetOffset()), numVerts * 12);
							bs.SetOffset(bs.GetOffset() + numVerts * 12);
							break;
						case VIF_FORMAT_V3_16:
							for (int i = 0; i < numVerts; i++)
							{
								short compX = *(short*)(fileBuffer + bs.GetOffset());
								short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);
								short compZ = *(short*)(fileBuffer + bs.GetOffset() + 4);

								bs.SetOffset(bs.GetOffset() + 6);

								normalBuffer[i * 3 + 0] = (float)compX * normalDecompScale;
								normalBuffer[i * 3 + 1] = (float)compY * normalDecompScale;
								normalBuffer[i * 3 + 2] = (float)compZ * normalDecompScale;
							}

							break;
						case VIF_FORMAT_V3_8:
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
						skipVifPadding(bs, *vifunpack);
						rapi->rpgBindNormalBufferSafe(normalBuffer, RPGEODATA_FLOAT, 12, numVerts * 12);
					}
					//UV1
					if (hasUV)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						int format = (vifunpack->vn << 2) | vifunpack->vl;
						float* uvBuffer = hasUV ? (new float[sizeof(float) * numVerts * 2]) : NULL;
						switch (format)
						{
						case VIF_FORMAT_V2_32:
							for (int i = 0; i < numVerts; i++)
							{
								uvBuffer[i * 2 + 0] = (float)bs.ReadInt() * uvDecompScale;
								uvBuffer[i * 2 + 1] = (float)bs.ReadInt() * uvDecompScale;
							}
							break;
						case VIF_FORMAT_V2_16:
							for (int i = 0; i < numVerts; i++)
							{
								short compX = *(short*)(fileBuffer + bs.GetOffset());
								short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);

								bs.SetOffset(bs.GetOffset() + 4);

								uvBuffer[i * 2 + 0] = (float)compX * uvDecompScale;
								uvBuffer[i * 2 + 1] = (float)compY * uvDecompScale;
							}
							break;
						case VIF_FORMAT_V2_8:
							for (int i = 0; i < numVerts; i++)
							{
								char compX = bs.ReadByte();
								char compY = bs.ReadByte();
								uvBuffer[i * 2 + 0] = (float)compX * uvDecompScale;
								uvBuffer[i * 2 + 1] = (float)compY * uvDecompScale;
							}
							break;
						}
						skipVifPadding(bs, *vifunpack);
						rapi->rpgBindUV1BufferSafe(uvBuffer, RPGEODATA_FLOAT, 8, numVerts * 8);
					}

					// Skin BoneIDs									
					if (hasSkin)
					{
						//rapi->LogOutput("bidOfs:0x%x\n", bs.GetOffset());
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;

						for (int j = 0; j < numVerts; j++)
						{
							short bidx1 = *(short*)(fileBuffer + bs.GetOffset());
							short bidx2 = *(short*)(fileBuffer + bs.GetOffset() + 2);
							short bidx3 = *(short*)(fileBuffer + bs.GetOffset() + 4);
							short bidx4 = *(short*)(fileBuffer + bs.GetOffset() + 6);
							bidBuffer[j * 4 + 1] = boneMap[bidx1 / 0x10 - 1];
							bidBuffer[j * 4 + 2] = boneMap[bidx2 / 0x10 - 1];
							bidBuffer[j * 4 + 3] = boneMap[bidx3 / 0x10 - 1];
							BYTE bone4_pad = boneMap[bidx4 / 0x10 - 1];
							bs.SetOffset(bs.GetOffset() + 8);
							
						}
						skipVifPadding(bs, *vifunpack);
					}

					// Colors
					RichBitStream colorAr;
					if (hasColor && !hasSkin)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						for (int j = 0; j < numVerts; j++)
						{
							BYTE B = bs.ReadByte();
							BYTE G = bs.ReadByte();
							BYTE R = bs.ReadByte();
							BYTE A = bs.ReadByte();
							colorAr.WriteByte(R); colorAr.WriteByte(G); colorAr.WriteByte(B); colorAr.WriteByte(A);
						}
						skipVifPadding(bs, *vifunpack);
						rapi->rpgBindColorBufferSafe(colorAr.GetBuffer(), RPGEODATA_UBYTE, 4, 4, numVerts * 4);
					}

					// UV2
					
					if (hasUV2)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						int format = (vifunpack->vn << 2) | vifunpack->vl;
						float* uv2Buffer = hasUV2 ? (new float[sizeof(float) * numVerts * 2]) : NULL;
						switch (format)
						{
						case VIF_FORMAT_V2_32:
							for (int i = 0; i < numVerts; i++)
							{
								uv2Buffer[i * 2 + 0] = bs.ReadFloat();
								uv2Buffer[i * 2 + 1] = bs.ReadFloat();
							}
							break;
						case VIF_FORMAT_V2_16:
							for (int i = 0; i < numVerts; i++)
							{
								short compX = *(short*)(fileBuffer + bs.GetOffset());
								short compY = *(short*)(fileBuffer + bs.GetOffset() + 2);

								bs.SetOffset(bs.GetOffset() + 4);

								uv2Buffer[i * 2 + 0] = (float)compX * uvDecompScale;
								uv2Buffer[i * 2 + 1] = (float)compY * uvDecompScale;
							}
							break;
						case VIF_FORMAT_V2_8:
							for (int i = 0; i < numVerts; i++)
							{
								char compX = bs.ReadByte();
								char compY = bs.ReadByte();
								uv2Buffer[i * 2 + 0] = (float)compX * uvDecompScale;
								uv2Buffer[i * 2 + 1] = (float)compY * uvDecompScale;
							}
							break;
						}
						skipVifPadding(bs, *vifunpack);
						rapi->rpgBindUV2BufferSafe(uv2Buffer, RPGEODATA_FLOAT, 8, numVerts * 8);
					}

					//Tangents
					if (hasTangent)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						int format = (vifunpack->vn << 2) | vifunpack->vl;
						switch (format)
						{
						case VIF_FORMAT_V3_8:	bs.SetOffset(bs.GetOffset() + numVerts * 3); break;
						case VIF_FORMAT_V3_16:	bs.SetOffset(bs.GetOffset() + numVerts * 6); break;
						case VIF_FORMAT_V3_32:	bs.SetOffset(bs.GetOffset() + numVerts * 12); break;
						}
						skipVifPadding(bs, *vifunpack);
					}




					//Skin Bone Weights		
					RichBitStream bwgt;
					if (hasSkin)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);

						int numVerts = vifunpack->NUM;
						for (int i = 0; i < numVerts; i++)
						{
							bwgt.WriteFloat((float)bs.ReadByte() / 255.0f);
							bwgt.WriteFloat((float)bs.ReadByte() / 255.0f);
							bwgt.WriteFloat((float)bs.ReadByte() / 255.0f);
							bwgt.WriteFloat((float)bs.ReadByte() / 255.0f);
						}
						skipVifPadding(bs, *vifunpack);
						
					}
					if (hasDeltaShader)
					{
						VIF_UNPACK* vifunpack = (VIF_UNPACK*)(fileBuffer + bs.GetOffset());
						bs.SetOffset(bs.GetOffset() + 4);
						bs.SetOffset(bs.GetOffset() + vifunpack->NUM * 12);
						skipVifPadding(bs, *vifunpack);
					}

					//skip MSCNT
					VIFcode* vifcode = (VIFcode*)(fileBuffer + bs.GetOffset());
					bs.SetOffset(bs.GetOffset() + 4);

					if (hasSkin)
					{
						rapi->rpgBindBoneIndexBufferSafe(bidBuffer, RPGEODATA_UBYTE, 4, 4, numVerts * 4);
						rapi->rpgBindBoneWeightBufferSafe(bwgt.GetBuffer(), RPGEODATA_FLOAT, 16, 4, numVerts * 16);
					}



					char meshName[64];
					sprintf_s(meshName, "LOD%i_Surf%i_%s", lodIndex, meshID, meshNames[meshID]);
					//sprintf_s(meshName, "LOD%i_Surf%i_%s_%d", lodIndex, meshID, meshNames[meshID],stripIndex);
					rapi->rpgSetName(meshName);
					rapi->rpgSetMaterial(meshNames[meshID]);

					rapi->rpgCommitTriangles(faceBuffer.data(), RPGEODATA_UBYTE, faceBuffer.size(), RPGEO_TRIANGLE, TRUE);
					rapi->rpgClearBufferBinds();

					stripIndex++;
				}

			} while (bs.GetOffset() < nextOfs);
			bs.SetOffset(nextOfs);
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

void skipVifPadding(RichBitStream& bs, VIF_UNPACK& vifcode)
{
	int dataSize = vifcode.vn + 1;


	switch (vifcode.vl)
	{
	case 0: dataSize *= 4; break;
	case 1: dataSize *= 2; break;
	case 2: dataSize *= 1; break;
	case 3: dataSize *= 3; break;
	}
	dataSize *= vifcode.NUM;

	if ((dataSize % 4) > 0)
	{
		bs.SetOffset(bs.GetOffset() + (4 - (dataSize % 4)));
	}
}