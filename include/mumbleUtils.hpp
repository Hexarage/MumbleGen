#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <mumble.hpp>

namespace mumble
{

	namespace preset{
		// presets that can be used and sound good
		inline constexpr VoiceCfg man {95.f, 0.85f,  9.f, false, 0.f, 0.020f, 0.08f, 0.90f};
		inline constexpr VoiceCfg woman {200.f, 1.20f, 10.f, false, 0.f, 0.020f, 0.07f, 0.80f};
		inline constexpr VoiceCfg child {280.f, 1.45f, 11.f, false, 0.f, 0.025f, 0.07f, 0.75f};
		inline constexpr VoiceCfg pirate {90.f, 0.75f, 25.f, false, 0.f, 0.030f, 0.12f, 0.90f};
		inline constexpr VoiceCfg elderMan {130.f, 0.90f,  9.f, true,  5.f, 0.020f, 0.08f, 0.85f};
	}

    bool writeWavToFile(const std::vector<float>& data, const float sampleRate = SAMPLE_RATE, std::string_view path = "demo.wav")
    {
	    std::ofstream writeFile(path.data(), std::ios::binary);
	    if(!writeFile)
	    {
	    	std::clog << "Couldn't open \"" << path.data() << "\"\n";
	    	return false;
	    }

	    const uint32_t dataBytes = static_cast<uint32_t>(data.size() * 2);
    
	    auto u32w = [&](uint32_t x){ writeFile.write(reinterpret_cast<const char*>(&x), 4); };
	    auto u16w = [&](uint16_t x){ writeFile.write(reinterpret_cast<const char*>(&x), 2); };

	    writeFile.write("RIFF", 4); // https://en.wikipedia.org/wiki/WAV#WAV_file_header
	    u32w(36 + dataBytes);
	    writeFile.write("WAVE", 4);
	    writeFile.write("fmt ", 4);
	    u32w(16);
	    u16w(1);
	    u16w(1); // PCM Mono, not bothering with something more complex at the moment
	    u32w(static_cast<uint32_t>(sampleRate));
	    u32w(static_cast<uint32_t>(sampleRate) * 2);
	    u16w(2);
	    u16w(16);
	    writeFile.write("data", 4);
	    u32w(dataBytes);
	    for(const float& sample: data)
	    {
            const long v = std::lround(std::clamp(sample, -1.f, 1.f) * 32767.f); // wav is int based, not float based
	    	int16_t x = static_cast<int16_t>(v);
	    	writeFile.write(reinterpret_cast<const char*>(&x), 2);
	    }

	    return true;
    }
}