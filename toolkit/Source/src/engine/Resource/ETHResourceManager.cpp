/*--------------------------------------------------------------------------------------
 Ethanon Engine (C) Copyright 2008-2012 Andre Santee
 http://www.asantee.net/ethanon/

	Permission is hereby granted, free of charge, to any person obtaining a copy of this
	software and associated documentation files (the "Software"), to deal in the
	Software without restriction, including without limitation the rights to use, copy,
	modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so, subject to the
	following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
	INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
	PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
	OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
--------------------------------------------------------------------------------------*/

#include "ETHResourceManager.h"
#include <Platform/Platform.h>

#include "ETHResourceProvider.h"

static str_type::string RemoveResourceDirectory(
	const str_type::string& resourceDirectory,
	const str_type::string& fullOriginPath)
{
	str_type::string r = fullOriginPath, fixedResourceDirectory = resourceDirectory;
	Platform::FixSlashes(fixedResourceDirectory);
	if (r.find(fixedResourceDirectory) == 0)
		r = r.substr(fixedResourceDirectory.length(), str_type::string::npos);
	return r;
}

ETHGraphicResourceManager::SpriteResource::SpriteResource(
	const str_type::string& resourceDirectory,
	const str_type::string& fullOriginPath,
	const SpritePtr& sprite) :
	m_sprite(sprite),
	m_fullOriginPath(RemoveResourceDirectory(resourceDirectory, fullOriginPath))
{
}

void ETHGraphicResourceManager::ReleaseResources()
{
	m_resource.clear();
}

ETHGraphicResourceManager::ETHGraphicResourceManager(const ETHSpriteDensityManager& densityManager) :
	m_densityManager(densityManager)
{
}

SpritePtr ETHGraphicResourceManager::GetPointer(
	VideoPtr video,
	const str_type::string &fileRelativePath, const str_type::string &resourceDirectory,
	const str_type::string &searchPath, const bool cutOutBlackPixels)
{
	if (fileRelativePath == GS_L(""))
		return SpritePtr();

	str_type::string fileName = Platform::GetFileName(fileRelativePath);
	str_type::string resourceFullPath = AssembleResourceFullPath(resourceDirectory, searchPath, fileName);

	if (!m_resource.empty())
	{
		SpritePtr sprite = FindSprite(resourceFullPath, fileName, resourceDirectory);
		if (sprite)
		{
			return sprite;
		}
	}

	// we can set a search path to search the file in case
	// it hasn't been loaded yet
	if (searchPath != GS_L(""))
	{
		AddFile(video, resourceFullPath, resourceDirectory, cutOutBlackPixels);
		return FindSprite(resourceFullPath, fileName, resourceDirectory);
	}
	return SpritePtr();
}

SpritePtr ETHGraphicResourceManager::AddFile(
	VideoPtr video,
	const str_type::string &path,
	const str_type::string& resourceDirectory,
	const bool cutOutBlackPixels)
{
	str_type::string fileName = Platform::GetFileName(path);
	{
		SpritePtr sprite = FindSprite(path, fileName, resourceDirectory);
		if (sprite)
			return sprite;
	}

	SpritePtr pBitmap;
	str_type::string fixedName(path);
	Platform::FixSlashes(fixedName);

	ETHSpriteDensityManager::DENSITY_LEVEL densityLevel;
	const str_type::string finalFileName(m_densityManager.ChooseSpriteVersion(fixedName, video, densityLevel));

	if (!(pBitmap = video->CreateSprite(finalFileName, (cutOutBlackPixels)? 0xFF000000 : 0xFFFF00FF)))
	{
		pBitmap.reset();
		ETH_STREAM_DECL(ss) << GS_L("(Not loaded) ") << path;
		ETHResourceProvider::Log(ss.str(), Platform::Logger::ERROR);
		return SpritePtr();
	}

	m_densityManager.SetSpriteDensity(pBitmap, densityLevel);

	//#if defined(_DEBUG) || defined(DEBUG)
	ETH_STREAM_DECL(ss) << GS_L("(Loaded) ") << fileName;
	ETHResourceProvider::Log(ss.str(), Platform::Logger::INFO);
	//#endif
	m_resource.insert(std::pair<str_type::string, SpriteResource>(fileName, SpriteResource(resourceDirectory, fixedName, pBitmap)));
	return pBitmap;
}

int ETHGraphicResourceManager::GetNumResources()
{
	return m_resource.size();
}

gs2d::SpritePtr ETHGraphicResourceManager::FindSprite(
	const str_type::string& fullFilePath,
	const str_type::string& fileName,
	const str_type::string& resourceDirectory)
{
	std::map<str_type::string, SpriteResource>::iterator iter = m_resource.find(fileName);
	if (iter != m_resource.end())
	{
		str_type::string fixedPath(fullFilePath);
		Platform::FixSlashes(fixedPath);
		const SpriteResource& resource = iter->second;
		if (RemoveResourceDirectory(resourceDirectory, fixedPath) != resource.m_fullOriginPath)
		{
			str_type::stringstream ss; ss << GS_L("Duplicate resource name found: ") << fixedPath
				<< GS_L(" <-> ") << resource.m_fullOriginPath;
			ETHResourceProvider::Log(ss.str(), Platform::Logger::ERROR);
		}
		return resource.m_sprite;
	}
	else
	{
		return SpritePtr();
	}
}

str_type::string ETHGraphicResourceManager::AssembleResourceFullPath(
	const str_type::string& programPath,
	const str_type::string& searchPath,
	const str_type::string& fileName )
{
	return programPath + searchPath + fileName;
}

void ETHGraphicResourceManager::RemoveResource(const str_type::string &file)
{
	std::map<str_type::string, SpriteResource>::iterator iter = m_resource.find(Platform::GetFileName(file));
	if (iter != m_resource.end())
	{
		m_resource.erase(iter);
	}
}

void ETHAudioResourceManager::ReleaseResources()
{
	ReleaseAllButMusic();
}

void ETHAudioResourceManager::ReleaseAllButMusic()
{
	std::map<str_type::string, AudioSamplePtr> musicTracks;
	for (std::map<str_type::string, AudioSamplePtr>::iterator iter = m_resource.begin(); iter != m_resource.end(); ++iter)
	{
		if ((iter->second)->GetType() == Audio::MUSIC)
			musicTracks[iter->first] = iter->second;
	}
	m_resource.clear(); // probably just paranoia
	m_resource = musicTracks;
}

AudioSamplePtr ETHAudioResourceManager::GetPointer(
	AudioPtr audio,
	const Platform::FileIOHubPtr& fileIOHub,
	const str_type::string &fileRelativePath,
	const str_type::string &searchPath,
	const Audio::SAMPLE_TYPE type)
{
	if (fileRelativePath == GS_L(""))
		return AudioSamplePtr();

	if (!m_resource.empty())
	{
		str_type::string fileName = Platform::GetFileName(fileRelativePath);
		std::map<str_type::string, AudioSamplePtr>::iterator iter = m_resource.find(fileName);
		if (iter != m_resource.end())
			return iter->second;
	}

	// we can set a search path to search the file in case
	// it hasn't been loaded yet
	if (searchPath != GS_L(""))
	{
		str_type::string fileName = Platform::GetFileName(fileRelativePath);

		str_type::string path = fileIOHub->GetResourceDirectory();
		path += searchPath;
		path += fileName;
		AddFile(audio, fileIOHub, path, type);
		return GetPointer(audio, fileIOHub, fileName, GS_L(""), type);
	}
	return AudioSamplePtr();
}

AudioSamplePtr ETHAudioResourceManager::AddFile(
	AudioPtr audio,
	const Platform::FileIOHubPtr& fileIOHub,
	const str_type::string& path,
	const Audio::SAMPLE_TYPE type)
{
	if (!m_resource.empty())
	{
		str_type::string fileName = Platform::GetFileName(path);
		std::map<str_type::string, AudioSamplePtr>::iterator iter = m_resource.find(fileName);
		if (iter != m_resource.end())
			return iter->second;
	}

	AudioSamplePtr pSample;
	str_type::string fixedName(path);
	Platform::FixSlashes(fixedName);
	if (!(pSample = audio->LoadSampleFromFile(fixedName, fileIOHub->GetFileManager(), type)))
	{
		pSample.reset();
		ETH_STREAM_DECL(ss) << GS_L("(Not loaded) \"") << fixedName << GS_L("\"");
		ETHResourceProvider::Log(ss.str(), Platform::Logger::ERROR);
		return AudioSamplePtr();
	}
	//#if defined(_DEBUG) || defined(DEBUG)
	str_type::string fileName = Platform::GetFileName(path);
	ETH_STREAM_DECL(ss) << GS_L("(Loaded) ") << fileName;
	ETHResourceProvider::Log(ss.str(), Platform::Logger::INFO);
	//#endif
	m_resource[fileName] = pSample;
	return pSample;
}

int ETHAudioResourceManager::GetNumResources()
{
	return m_resource.size();
}
