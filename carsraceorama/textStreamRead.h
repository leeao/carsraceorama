#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace std;

class textStreamRead
{

public:

	
	char *inputName;	
	int fileLen;
	
	
	vector<string> linesList;

	vector<int>	blockHeaderLineNumberList;
	vector<int>	blockDataLineNumberList;

	vector<string> blockNames;
	vector<int>	blockOffsets;
	vector<int>	blockDataOffsets;

	vector<string> parameterNames;
	vector<string> parameterValues;
	string currentBlockName;
	vector<vector<string>> currentBlockDataLineValues;


	void SetCurrentFile(char* fname);
	bool ReadParameterBlock(const char* blockName);
	bool ReadParameterBlock2(const char* blockName);
	void ClearParameter();
	int FindParameterBlock(const char* blockName);
	bool GetParameter(const char* parameterName, int* value);
	bool GetParameter(const char* parameterName, unsigned int* value);
	bool GetParameter(const char* parameterName, char* value);
	bool GetParameter(const char* parameterName, float* value);
	void Split(string str, char splitChar, vector<string>& strArray);
	bool ReadDataBlock(const char* blockName);
	bool ReadDataBlock2(const char* blockName);
	bool GetData(int lineIndex, int index, float* value);
	bool GetData(int lineIndex, int index, char* value);
	bool GetData(int lineIndex, int index, unsigned int* value);
};