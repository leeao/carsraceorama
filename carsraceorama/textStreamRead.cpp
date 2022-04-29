#include "textStreamRead.h"

void textStreamRead::SetCurrentFile(char* fname)
{
	linesList.clear();
	inputName = fname;
	std::ifstream fin;
	fin.open(fname, ios::in);
	if (fin.fail())
	{
		g_nfn->NPAPI_DebugLogStr((char*)"Fail open file:\n");
		g_nfn->NPAPI_DebugLogStr(fname);
		g_nfn->NPAPI_DebugLogStr((char*)"\n");
		return;
	}

	fileLen = fin.end;       

	string buf;
	while (getline(fin, buf))
	{
		linesList.push_back(buf);
		if (buf[0] == '[')
		{
			blockNames.push_back(buf.c_str());
			blockOffsets.push_back((int)fin.tellg() - (buf.size() + 2));
			blockDataOffsets.push_back((int)fin.tellg());

			blockHeaderLineNumberList.push_back(linesList.size()-1);
			blockDataLineNumberList.push_back(linesList.size());
		}		
	}
	fin.close();

}




bool textStreamRead::ReadParameterBlock(const char* blockName)
{
	ClearParameter();
	std::ifstream fin;
	fin.open(inputName, ios::in);

	char fullBlockName[128];
	sprintf(fullBlockName, "[%s]", blockName);
	currentBlockName = fullBlockName;
	
	int blockIndex = FindParameterBlock(fullBlockName);
	if (blockIndex != -1)
	{
		int endOfs = 0;
		if (blockIndex != blockNames.size() - 1)
		{
			endOfs = blockOffsets[blockIndex + 1];
		}
		else {
			endOfs = fin.end;
		}
		fin.seekg(blockDataOffsets[blockIndex], ios::beg);
		while (fin.tellg() < endOfs)
		{
			string str;			
			if (getline(fin, str))
			{
				size_t pos = str.find('=');
				size_t size = str.size();
				if (pos != str.npos)
				{
					string parametrName = str.substr(0, pos);
					string parameterValue = str.substr(pos + 1, size-1-pos);
					parameterNames.push_back(parametrName);
					parameterValues.push_back(parameterValue);
				}
			}

		} 
	}
	else
	{
		fin.close();
		return false;
	}
	fin.close();
	return true;
}

bool textStreamRead::ReadParameterBlock2(const char* blockName)
{
	ClearParameter();


	char fullBlockName[128];
	sprintf(fullBlockName, "[%s]", blockName);
	currentBlockName = fullBlockName;

	int blockIndex = FindParameterBlock(fullBlockName);
	if (blockIndex != -1)
	{
		int endLineNumber = 0;
		if (blockIndex != blockNames.size() - 1)
		{
			endLineNumber = blockHeaderLineNumberList[blockIndex + 1];
		}
		else {
			endLineNumber = blockHeaderLineNumberList.size()-1;
		}
		int startLineIndex = blockDataLineNumberList[blockIndex];
		
		for(int i = startLineIndex; i < endLineNumber; i++)
		{
			string str = linesList[i];

			size_t pos = str.find('=');
			size_t size = str.size();
			if (pos != str.npos)
			{
				string parametrName = str.substr(0, pos);
				string parameterValue = str.substr(pos + 1, size - 1 - pos);
				parameterNames.push_back(parametrName);
				parameterValues.push_back(parameterValue);
			}
			

		}
	}
	else
	{
		return false;
	}
	return false;
}


void textStreamRead::ClearParameter()
{
	parameterNames.clear();
	parameterValues.clear();
}
int textStreamRead::FindParameterBlock(const char* blockName)
{
	for (int i = 0; i < blockNames.size(); i++)
	{
		if (blockNames[i] == blockName)
		{
			return i;
		}
	}
	return -1;
}
bool textStreamRead::GetParameter(const char* parameterName, int* value)
{
	for (int i = 0; i < parameterNames.size(); i++)
	{
		if (parameterNames[i] == parameterName)
		{
			*value = stoi(parameterValues[i]);
			return i;
		}
	}
	return false;
}
bool textStreamRead::GetParameter(const char* parameterName, unsigned int* value)
{
	for (int i = 0; i < parameterNames.size(); i++)
	{
		if (parameterNames[i] == parameterName)
		{
			*value = stoul(parameterValues[i]);
			return i;
		}
	}
	return false;
}
bool textStreamRead::GetParameter(const char* parameterName, char* value)
{
	for (int i = 0; i < parameterNames.size(); i++)
	{
		if (parameterNames[i] == parameterName)
		{			
			strcpy(value, parameterValues[i].c_str());
			return i;
		}
	}
	return false;
}
bool textStreamRead::GetParameter(const char* parameterName, float* value)
{
	for (int i = 0; i < parameterNames.size(); i++)
	{
		if (parameterNames[i] == parameterName)
		{
			*value = stof(parameterValues[i].c_str());
			return i;
		}
	}
	return false;
}
void textStreamRead::Split(string str,char splitChar,vector<string>& strArray) 
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
bool textStreamRead::ReadDataBlock(const char* blockName)
{
	currentBlockDataLineValues.clear();
	std::ifstream fin;
	fin.open(inputName, ios::in);

	char fullBlockName[128];
	sprintf(fullBlockName, "[%s]", blockName);
	currentBlockName = fullBlockName;

	int blockIndex = FindParameterBlock(fullBlockName);
	if (blockIndex != -1)
	{
		int endOfs = 0;
		if (blockIndex != blockNames.size() - 1)
		{
			endOfs = blockOffsets[blockIndex + 1];
		}
		else {
			endOfs = fin.end;
		}
		fin.seekg(blockDataOffsets[blockIndex], ios::beg);
		while (fin.tellg() < endOfs)
		{
			string str;
			if (getline(fin, str))
			{
				vector<string> curLineDatas;
				Split(str, ',', curLineDatas);
				if (curLineDatas.size() != 0)
				{
					currentBlockDataLineValues.push_back(curLineDatas);
				}
				
			}

		}
	}
	else
	{
		fin.close();
		return false;
	}
	fin.close();
	return true;
}

bool textStreamRead::ReadDataBlock2(const char* blockName)
{
	currentBlockDataLineValues.clear();


	char fullBlockName[128];
	sprintf(fullBlockName, "[%s]", blockName);
	currentBlockName = fullBlockName;

	int blockIndex = FindParameterBlock(fullBlockName);
	if (blockIndex != -1)
	{
		int endLineNumber = 0;
		if (blockIndex != blockNames.size() - 1)
		{
			endLineNumber = blockHeaderLineNumberList[blockIndex + 1];
		}
		else {
			endLineNumber = linesList.size();
		}
		int startLineIndex = blockDataLineNumberList[blockIndex];

		for (int i = startLineIndex; i < endLineNumber; i++)
		{
			string str = linesList[i];
			vector<string> curLineDatas;
			Split(str, ',', curLineDatas);
			if (curLineDatas.size() != 0)
			{
				currentBlockDataLineValues.push_back(curLineDatas);
			}
		}
	}
	else
	{

		return false;
	}

	return true;
}

bool textStreamRead::GetData(int lineIndex,int index, float* value)
{
	const char* str = currentBlockDataLineValues[lineIndex][index].c_str();
	*value = stof(str);
	return true;
}
bool textStreamRead::GetData(int lineIndex, int index, char* value)
{
	const char* str = currentBlockDataLineValues[lineIndex][index].c_str();
	strcpy(value, str);
	return true;
}
bool textStreamRead::GetData(int lineIndex, int index,unsigned int* value)
{
	const char* str = currentBlockDataLineValues[lineIndex][index].c_str();
	*value = stoul(str);
	return true;
}