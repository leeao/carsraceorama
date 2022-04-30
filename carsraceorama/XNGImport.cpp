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
		for (int i = 0; i < numMeshes; i++)
		{
			int meshID = bs.ReadInt();
			int vertFormat = bs.ReadInt();
			BYTE numUV = bs.ReadByte();
			BYTE compressed = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);
			

			unsigned short tempUSHORT[2];

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*) & tempUSHORT);
			int numVerts = *(USHORT*)tempUSHORT;

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numFaceIndex = *(USHORT*)tempUSHORT;


			bool hasPos = (vertFormat & 1) == 1;
			bool hasNormal = (vertFormat & 2) == 2;
			bool hasColor = (vertFormat & 8) == 8;
			bool hasUV = (vertFormat & 4) == 4;
			bool hasUV2 = (vertFormat & 0x10) == 0x10;
			bool hasTangent = (vertFormat & 0x20) == 0x20;
			bool hasSingleBone = (vertFormat & 0x40) == 0x40;//HAS_BONE_INDEX
			bool hasDeltaCpu = (vertFormat & 0x100) == 0x100;
			bool hasSkin = (vertFormat & 0x1000) == 0x1000; //HAS_WEIGHTS
			bool hasUV3 = (vertFormat & 0x4000) == 0x4000;
			bool hasUV4 = (vertFormat & 0x8000) == 0x8000;

			bs.SetOffset(bs.GetOffset() + numFaceIndex * 2);
			if (!memcmp(hdr->id,"xng",4) || !memcmp(hdr->id,"p3g",4))
			{
				if ((numFaceIndex % 2) == 1) bs.SetOffset(bs.GetOffset() + 2); // skip unused flag
			}
			

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
	}
	
	vector<vector<BYTE>> skinBonesList;
	vector<BYTE> usedSkinListIDArray;
	if (Animates)
	{
		int info = bs.ReadInt();
		short numUsedSkinBonesList = (short)(info & 0xffff);
		if (numUsedSkinBonesList > 0)
		{
			for (int id = 0; id < numUsedSkinBonesList; id++)
			{
				vector<BYTE> skinBoneIDs;
				int numBoneIDs = bs.ReadInt();
				for (int j = 0; j < numBoneIDs; j++)
				{
					BYTE boneID = bs.ReadByte();
					skinBoneIDs.push_back(boneID);
				}
				skinBonesList.push_back(skinBoneIDs);
			}


			if ((numSurface > 1) && ComplexMatrixPalettes)//ComplexMatrixPalettes
			{
				for (int id = 0; id < numSurface; id++)
				{
					usedSkinListIDArray.push_back(bs.ReadByte());
				}
			}
			else//Simple palettes
			{
				usedSkinListIDArray.push_back(0);
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
		for (int i = 0; i < numMeshes; i++)
		{

			int meshID = bs.ReadInt();
			int vertFormat = bs.ReadInt();
			BYTE numUV = bs.ReadByte();
			BYTE compressed = bs.ReadByte();
			BYTE streaming = bs.ReadByte();
			BYTE unused1 = bs.ReadByte();
			BYTE unused2 = bs.ReadByte();
			BYTE unused3 = bs.ReadByte();
			bs.SetOffset(bs.GetOffset() + numUV * 4);

			unsigned short tempUSHORT[2];

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numVerts = *(USHORT*)tempUSHORT;

			bs.ReadBytes(tempUSHORT, 2);
			if (isBig) LITTLE_BIG_SWAP(*(USHORT*)&tempUSHORT);
			int numFaceIndex = *(USHORT*)tempUSHORT;

			bool hasPos = (vertFormat & 1) == 1;
			bool hasNormal = (vertFormat & 2) == 2;
			bool hasColor = (vertFormat & 8) == 8;
			bool hasUV = (vertFormat & 4) == 4;
			bool hasUV2 = (vertFormat & 0x10) == 0x10;
			bool hasTangent = (vertFormat & 0x20) == 0x20;
			bool hasSingleBone = (vertFormat & 0x40) == 0x40;//HAS_BONE_INDEX
			bool hasDeltaCpu = (vertFormat & 0x100) == 0x100;
			bool hasSkin = (vertFormat & 0x1000) == 0x1000; //HAS_WEIGHTS
			bool hasUV3 = (vertFormat & 0x4000) == 0x4000;
			bool hasUV4 = (vertFormat & 0x8000) == 0x8000;

			//Face
			UINT16* faceAr = (UINT16*)(fileBuffer + bs.GetOffset());
			bs.SetOffset(bs.GetOffset() + numFaceIndex * 2);
			//unsigned short* faceBuffer = new unsigned short[numFaceIndex];

			vector<BYTE> boneIDs;
			if (Animates)
			{
				int index = meshID;
				if (!ComplexMatrixPalettes)
				{
					index = 0;
				}
				for (int j = 0; j < skinBonesList[usedSkinListIDArray[index]].size(); j++)
				{
					boneIDs.push_back(skinBonesList[usedSkinListIDArray[index]][j]);
				}
			}


			if (!memcmp(hdr->id, "xng", 4) || !memcmp(hdr->id, "p3g", 4))
			{
				if ((numFaceIndex % 2) == 1) bs.SetOffset(bs.GetOffset() + 2); // skip unused flag
			}

			//Position
			float* posAr = hasPos ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			BYTE* vertBuffer = hasPos ? (new BYTE[sizeof(RichVec3) * numVerts]) : NULL;
			if (hasPos) bs.SetOffset(bs.GetOffset() + numVerts * 12);

			//Normals
			float* nrmAr = hasNormal ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasNormal) bs.SetOffset(bs.GetOffset() + numVerts * 12);

			//Colors
			BYTE* srcClrAr = hasColor ? (BYTE*)(fileBuffer + bs.GetOffset()) : NULL;
			//if (hasColor) bs.SetOffset(bs.GetOffset() + numVerts * 4);
			RichBitStream colorAr;
			if (hasColor)
			{
				for (int j = 0; j < numVerts; j++)
				{
					BYTE A = bs.ReadByte();
					BYTE R = bs.ReadByte();
					BYTE G = bs.ReadByte();
					BYTE B = bs.ReadByte();
					colorAr.WriteByte(R); colorAr.WriteByte(G); colorAr.WriteByte(B); colorAr.WriteByte(A);
				}
			}

			//UV1
			float* uvAr = (hasUV) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasUV)  bs.SetOffset(bs.GetOffset() + numVerts * 8);

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
						BYTE bone0 = boneIDs[bs.ReadFloat()];
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
			float* bidxAr = (hasSkin) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			RichBitStream bid;
			if (hasSkin)
			{
				for (int j = 0; j < numVerts; j++)
				{
					BYTE bone0 = boneIDs[bs.ReadFloat()];
					BYTE bone1 = boneIDs[bs.ReadFloat()];
					BYTE bone2 = boneIDs[bs.ReadFloat()];
					BYTE bone3 = boneIDs[bs.ReadFloat()];
					bid.WriteByte(bone0); bid.WriteByte(bone1); bid.WriteByte(bone2); bid.WriteByte(bone3);

				}
			}

			//Skin Bone Weights			
			float* bwgtAr = (hasSkin) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasSkin) bs.SetOffset(bs.GetOffset() + numVerts * 16);

			//UV2
			float* uv2Ar = (hasUV2) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasUV2) bs.SetOffset(bs.GetOffset() + numVerts * 8);

			//UV3
			float* uv3Ar = (hasUV3) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasUV3) bs.SetOffset(bs.GetOffset() + numVerts * 8);

			//UV4
			float* uv4Ar = (hasUV4) ? (float*)(fileBuffer + bs.GetOffset()) : NULL;
			if (hasUV4) bs.SetOffset(bs.GetOffset() + numVerts * 8);

			//Tangent			
			if (hasTangent) bs.SetOffset(bs.GetOffset() + numVerts * 12); //Just SKIP

			for (int j = 0; j < numVerts; j++)
			{

				if (hasPos) rapi->rpgBindPositionBuffer(posAr, RPGEODATA_FLOAT, 12);
				if (hasNormal) rapi->rpgBindNormalBuffer(nrmAr, RPGEODATA_FLOAT, 12);
				if (hasUV) rapi->rpgBindUV1Buffer(uvAr, RPGEODATA_FLOAT, 8);
				if (hasUV2) rapi->rpgBindUV2Buffer(uv2Ar, RPGEODATA_FLOAT, 8);
				if (hasUV3) rapi->rpgBindUVXBuffer(uv3Ar, RPGEODATA_FLOAT, 8, 2, 2);
				if (hasUV4) rapi->rpgBindUVXBuffer(uv4Ar, RPGEODATA_FLOAT, 8, 3, 2);
			}
			if (hasColor) rapi->rpgBindColorBufferSafe(colorAr.GetBuffer(), RPGEODATA_UBYTE, 4, 4, numVerts * 4);
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
