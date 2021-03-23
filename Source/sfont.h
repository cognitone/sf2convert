///////////////////////////////////////////////////////////////////////////////
//
//  sf2convert
//  SoundFont Conversion/Compression Utility
//
//  Copyright (C)
//  2010 Werner Schweer and others (MuseScore)
//  2015 Davy Triponney (Polyphone)
//  2017 Cognitone (Juce port, converter)
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef __SOUNDFONT_H__
#define __SOUNDFONT_H__

#include "../JuceLibraryCode/JuceHeader.h"

// Disable this, if you don't want to use the Juce Vorbis code
#define USE_JUCE_VORBIS 1

// Enable this, if compression format is set individually per sample (not yet possible)
#define USE_MULTIPLE_COMPRESSION_FORMATS 0

// Disable this, if you don't want to fix invalid sampletype.
#define FIX_INVALID_SAMPLETYPE 1

#ifndef byte
typedef unsigned char byte;
#endif
#ifndef ushort
typedef unsigned short ushort;
#endif
#ifndef uint
typedef unsigned int uint;
#endif

/**
    @todo:
    - Port char pointers to Juce Strings
    - Proper destructor and leak detection
 */


namespace SF2 {

//---------------------------------------------------------
//   sfVersionTag
//---------------------------------------------------------

enum FileType
{
    SF2Format,
    SF3Format,
    SF4Format
};
    
struct sfVersionTag
{
    int major;
    int minor;
};

enum Modulator { };
    
enum Generator
{
    Gen_StartAddrOfs, Gen_EndAddrOfs, Gen_StartLoopAddrOfs,
    Gen_EndLoopAddrOfs, Gen_StartAddrCoarseOfs, Gen_ModLFO2Pitch,
    Gen_VibLFO2Pitch, Gen_ModEnv2Pitch, Gen_FilterFc, Gen_FilterQ,
    Gen_ModLFO2FilterFc, Gen_ModEnv2FilterFc, Gen_EndAddrCoarseOfs,
    Gen_ModLFO2Vol, Gen_Unused1, Gen_ChorusSend, Gen_ReverbSend, Gen_Pan,
    Gen_Unused2, Gen_Unused3, Gen_Unused4,
    Gen_ModLFODelay, Gen_ModLFOFreq, Gen_VibLFODelay, Gen_VibLFOFreq,
    Gen_ModEnvDelay, Gen_ModEnvAttack, Gen_ModEnvHold, Gen_ModEnvDecay,
    Gen_ModEnvSustain, Gen_ModEnvRelease, Gen_Key2ModEnvHold,
    Gen_Key2ModEnvDecay, Gen_VolEnvDelay, Gen_VolEnvAttack,
    Gen_VolEnvHold, Gen_VolEnvDecay, Gen_VolEnvSustain, Gen_VolEnvRelease,
    Gen_Key2VolEnvHold, Gen_Key2VolEnvDecay, Gen_Instrument,
    Gen_Reserved1, Gen_KeyRange, Gen_VelRange,
    Gen_StartLoopAddrCoarseOfs, Gen_Keynum, Gen_Velocity,
    Gen_Attenuation, Gen_Reserved2, Gen_EndLoopAddrCoarseOfs,
    Gen_CoarseTune, Gen_FineTune, Gen_SampleId, Gen_SampleModes,
    Gen_Reserved3, Gen_ScaleTune, Gen_ExclusiveClass, Gen_OverrideRootKey,
    Gen_Dummy
};

enum Transform
{
    Linear,
    AbsoluteValue = 2
};
    
/** Bit-masked SampleType, extended with flags 
    for compression (See SF2 spec for details) */
enum SampleType
{
    Mono        = 1,
    Right       = 2,
    Left        = 4,
    Linked      = 8,
    // Compression flags
    TypeVorbis  = 16,  // compatible with FluidSynth/MuseScore
    TypeFlac    = 32,
    // ROM sample flag
    Rom         = 0x8000
};
    
enum SampleCompression
{
    Raw,
    Vorbis,
    Flac
};
    

//---------------------------------------------------------
//   ModulatorList
//---------------------------------------------------------

class ModulatorList
{
public:
    ModulatorList () { zerostruct(*this); };
   ~ModulatorList () {};
    
    Modulator src;
    Generator dst;
    int amount;
    Modulator amtSrc;
    Transform transform;
    
    JUCE_LEAK_DETECTOR (ModulatorList);
};

//---------------------------------------------------------
//   GeneratorList
//---------------------------------------------------------

union GeneratorAmount
{
    short sword;
    ushort uword;
    struct {
        byte lo, hi;
    };
};

class GeneratorList
{
public:
    GeneratorList () { zerostruct(*this); };
   ~GeneratorList () {};
    
    Generator gen;
    GeneratorAmount amount;
    
    JUCE_LEAK_DETECTOR (GeneratorList);
};

//---------------------------------------------------------
//   Zone
//---------------------------------------------------------

class Zone
{
public:
    int instrumentIndex;
    OwnedArray<GeneratorList> generators;
    OwnedArray<ModulatorList> modulators;
    
    JUCE_LEAK_DETECTOR (Zone);
};

//---------------------------------------------------------
//   Preset
//---------------------------------------------------------

class Preset
{
public:
    Preset() : name(0), preset(0), bank(0), presetBagNdx(0), library(0), genre(0), morphology(0) {};
   ~Preset() {};
    
    String name;
    int preset;
    int bank;
    int presetBagNdx; // used only for read
    int library;
    int genre;
    int morphology;
    
    OwnedArray<Zone> zones;

    JUCE_LEAK_DETECTOR (Preset);
};

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

class Instrument
{
public:
    Instrument() : index(0), name() {};
   ~Instrument() {};
    
    int index;        // used only for reading
    String name;
    OwnedArray<Zone> zones;
    
    JUCE_LEAK_DETECTOR (Instrument);
};

    
//---------------------------------------------------------
//   Sample
//---------------------------------------------------------
    
/** Optional meta data for verification of samples after decompression */

class SampleMeta
{
public:
    SampleMeta() : name (), samples(0), loopstart(0), loopend(0) {};
   ~SampleMeta() {};
    
    String name;
    uint samples;   // original number of samples
    uint loopstart; // Relative
    uint loopend;
    
    JUCE_LEAK_DETECTOR (SampleMeta);
};

// Size in bytes for file positioning - critical
#define SampleMetaSize 32

/** Offsets start/end are absolute from start of chunk, measured in
    samples or bytes (depending on compression format). Loop points
    are absolute in the file (SF2 only), but turn into relative offsets 
    from start after loaded into RAM. This is to support Vorbis and Flac 
    compression, which unpredictably changes offsets in the file. */
    
class Sample
{
public:
     Sample();
    ~Sample();

    int numSamples() const;
    
    SampleCompression getCompressionType();
    void setCompressionType (SampleCompression c);
    void dropSampleData();
    void dropByteData();
    SampleMeta* createMeta();
    bool checkMeta();
    
    String name;
    uint start;
    uint end;
    uint loopstart;
    uint loopend;
    uint samplerate;
    int origpitch;
    int pitchadj;
    int sampleLink;
    int sampletype;
    // Raw byte data, used for compression i/o
    int byteDataSize;
    byte * byteData;
    // Native SF2 sample data, after decompression
    int sampleDataSize;
    short * sampleData;
    
    ScopedPointer<SampleMeta> meta;
    
    JUCE_LEAK_DETECTOR (Sample);
};
    


//---------------------------------------------------------
//   SoundFont
//---------------------------------------------------------

class SoundFont
{
public:

#if ! USE_JUCE_VORBIS
    /** This is a hack to simplify static Ogg callbacks for decoding */
    struct CallbackData {
        Sample* decodeSample;
        int     decodePosition;
    };
#endif
    
    SoundFont (const File filename);
   ~SoundFont ();
    
    bool read();
    bool write(const File filename, FileType format, int quality);
    void dumpPresets();
    void log(const String message);
    
    
private:
    
    unsigned readDword();
    int readWord();
    int readShort();
    int readByte();
    int readChar();
    int readFourcc (const char* signature);
    int readFourcc (char* signature);
    void readSignature (const char* signature);
    void readSignature (char* signature);
    void skip (int n);
    void readSection (const char* fourcc, int len);
    void readVersion();
    
    String readString (int n);
    
    void readPhdr (int len);
    void readBag (int, Array<Zone*>* zones);
    void readMod (int, Array<Zone*>* zones);
    void readGen (int, Array<Zone*>* zones);
    void readInst (int size);
    void readShdr (int size);
    
    /** 
     Non-standard extension: This optional chunk retains information on original 
     sample lengths & loops for later verification of a compressed file.
     */
    void readShdX (int size);
    
    int readSampleData (Sample* s);
    int readSampleDataRaw (Sample* s);
    int readSampleDataVorbis (Sample* s);
    int readSampleDataFlac (Sample* s);

    void writeDword (int val);
    void writeWord (unsigned short int val);
    void writeByte (unsigned char val);
    void writeChar (char val);
    void writeShort (short val);
    void write (const char* p, int n);
    void writeString (const String& string, size_t size);
    void writeStringSection (const char* fourcc, const String& string);
    void writePreset (int zoneIdx, const Preset* preset);
    void writeModulator (const ModulatorList* m);
    void writeGenerator (const GeneratorList* g);
    void writeInstrument (int zoneIdx, const Instrument* instrument);

    void writeIfil();
    void writeSmpl (int quality);
    void writePhdr();
    void writeBag (const char* fourcc, Array<Zone*>* zones);
    void writeMod (const char* fourcc, const Array<Zone*>* zones);
    void writeGen (const char* fourcc, Array<Zone*>* zones);
    void writeInst();
    void writeShdr();
    void writeShdrEach (const Sample* s);
    
    void writeShdX();
    void writeShdXEach (const SampleMeta* m);

    int writeSampleDataPlain (Sample* s);
    int writeSampleDataVorbis (Sample* s, int quality);
    int writeSampleDataFlac (Sample* s, int quality);
    
    bool writeCSample (Sample*, int idx);
    
#if FIX_INVALID_SAMPLETYPE
    void fixSampleType();
#endif

#if ! USE_JUCE_VORBIS
    bool decodeOggVorbis (Sample* s);
#endif

protected:
    /** You may want to access these from your code, so make it a friend class */
    
    OwnedArray<Preset>      _presets;
    OwnedArray<Instrument>  _instruments;
    OwnedArray<Sample>      _samples;

private:
    File _path;
    sfVersionTag _version;
    
    String _engine;
    String _name;
    String _date;
    String _comment;
    String _tools;
    String _creator;
    String _product;
    String _copyright;
    
    int64 _samplePos;
    int64 _sampleLen;
    
    FileInputStream* _infile;   // should be a WeakReference, actually
    FileOutputStream* _outfile; // should be a WeakReference, actually

    FileType _fileFormatIn, _fileFormatOut;
    int64 _fileSizeIn, _fileSizeOut;
    
    AudioFormatManager _manager;
    OggVorbisAudioFormat* _audioFormatVorbis;
    FlacAudioFormat* _audioFormatFlac;
    StringArray _qualityOptionsVorbis;
    StringArray _qualityOptionsFlac;
    
    Array<Zone*> _pZones; // owned by _presets after loading
    Array<Zone*> _iZones; // owned by _instruments after loading
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFont);
};
    
} // namespace




#endif

