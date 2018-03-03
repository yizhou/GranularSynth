#include <stdio.h>
#include <memory.h>
#include <inttypes.h>
#include <vector>
 
// typedefs
typedef uint16_t    uint16;
typedef uint32_t    uint32;
typedef int32_t     int32;
 
//this struct is the minimal required header data for a wav file
struct SMinimalWaveFileHeader
{
    //the main chunk
    unsigned char m_chunkID[4];
    uint32        m_chunkSize;
    unsigned char m_format[4];
 
    //sub chunk 1 "fmt "
    unsigned char m_subChunk1ID[4];
    uint32        m_subChunk1Size;
    uint16        m_audioFormat;
    uint16        m_numChannels;
    uint32        m_sampleRate;
    uint32        m_byteRate;
    uint16        m_blockAlign;
    uint16        m_bitsPerSample;
 
    //sub chunk 2 "data"
    unsigned char m_subChunk2ID[4];
    uint32        m_subChunk2Size;
 
    //then comes the data!
};


inline void FloatToPCM(unsigned char *PCM, const float& in, size_t numBytes)
{
    // if a negative number
    uint32 data;
    if (in < 0.0f)
        data = uint32(in * float(0x80000000));
    else
        data = uint32(in * float(0x7fffffff));

    // TODO: make numBytes a template param when doing it in a block
    switch (numBytes)
    {
        case 4: PCM[3] = ((data >> 24) & 0xFF); PCM[2] = ((data >> 16) & 0xFF); PCM[1] = ((data >> 8) & 0xFF); PCM[0] = (data & 0xFF); break;
        case 3: PCM[2] = ((data >> 24) & 0xFF); PCM[1] = ((data >> 16) & 0xFF); PCM[0] = ((data >> 8) & 0xFF); break;
        case 2: PCM[1] = ((data >> 24) & 0xFF); PCM[0] = ((data >> 16) & 0xFF); break; 
        case 1: PCM[0] = ((data >> 24) & 0xFF); break;
    }
}

inline void PCMToFloat(float& out, const unsigned char *PCM, size_t numBytes)
{
    // TODO: make numBytes a template param when doing it in a block
    uint32 data = 0;
    switch (numBytes)
    {
        case 4: data = (uint32(PCM[3]) << 24) | (uint32(PCM[2]) << 16) | (uint32(PCM[1]) << 8) | uint32(PCM[0]); break;
        case 3: data = (uint32(PCM[2]) << 24) | (uint32(PCM[1]) << 16) | (uint32(PCM[0]) << 8); break;
        case 2: data = (uint32(PCM[1]) << 24) | (uint32(PCM[0]) << 16); break;
        case 1: data = (uint32(PCM[0]) << 24); break;
    }

    // if a negative number
    if (data & 0x80000000)
        out = float(int32(data)) / float(0x80000000);
    else
        out = float(data) / float(0x7fffffff);
}

void FloatToPCM (const std::vector<float>& in, std::vector<int32>& out)
{
    out.resize(in.size());
    for (size_t i = 0; i < in.size(); ++i)
        FloatToPCM((unsigned char*)&out[i], in[i], 4);        
}
 
bool WriteWaveFile(const char *fileName, std::vector<float>& dataFloat, uint16 numChannels, uint32 sampleRate)
{
    std::vector<int32> data;
    FloatToPCM(dataFloat, data);

    uint32 dataSize = (uint32)(data.size() * sizeof(int32));
    uint16 bitsPerSample = sizeof(int32) * 8;
 
    //open the file if we can
    FILE *File = nullptr;
    fopen_s(&File, fileName, "w+b");
    if (!File)
        return false;
 
    SMinimalWaveFileHeader waveHeader;
 
    //fill out the main chunk
    memcpy(waveHeader.m_chunkID, "RIFF", 4);
    waveHeader.m_chunkSize = dataSize + 36;
    memcpy(waveHeader.m_format, "WAVE", 4);
 
    //fill out sub chunk 1 "fmt "
    memcpy(waveHeader.m_subChunk1ID, "fmt ", 4);
    waveHeader.m_subChunk1Size = 16;
    waveHeader.m_audioFormat = 1;
    waveHeader.m_numChannels = numChannels;
    waveHeader.m_sampleRate = sampleRate;
    waveHeader.m_byteRate = sampleRate * numChannels * bitsPerSample / 8;
    waveHeader.m_blockAlign = numChannels * bitsPerSample / 8;
    waveHeader.m_bitsPerSample = bitsPerSample;
 
    //fill out sub chunk 2 "data"
    memcpy(waveHeader.m_subChunk2ID, "data", 4);
    waveHeader.m_subChunk2Size = dataSize;
 
    //write the header
    fwrite(&waveHeader, sizeof(SMinimalWaveFileHeader), 1, File);
 
    //write the wave data itself
    fwrite(&data[0], dataSize, 1, File);
 
    //close the file and return success
    fclose(File);
    return true;
}

bool ReadWaveFile(const char *fileName, std::vector<float>& data, uint16& numChannels, uint32& sampleRate)
{
	//open the file if we can
    FILE *file = nullptr;
    fopen_s(&file, fileName, "rb");
	if(!file)
	{
		return false;
	}

	//read the main chunk ID and make sure it's "RIFF"
	char buffer[5];
	buffer[4] = 0;
	if(fread(buffer,4,1,file) != 1 || strcmp(buffer,"RIFF"))
	{
		fclose(file);
		return false;
	}

	//read the main chunk size
	uint32 chunkSize;
	if(fread(&chunkSize,4,1,file) != 1)
	{
		fclose(file);
		return false;
	}

	//read the format and make sure it's "WAVE"
	if(fread(buffer,4,1, file) != 1 || strcmp(buffer,"WAVE"))
	{
		fclose(file);
		return false;
	}

	long chunkPosFmt = -1;
	long chunkPosData = -1;

	while(chunkPosFmt == -1 || chunkPosData == -1)
	{
		//read a sub chunk id and a chunk size if we can
		if(fread(buffer,4,1, file) != 1 || fread(&chunkSize,4,1, file) != 1)
		{
			fclose(file);
			return false;
		}

		//if we hit a fmt
		if(!strcmp(buffer,"fmt "))
		{
			chunkPosFmt = ftell(file) - 8;
		}
		//else if we hit a data
		else if(!strcmp(buffer,"data"))
		{
			chunkPosData = ftell(file) - 8;
		}

		//skip to the next chunk
		fseek(file,chunkSize,SEEK_CUR);
	}

	//we'll use this handy struct to load in 
	SMinimalWaveFileHeader waveData;

	//load the fmt part if we can
	fseek(file,chunkPosFmt,SEEK_SET);
	if(fread(&waveData.m_subChunk1ID,24,1, file) != 1)
	{
		fclose(file);
		return false;
	}

	//load the data part if we can
	fseek(file,chunkPosData,SEEK_SET);
	if(fread(&waveData.m_subChunk2ID,8,1, file) != 1)
	{
		fclose(file);
		return false;
	}

	//verify a couple things about the file data
	if(waveData.m_audioFormat != 1 ||       //only pcm data
	   waveData.m_numChannels < 1 ||        //must have a channel
	   waveData.m_numChannels > 2 ||        //must not have more than 2
	   waveData.m_bitsPerSample > 32 ||     //32 bits per sample max
	   waveData.m_bitsPerSample % 8 != 0 || //must be a multiple of 8 bites
	   waveData.m_blockAlign > 8)           //blocks must be 8 bytes or lower
	{
		fclose(file);
		return false;
	}

	//figure out how many samples and blocks there are total in the source data
	int bytesPerBlock = waveData.m_blockAlign;
	int numBlocks = waveData.m_subChunk2Size / bytesPerBlock;
	int numSourceSamples = numBlocks * waveData.m_numChannels;

	//allocate space for the source samples
    data.resize(numSourceSamples);

	//maximum size of a block is 8 bytes.  4 bytes per samples, 2 channels
	unsigned char blockData[8];
	memset(blockData,0,8);

	//read in the source samples at whatever sample rate / number of channels it might be in
	int bytesPerSample = bytesPerBlock / waveData.m_numChannels;
	for(int nIndex = 0; nIndex < numSourceSamples; nIndex += waveData.m_numChannels)
	{
		//read in a block
		if(fread(blockData,waveData.m_blockAlign,1, file) != 1)
		{
			fclose(file);
			return false;
		}

		//get the first sample
        PCMToFloat(data[nIndex], blockData, bytesPerSample);

        if (data[nIndex] < -1.0f || data[nIndex] > 1.0f)
        {
            int ijkl = 0;
        }

		//get the second sample if there is one
		if(waveData.m_numChannels == 2)
		{
            PCMToFloat(data[nIndex + 1] , &blockData[bytesPerSample], bytesPerSample);

            if (data[nIndex] < -1.0f || data[nIndex] > 1.0f)
            {
                int ijkl = 0;
            }
		}
	}

	//return our data
    numChannels = waveData.m_numChannels;
    sampleRate = waveData.m_sampleRate;
    fclose(file);
	return true;
}
 
//the entry point of our application
int main(int argc, char **argv)
{
    uint16 numChannels;
    uint32 sampleRate;
    std::vector<float> legend1;
    ReadWaveFile("legend1.wav", legend1, numChannels, sampleRate);

    float maxAbsVal = 0.0f;
    float maxValue = 0.0f;
    for (float f : legend1)
    {
        if (std::fabsf(f) > maxAbsVal)
        {
            maxAbsVal = std::fabsf(f);
            maxValue = f;
        }
    }


    WriteWaveFile("out.wav", legend1, numChannels, sampleRate);

    /*
    // generate the mono beating effect
    {
        // sound format parameters
        const int c_sampleRate = 44100;
        const int c_numSeconds = 4;
        const int c_numChannels = 1;
        const int c_numSamples = c_sampleRate * c_numChannels * c_numSeconds;
 
        // make space for our samples
        std::vector<float> samples;
        samples.resize(c_numSamples);
 
        // generate samples
        GenerateMonoBeatingSamples(samples, c_sampleRate);
 
        // convert from float to the final format
        std::vector<int32> samplesInt;
        ConvertFloatSamples(samples, samplesInt);
 
        // write our samples to a wave file
        WriteWaveFile("monobeat.wav", samplesInt, c_numChannels, c_sampleRate);
    }
 
    // generate the stereo beating effect (binaural beat)
    {
        // sound format parameters
        const int c_sampleRate = 44100;
        const int c_numSeconds = 4;
        const int c_numChannels = 2;
        const int c_numSamples = c_sampleRate * c_numChannels * c_numSeconds;
 
        // make space for our samples
        std::vector<float> samples;
        samples.resize(c_numSamples);
 
        // generate samples
        GenerateStereoBeatingSamples(samples, c_sampleRate);
 
        // convert from float to the final format
        std::vector<int32> samplesInt;
        ConvertFloatSamples(samples, samplesInt);
 
        // write our samples to a wave file
        WriteWaveFile("stereobeat.wav", samplesInt, c_numChannels, c_sampleRate);
    }
    */
}

/*

TODO:

* this stuff needs cleaning up big time :P

* fread the entire file in at once?

* test 1, 2, 3, 4, byte formats. test their round trips too!

* actually, maybe start with source code for "lament of tim curry"
 * https://blog.demofox.org/2012/06/18/diy-synth-3-sampling-mixing-and-band-limited-wave-forms/

 * build w32 and x64

* use granular synthesis to stretch and squish a sound without affecting frequency
* also, adjust frequency without affecting length
* auto tune? with note sliding

* compare using zero crossings, cubic interpolation, and envelopes

BLOG:

* Explain algorithm, give results, link to code and also include it!

*/