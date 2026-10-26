#pragma once

#include <algorithm>
#include <bit>
#include <vector>
#include <string_view>
#include <array>
#include <random>
#include <cmath>
#include <ranges>
#include <numbers>

namespace mumble
{
	inline constexpr int SAMPLE_RATE = 44100;
	inline constexpr float LETTER_DELAY = 1.0f / 13.0f; // should be about 77ms
	inline constexpr float COMMA_DELAY = 3.0f * LETTER_DELAY;
	inline constexpr float FULL_STOP_DELAY = 2.0f * COMMA_DELAY;
	inline constexpr float BEEP_DURATION = 0.105f;
	// BEEP_DURATION should be longer than LETTER_DELAY so that the sounds can flow into each other but less than comma delay
	// so that punctuation still creates audible gaps
	// I just happen to like 0.105f after a few tries

	struct VoiceCfg
	{
		float basePitch; // Hz of the voice "hum"
		float formantScale; // "tube length", affects F1, F2 and F3 together
		float formantResonanceWidth; // The higher the resonance width the narrower the mouth
		bool elderWobble; // whether the voice is of an old person, adds a 2Hz wobble to the voice
		float vibratoDepth; // Hz, only used when elderly
		float jitter; // per beep pitch scatter, about +/- 2% feels human, 0 as noted in video is robotic
		float breath; // 0..1
		float gain;
	};

	enum class Vowel
	{
		Ah,
		Eh,
		Ee,
		Oh,
		Ooh,
		Uh
	};

	struct Formants
	{
		float f1;
		float f2;
		float f3;
	};

	inline constexpr std::array<Formants, 6> vowelTable{{ // These appear to be largely agreed upon for the vowels
		{730.f, 1090.f, 2440.f}, // Ah
		{530.f, 1840.f, 2480.f}, // Eh
		{270.f, 2290.f, 3010.f}, // Ee
		{570.f,  840.f, 2410.f}, // Oh
		{300.f,  870.f, 2240.f}, // Ooh
		{640.f, 1190.f, 2390.f} // Uh
	}};

	inline bool isVowel(char c)
	{
		c = char(std::tolower(static_cast<unsigned char>(c)));
		return c == 'a' || c == 'e'|| c == 'i' || c == 'o' || c == 'u'; // TODO: make this smarter (maybe syllable based) so that it can take into account 'y'
	}

	// vowel letters to vowel sounds
	// english spelling != phonetics so this is at best an approximation
	// TODO: use this as a fallback from a phoneme dictionary that uses words as keys directly
	inline Vowel vowelFor(std::string_view run)
	{
		const char c0 = run[0];
		const char c1 = run.size() > 1 ? run[1] : '\0';
		switch(c0)
		{
			case 'a':
			case 'A':
				return Vowel::Ah; // aw/au
			case 'e':
			case 'E':
				return (c1 == 'e' || c1 == 'a') ? Vowel::Ee : Vowel::Eh; // ee/ea
			case 'i':
			case 'I':
				return Vowel::Ee;
			case 'o':
			case 'O':
				return (c1 == 'o' || c1 == 'u') ? Vowel::Ooh : Vowel::Oh; // oo/ou
			case 'u':
			case 'U':
				return Vowel::Uh;
			default:
				return Vowel::Uh; // fallback
		}
	}

	inline uint64_t fnvHash(std::string_view sv)// Fowler–Noll–Vo hashing algorithm
	{
		uint64_t h = 469598103934665603ull;
		for(char c : sv)
		{
			h ^= static_cast<uint8_t>(c);
			h *= 1099511628211ull;
		}

		return h;
	}

	inline uint64_t hashVoice(const VoiceCfg& v)
	{
		uint64_t h = 1469598103934665603ull;
		auto mix = [&](uint32_t x)
		{
			h ^= x;
			h *= 1099511628211ull;
		};

		mix(std::bit_cast<uint32_t>(v.basePitch));
		mix(std::bit_cast<uint32_t>(v.formantScale));
		mix(std::bit_cast<uint32_t>(v.formantResonanceWidth));
		mix(std::bit_cast<uint32_t>(v.vibratoDepth));
		mix(std::bit_cast<uint32_t>(v.jitter));
		mix(std::bit_cast<uint32_t>(v.breath));
		mix(v.elderWobble ? 1u : 0u);

		return h;
	}

	struct Glyph
	{
		char ch;
		float revealTime;
		float duration;
		bool wordIsShouted;
	};

	inline bool isAlpha(char c)
	{
		c = char(std::tolower(static_cast<unsigned char>(c)));
		return c >= 'a' && c <= 'z';
	}

	inline std::vector<Glyph> layoutLine(std::string_view text)
	{
		// prepass for all all-caps words
		std::vector<bool> shout(text.size(), false);
		for(size_t i = 0; i < text.size(); )
		{
			if(!isAlpha(text[i]))
			{
				++i;
				continue;
			}

			size_t j = i;
			bool anyUpper = false;
			bool anyLower = false;

			while(j < text.size() && isAlpha(text[j]))
			{
				const char u = char(std::toupper(static_cast<unsigned char>(text[j])));
				anyUpper |= (text[j] == u);
				anyLower |= (text[j] != u);
				++j;
			}

			if(anyUpper && !anyLower && (j - i) >= 2) // to avoid a lone 'I' being flagged
			{
				for(size_t shoutPos = i; shoutPos < j; ++shoutPos)
				{
					shout[shoutPos] = true;
				}
			}

			i = j;
		}

		std::vector<Glyph> output;
		output.reserve(text.size());
		float t = 0.0f;

		for(size_t i = 0; i < text.size(); ++i)
		{
			const char c = text[i];
			float delay = LETTER_DELAY;

			if(c == ',')
			{
				delay = COMMA_DELAY;
			} else if(c == '.' || c == '?' || c == '!') // obviously this doesn't work in languages that use other punctuation
			{
				delay = FULL_STOP_DELAY;
			}
			// spaces don't have a special delay
			output.push_back({c, t, delay, shout[i]});
			t += delay;
		}

		return output;
	}

	struct Beep
	{
		float t0; // start time
		float duration; // BEEP_DURATION
		Vowel vowel;
		float pitch; // Hz (base * declination * jitter; afterwards any countour edits)
		float gain;
		size_t glyphIndex; // for the punctuation lookahead pass
	};

	struct LinePlan
	{
		std::vector<Beep> beeps;
		float length = 0.0f; // total duration with the trail included
	};

	inline LinePlan planBeeps(std::string_view text, const VoiceCfg &v)
	{
		const auto glyphs = layoutLine(text);
		std::mt19937_64 rng(fnvHash(text)^hashVoice(v));
		std::uniform_real_distribution<float> jitterDist(-v.jitter, v.jitter);

		LinePlan linePlan;
		float phraseStart = 0.0f;

		for(size_t i = 0; i < glyphs.size(); ++i)
		{
			const char c = glyphs[i].ch;

			if(isVowel(c) && (i == 0 || !isVowel(glyphs[i-1].ch)))
			{
				// take the whole mini-syllable so the rules in isVowel can catch stuff like "oo"
				size_t j = i;
				while(j < glyphs.size() && isVowel(glyphs[j].ch))
				{
					++j;
				}

				const Vowel vowel = vowelFor(text.substr(i, j - i));

				// lower pitch across a phrase then picks back up on the next one
				// it's bounded so that it doesn't eventually drop to an incoherent growl
				const float phraseT = glyphs[i].revealTime - phraseStart;
				const float decline = 1.0f - 0.15f * std::min(phraseT / 3.0f, 1.0f);
				float pitch = v.basePitch * decline;
				pitch *= 1.0f + jitterDist(rng); // add in a random jitter so it sounds human-y

				float gain = v.gain;
				if(glyphs[i].wordIsShouted) // word is all caps
				{
					gain *= 1.5f;
					pitch *= 1.04f; // shouting is supposed to sharpen/raise the pitch slightly; 4% may still be too much but it sounds fine to me
				}

				linePlan.beeps.push_back({glyphs[i].revealTime, BEEP_DURATION, vowel, pitch, gain, i});
			}

			if(c == ',' || c == '.' || c == '?' || c == '!')
			{
				phraseStart = glyphs[i].revealTime;
			}
		}

		// lookahead for the punctuation and beep changes
		auto previousBeepSince = [&](size_t before, size_t boundary) -> Beep*
		{
			Beep* r = nullptr;
			for(auto& b : linePlan.beeps)
			{
				if(b.glyphIndex < before && b.glyphIndex > boundary)
				{
					r = &b;
				}
			}
			return r;
		};

		auto nextBeepAfter = [&](size_t after) -> Beep*
		{
			for(auto& b : linePlan.beeps)
			{
				if(b.glyphIndex > after)
				{
					return &b;
				}
			}
			return nullptr;
		};

		size_t boundary = 0; // last comma or sentence end
		for(size_t i = 0; i < glyphs.size(); ++i)
		{
			const char c = glyphs[i].ch;
			const bool dotRunContinues = (c == '.'  && i+1 < glyphs.size() && glyphs[i+1].ch == '.');
			if(c == '.' && dotRunContinues) // only the last '.' is supposed to have an effect
			{
				continue;
			}

			if( c == '?' || c == '!' || c == '.')
			{
				if(c == '?')
				{
					if(Beep* b = previousBeepSince(i, boundary))
					{
						b->pitch *= 1.30f; // uptalk
					}
				} else if(c == '!')
				{
					if(Beep * b = nextBeepAfter(i)) // next phrase is supposed to start high
					{
						b->pitch *= 1.15f;
						b->gain *= 1.1f;
					}
				} else // '.'; check if this was an elipsis
				{
					size_t run = 1;
					while(i >= run && glyphs[i - run].ch == '.')
					{
						++run;
					}
					if(run >= 3) // fade voice
					{
						std::vector<Beep*> tail;
						for(auto& b : linePlan.beeps)
						{
							if(b.glyphIndex < i && b.glyphIndex > boundary)
							{
								tail.push_back(&b);
							}
						}
						const float scale[3] = {0.6f, 0.3f, 0.15f};
						for(size_t k = 0; k < std::min<size_t>(tail.size(), 3); ++k)
						{
							tail[tail.size()- 1 - k]->gain *= scale[k];
						}
					}
				}

				boundary = i;
			} else if(c == ',')
			{
				boundary = i;
			}
		}

		linePlan.length = glyphs.empty() ? 0.0f : glyphs.back().revealTime + glyphs.back().duration + 0.2f;

		return linePlan;
	}

	// RBJ biquad bandpass (constant 0 dB peak gain), transposed direct form II
	// more in depth reading at: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
	// TODO: precompute coefficients per (vowel × voice) into a cache
	struct Biquad
	{
		float b0 = 0;
		float b1 = 0;
		float b2 = 0;
		float a1 = 0;
		float a2 = 0;
		float x1 = 0;
		float x2 = 0;
		float y1 = 0;
		float y2 = 0;

		// frequency, q, frequancy sample
		// w0 is the centre frequency (since this is digital, not analog)
		static Biquad bandpass(float frequency, float q, float fs)
		{
			const float w0 = 2.0f * std::numbers::pi_v<float> * frequency/fs;
			const float cs = std::cos(w0);
			const float al = std::sin(w0) / (2.0f * q);

			Biquad b;
			// this is to reject frequencies far from the formant
			b.b0 = al;
			b.b1 = 0.0f; // any symmetric bandpass with nulls at both ends resolves to b1 = 0
			b.b2 = -al;

			b.a1 = -2.0f * cs; // this ensures the peak sits at the formant frequency
			b.a2 = 1.0f - al;

			const float a0 = 1.0f + al; // normalize everything to get the denominator monic and allows peak gain to be 0db
			b.b0 /= a0;
			b.b1 /= a0;
			b.b2 /= a0;
			b.a1 /= a0;
			b.a2 /= a0;
		
			return b;
		}

		float process(float x0) // Direct Form 2 (DF-II); can use just 2 state variables in stead of 4 for DF-I
		{
			const float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
			x2 = x1;
			x1 = x0;
			y2 = y1;
			y1 = y0;

			return y0;
		}
	};

	// PolyBLEP-corrected sawtooth
	// Online recommendations are for supersampling at double or quadruple, but no
	inline float polyBlep(float t, float dt)
	{
		if(t < dt)
		{
			const float u = t / dt;
			return u + u - u * u - 1.0f;
		}

		if(t > 1.0f - dt)
		{
			const float u = (t - 1.0f) / dt;
			return u * u + u + u + 1.0f;
		}

		return 0.0f;
	}

	inline float sawBlep(float phase, float dt)
	{
		return 2.0f * phase - 1.0f - polyBlep(phase, dt);
	}

	// xorshift noise for a "breath" component
	// make sure to keep this rng separate from the one in planBeeps so that the beeps don't correlate
	struct Xorshift
	{
		uint32_t s;
		float next()
		{
			s ^= s << 13;
			s ^= s >> 17;
			s ^= s << 5;
			return float(s >> 8) * (1.0f/8388608.f) - 1.0f;
		}
	};

	inline float envelope(float tau) // tau is supposed to be [0, 1]
	{
		if(tau < 0.12f)
		{
			return tau / 0.12f;
		}

		if(tau < 0.6f)
		{
			return 1.0f;
		}

		return std::max(0.0f, 1.0f - (tau - 0.6f) / 0.4f);
	}

	inline std::vector<float> renderLine(std::string_view text, const VoiceCfg& v, float fs = SAMPLE_RATE)
	{
		const LinePlan linePlan = planBeeps(text, v);
		std::vector<float> out(static_cast<size_t>((linePlan.length + 0.05f) * fs) + 1, 0.0f);

		if(out.size() < 2)
		{
			return out;
		}

		Xorshift noise{static_cast<uint32_t>(fnvHash(text) ^ 0x9E3779B9u)};
		
		for(const Beep& b : linePlan.beeps)
		{
			const Formants F = vowelTable[static_cast<int>(b.vowel)];
			// the formants must stay below Nyquist (sample frequency / 2) or the biquad warps
			// TODO: consider clamping?

			auto mk = [&](float f)
			{
				return Biquad::bandpass(std::min(f * v.formantScale, fs * 0.45f), v.formantResonanceWidth, fs);
			};

			auto F1 = mk(F.f1);
			auto F2 = mk(F.f2);
			auto F3 = mk(F.f3);

			const int start = static_cast<int>(b.t0 * fs);
			const int n = static_cast<int>(b.duration * fs);
			float phase = 0.0f;
			float lpNoise = 0.0f;
			for(int i = 0; i < n && start + i < static_cast<int>(out.size()); ++i)
			{
				const float tSec = (start + i) / fs;
				
				float pitch = b.pitch;
				
				// Elder wobble is a 2Hz sine on global time as resetting wobble per beep reads weird
				if(v.elderWobble)
				{
					pitch += v.vibratoDepth * std::sin(2.0f * std::numbers::pi_v<float> * 2.0f * tSec);
				}

				const float dt = pitch / fs;
				phase += dt;
				if(phase >= 1.0f)
				{
					phase -= 1.0f;
				}

				float x = sawBlep(phase, dt);
				const float nz = noise.next();
				lpNoise += 0.3f * (nz - lpNoise);
				x = x * (1.0f - v.breath) + lpNoise * (v.breath * 2.0f);

				// Parallel formant bank: each bandpass boosts its band; sum gives the vowel coloration.
				// if Q too high it rings like a bell
				// if q too low it sounds muffled and/or nasal 8 to about 12 should be a good range
				x = (F1.process(x) + F2.process(x) + F3.process(x)) * 0.4f;

				out[start + i] += x * envelope(i / static_cast<float>(n)) * b.gain;
			}
		}

		// overlapping beeps stack, normalizing to leave headroom and not deafen people
		// TODO: impl tanh soft-clip prior to normalization so it is better
		const float peak = std::ranges::max(out | std::views::transform([](float s){return std::fabs(s);}));

		if(peak > 1e-6f)
		{
			const float g = 0.9f / peak;
			for(auto& s : out)
			{
				s *= g;
			}
		}

		return out;
	}
}