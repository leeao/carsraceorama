// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <vector>
#include "CarsTypes.h"
using namespace std;


const char* g_pPluginName = "carsraceorama";
const char* g_pPluginDesc = "XNG P3G GCG DXG format handler, by Allen.";

int g_fmtHandle;
sltOpts_t* g_opts = NULL;

extern noesisModel_t* Model_XNG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi);
extern noesisModel_t* Model_GCG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi);
extern noesisModel_t* Model_SLT_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi);
extern bool Model_SLT_Write(noesisModel_t* mdl, RichBitStream* outStream, noeRAPI_t* rapi);
extern noesisModel_t* Model_PSG_Load(BYTE* fileBuffer, int bufferLen, int& numMdl, noeRAPI_t* rapi);

//see if something is valid xng p3g data
bool Model_XNG_Check(BYTE* fileBuffer, int bufferLen, noeRAPI_t* rapi)
{
	xngHdr_t* hdr = (xngHdr_t*)fileBuffer;

	if (memcmp(hdr->id, "xng",4) && memcmp(hdr->id, "p3g",4) && memcmp(hdr->id, "dxg", 4))
	{
		return false;
	}
		
	if ((hdr->version != 0x05000000) && (hdr->version != 0x5)) 	//big endian ; little endian = 0x5
	{
		return false;
	}
	
	//rapi->LogOutput("");
	return true;
}

bool Model_SLT_Check(BYTE* fileBuffer, int bufferLen, noeRAPI_t* rapi)
{
	return true;
}

//see if something is valid gcg data
bool Model_GCG_Check(BYTE* fileBuffer, int bufferLen, noeRAPI_t* rapi)
{
	xngHdr_t* hdr = (xngHdr_t*)fileBuffer;

	if (memcmp(hdr->id, "gcg", 4))
	{
		return false;
	}

	if (hdr->version != 0x05000000)	//big endian ; little endian = 0x5
	{
		return false;
	}

	return true;
}

//see if something is valid psg data
bool Model_PSG_Check(BYTE* fileBuffer, int bufferLen, noeRAPI_t* rapi)
{
	xngHdr_t* hdr = (xngHdr_t*)fileBuffer;

	if (memcmp(hdr->id, "psg", 4))
	{
		return false;
	}

	if (hdr->version != 0x5)	//big endian ; little endian = 0x5
	{
		return false;
	}

	return true;
}
//handle -fixalpha
static bool Model_SLT_OptHandlerA(const char* arg, unsigned char* store, int storeSize)
{
	sltOpts_t* lopts = (sltOpts_t*)store;
	assert(storeSize == sizeof(sltOpts_t));
	lopts->fixAlpha = 1;
	return true;
}
//handle -removecolor
static bool Model_SLT_OptHandlerB(const char* arg, unsigned char* store, int storeSize)
{
	sltOpts_t* lopts = (sltOpts_t*)store;
	assert(storeSize == sizeof(sltOpts_t));
	lopts->removeColor = 1;
	return true;
}

static void *AddOption(addOptParms_t& optParms, int fmtHandle, char* optName, char* optDescr, bool(*handler)(const char* arg, unsigned char* store, int storeSize))
{
	
	
	optParms.optName = optName;
	optParms.optDescr = optDescr;

	optParms.handler = handler;
	//optParms.flags |= OPTFLAG_WANTARG;
	//g_opts = (sltOpts_t*)g_nfn->NPAPI_AddTypeOption(fmtHandle, &optParms);
	return g_nfn->NPAPI_AddTypeOption(fmtHandle, &optParms);
	
}
static sltOpts_t* optionInit(addOptParms_t &optParms, int fmtHandle, char* optName, char* optDescr, bool(*handler)(const char* arg, unsigned char* store, int storeSize))
{
	memset(&optParms, 0, sizeof(optParms));
	optParms.storeSize = sizeof(sltOpts_t);
	sltOpts_t *opts = (sltOpts_t*)AddOption(optParms, fmtHandle, optName, optDescr, handler);
	assert(opts);
	optParms.shareStore = (unsigned char*)opts;
	return opts;
}

//called by Noesis to init the plugin
bool NPAPI_InitLocal(void)
{
	g_fmtHandle = g_nfn->NPAPI_Register((char *)"Cars Race-O-Rama XBOX360 PS3 PC", (char*)".xng;.p3g;.dxg");
	int fmtSLT = g_nfn->NPAPI_Register((char*)"Cars SLT Model Format", (char*)".slt");
	int fmtGCG = g_nfn->NPAPI_Register((char*)"Cars Race-O-Rama WII", (char*)".gcg");
	int fmgPSG = g_nfn->NPAPI_Register((char*)"Cars Race-O-Rama PS2", (char*)".psg");
	
	if (g_fmtHandle < 0 || fmtSLT < 0 || fmtGCG < 0 || fmgPSG < 0)
	{
		return false;
	}

	g_nfn->NPAPI_SetTypeHandler_TypeCheck(g_fmtHandle, Model_XNG_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(g_fmtHandle, Model_XNG_Load);
	//if (!g_nfn->NPAPI_DebugLogIsOpen())
	//	g_nfn->NPAPI_PopupDebugLog(0);
	
	
	
	//add first parm
	addOptParms_t optParms;
	g_opts = optionInit(optParms, fmtSLT, (char*)"-fixalpha", (char*)"disable vertex color alpha", Model_SLT_OptHandlerA);
	AddOption(optParms, fmtSLT, (char*)"-removecolor", (char*)"remove vertex color rgba", Model_SLT_OptHandlerB);
	/*
	memset(&optParms, 0, sizeof(optParms));
	optParms.optName = (char*)"-fixalpha";
	optParms.optDescr = (char*)"disable vertex color alpha";
	optParms.storeSize = sizeof(sltOpts_t);
	optParms.handler = Model_SLT_OptHandlerA;
	//optParms.flags |= OPTFLAG_WANTARG;
	g_opts = (sltOpts_t*)g_nfn->NPAPI_AddTypeOption(fmtSLT, &optParms);
	assert(g_opts);
	optParms.shareStore = (unsigned char*)g_opts;
	*/


	g_nfn->NPAPI_SetTypeHandler_TypeCheck(fmtSLT, Model_SLT_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(fmtSLT, Model_SLT_Load);
	g_nfn->NPAPI_SetTypeHandler_WriteModel(fmtSLT, Model_SLT_Write);
	




	g_nfn->NPAPI_SetTypeHandler_TypeCheck(fmtGCG, Model_GCG_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(fmtGCG, Model_GCG_Load);

	g_nfn->NPAPI_SetTypeHandler_TypeCheck(fmgPSG, Model_PSG_Check);
	g_nfn->NPAPI_SetTypeHandler_LoadModel(fmgPSG, Model_PSG_Load);
	return true;
}
//called by Noesis before the plugin is freed
void NPAPI_ShutdownLocal(void)
{
	//nothing to do in this plugin
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
  
