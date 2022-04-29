#pragma once

#define COMPRESSED_MATRIX_PALETTES  0x80000000
#define SIMPLE_MATRIX_PALETTE       0x40000000
#define MAX_BONES                   29
#define MAX_BONE_NEIGHBORS          6
#define MAX_BONE_IDS                256

//GCG WII
#define GX_NOP                   0x00
#define GX_DRAW_TRIANGLES        0x90
#define GX_DRAW_TRIANGLE_STRIP   0x98

#define GCG_HAS_NORMAL           0x01
#define GCG_HAS_UV1              0x02
#define GCG_HAS_UV2              0x04
#define GCG_HAS_SKIN             0x08
#define GCG_HAS_UNK              0x40
#define GCG_STREAMED             0x80

//XNG P3G XBOX360 PS3
#define HAS_XYZ            0x01
#define HAS_NORMAL         0x02
#define HAS_UV1            0x04
#define HAS_RGBA           0x08
#define HAS_UV2            0x10
#define HAS_TANGENT        0x20
#define HAS_BONE_INDEX     0x40
#define HAS_TEXTURE_BASIS  0x80
#define HAS_DELTA_CPU      0x100
#define HAS_COMPRESSION    0x200
#define HAS_DELTA_SHADER   0x400
#define HAS_DELTA_NORMALS  0x800
#define HAS_WEIGHTS        0x1000
#define HAS_GEOMETRY_COMPRESSION 0x2000
#define HAS_UV3            0x4000
#define HAS_UV4            0x8000

#define SWAP16(w)((((uint16_t)w & 0xff00) >> 8)|(((uint16_t)w & 0x00ff) << 8))
#define SWAP32(dw)((((uint32_t)dw & 0xFF000000) >> 24)|(((uint32_t)dw & 0x00ff0000) >> 8)|(((uint32_t)dw & 0x0000ff00)<< 8)|(((uint32_t)dw & 0x000000ff) << 24))

//GCG WII
typedef enum _GXCompType
{
    GX_U8 = 0,
    GX_S8 = 1,
    GX_U16 = 2,
    GX_S16 = 3,
    GX_F32 = 4,

    GX_RGB565 = 0,
    GX_RGB8 = 1,
    GX_RGBX8 = 2,
    GX_RGBA4 = 3,
    GX_RGBA6 = 4,
    GX_RGBA8 = 5
}
GXCompType;

typedef enum _GXAttrType
{
    GX_NONE = 0,
    GX_DIRECT = 1,
    GX_INDEX8 = 2,
    GX_INDEX16 = 3
}
GXAttrType;


//Common
typedef struct xngHdr_s
{
	char    id[4];//xng or p3g or gcg
	int     version;//5
	int		numBones;
} xngHdr_t;

typedef struct xngBone_s {
	char			name[128];
	RichMat44		mat;
	float			boundingBoxCenter[3];
	float			boundingBoxHalf[3];
	float			boundingBoxRadius;
	int				parentIdx;
}xngBone_t;

typedef struct xngMeshName_s {
	char    meshName[64];//matreial name
}xngMeshName_t;

//GCG WII
typedef struct skinWeights_s {
	short		boneID;
	float		weight;
}skinWeights_t;

typedef struct skinBoneWeightIndex_s {
    DWORD matrixOffset;
    short weightIDs[4];
    void swap(void) {
        LITTLE_BIG_SWAP(matrixOffset);
        for (int i = 0; i < 4; i++) LITTLE_BIG_SWAP(weightIDs[i]);
    }

}skinBoneWeightIndex_t;

typedef struct GcgSkinBoneInfo_s{
    short   id1;
    short   id2;
    short   id3;
    BYTE   matrixType;//4 = Matrix3x3 0 =Matrix3x4
    BYTE   matrixDataIDOffset;//9 floats = Matrix3x3 9*float(4bytes) = 36 bytes per data .,12 floats =Matrix3x4 12*float(4bytes) = 48 bytes per data .
    //matrix3x3 or 3x4
}GcgSkinBoneInfo_t;


typedef struct RotationMatrix3x4_s {
    float   row1[4];
    float   row2[4];
    float   row3[4];
}RotationMatrix3x4_t;

typedef struct RotationMatrix3x3_s {
    float   row1[3];
    float   row2[3];
    float   row3[3];
}RotationMatrix3x3_t;

//not use
typedef struct SLTBone_s
{
    char        name[128];
    char        parentName[128];
    RichMat43   mat;
    RichMat43   localMatrix;
    int	        index;
    SLTBone_s   *parent;
    SLTBone_s   *sub;
    SLTBone_s   *sibling;
}SLTBone_t;