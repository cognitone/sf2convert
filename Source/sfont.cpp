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

#include "sfont.h"

#if ! USE_JUCE_VORBIS
#include "juce_audio_formats/codecs/oggvorbis/codec.h"
#include "juce_audio_formats/codecs/oggvorbis/vorbisenc.h"
#include "juce_audio_formats/codecs/oggvorbis/vorbisfile.h"
#endif

using namespace SF2;

#define FOURCC(a, b, c, d) a << 24 | b << 16 | c << 8 | d

#define BLOCK_SIZE 1024

//---------------------------------------------------------
//   Sample
//---------------------------------------------------------

Sample::Sample() :
    name(),
    start(0),
    end(0),
    loopstart(0),
    loopend(0),
    samplerate(0),
    origpitch(0),
    pitchadj(0),
    sampleLink(0),
    sampletype(SampleType::Mono),
    byteDataSize(0),
    byteData(nullptr),
    sampleDataSize(0),
    sampleData(nullptr),
    meta()
{
    // All members are required to be all-zero, for a clean Sample instance is used as terminator in shdr chunk!
}

Sample::~Sample()
{
    dropSampleData();
    dropByteData();
}

void Sample::dropByteData()
{
    free(byteData);
    byteData = nullptr;
    byteDataSize = 0;
}

void Sample::dropSampleData()
{
    free(sampleData);
    sampleData = nullptr;
    sampleDataSize = 0;
}

SampleCompression Sample::getCompressionType()
{
    if ((sampletype & SampleType::TypeVorbis) > 0) return SampleCompression::Vorbis;
    if ((sampletype & SampleType::TypeFlac) > 0)   return SampleCompression::Flac;
    return SampleCompression::Raw;
}

void Sample::setCompressionType (SampleCompression c)
{
    switch (c)
    {
        case SampleCompression::Vorbis :
            sampletype &= ~((int)(SampleType::TypeVorbis + SampleType::TypeFlac));
            sampletype |= (int)SampleType::TypeVorbis;
            break;
        case SampleCompression::Flac :
            sampletype &= ~((int)(SampleType::TypeVorbis + SampleType::TypeFlac));
            sampletype |= (int)SampleType::TypeFlac;
            break;
        case SampleCompression::Raw :
            sampletype &= ~((int)(SampleType::TypeVorbis + SampleType::TypeFlac));
            break;
    }
}

/** 
 Getting the number of samples is a bit shaky, because this is
 derived from start/end offsets, which are subject to change when 
 written to a file that uses compression. Luckily, once the sample 
 data was loaded (and/or decompressed), we know for sure.
 */
int Sample::numSamples() const
{
    if (sampleData)
        return sampleDataSize;
    else
        return end - start;
}

/** 
 Since sample & loop offsets are 'repurposed' in compressed files, 
 this optional meta data preserves them, so we can verify if 
 decompression worked properly after load.
 */
SampleMeta* Sample::createMeta()
{
    meta = new SampleMeta();
    meta->name = name;
    meta->samples = numSamples();
    meta->loopstart = loopstart;
    meta->loopend = loopend;
    return meta;
}

/** 
 Verify if sample was properly restored after decompression.
 */
bool Sample::checkMeta()
{
    if (meta == nullptr)
        return true;
    
    return meta->samples == numSamples()
        && (meta->loopend - meta->loopstart) == (loopend - loopstart);
}


//---------------------------------------------------------
//   SoundFont
//---------------------------------------------------------

SoundFont::SoundFont (const File filename) :
    _path(filename),
    _engine(),
    _name(),
    _date(),
    _comment(),
    _tools(),
    _creator(),
    _product(),
    _copyright(),
    _infile(nullptr),
    _outfile(nullptr),
    _fileFormatIn(SF2Format),
    _fileFormatOut(SF2Format),
    _fileSizeIn(0),
    _fileSizeOut(0),
    _manager()
{
    _manager.registerBasicFormats();
    _audioFormatVorbis = dynamic_cast<OggVorbisAudioFormat*> (_manager.findFormatForFileExtension("ogg"));
    _audioFormatFlac   = dynamic_cast<FlacAudioFormat*> (_manager.findFormatForFileExtension("flac"));
    
    jassert (_audioFormatVorbis != nullptr);
    jassert (_audioFormatFlac != nullptr);
    
    _qualityOptionsVorbis = _audioFormatVorbis->getQualityOptions();
    _qualityOptionsFlac   = _audioFormatFlac->getQualityOptions();
    
    /** DEBUG: Use this snippet to learn about quality options */
    /*
    log("Vorbis");
    for (int i=0; i < _qualityOptionsVorbis.size(); i++)
        log (String(i) + ": " + _qualityOptionsVorbis[i]);
    log("FLAC");
    for (int i=0; i < _qualityOptionsFlac.size(); i++)
        log (String(i) + ": " + _qualityOptionsFlac[i]);
    */
}

SoundFont::~SoundFont()
{
    _manager.clearFormats();
    _qualityOptionsVorbis.clear();
    _qualityOptionsFlac.clear();
    _samples.clear();
    _instruments.clear();
    _presets.clear();
    _pZones.clear();
    _iZones.clear();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------




#if 0
#pragma mark Reading SF2
#endif


bool SoundFont::read()
{
    _fileSizeIn = _path.getSize();
    ScopedPointer<FileInputStream> in = new FileInputStream(_path);
    _infile = in;
    
    if (!_infile->openedOk()) {
        log(String("cannot open " + _path.getFullPathName()));
        return false;
    }
    try {
        int len = readFourcc("RIFF");
        readSignature("sfbk");
        len -= 4;
        while (len) {
            int len2 = readFourcc("LIST");
            len -= (len2 + 8);
            char fourcc[5];
            fourcc[0] = 0;
            readSignature(fourcc);
            fourcc[4] = 0;
            len2 -= 4;
            while (len2) {
                fourcc[0] = 0;
                int len3 = readFourcc(fourcc);
                fourcc[4] = 0;
                len2 -= (len3 + 8);
                readSection(fourcc, len3);
            }
        }
        // load sample data
        for (int i = 0; i < _samples.size(); i++)
            readSampleData(_samples[i]);
    }
    catch (juce::String s) {
        log(s);
        return false;
    }
    catch (const char* s) {
        log(String(s));
        return false;
    }
    return true;
}

//---------------------------------------------------------
//   skip
//---------------------------------------------------------

void SoundFont::skip (int n)
{
    int64 pos = _infile->getPosition();
    if (!_infile->setPosition(pos + n))
        throw("unexpected end of file");
}

//---------------------------------------------------------
//   readFourcc
//---------------------------------------------------------

int SoundFont::readFourcc (char* signature)
{
    readSignature(signature);
    return readDword();
}

int SoundFont::readFourcc (const char* signature)
{
    readSignature(signature);
    return readDword();
}

//---------------------------------------------------------
//   readSignature
//---------------------------------------------------------

void SoundFont::readSignature (const char* signature)
{
    char fourcc[4];
    readSignature(fourcc);
    if (memcmp(fourcc, signature, 4) != 0)
        throw(String("fourcc " + String(signature) + " expected"));
}

void SoundFont::readSignature(char* signature)
{
    if (_infile->read(signature, 4) != 4)
        throw("unexpected end of file");
}

//---------------------------------------------------------
//   readDword
//---------------------------------------------------------

unsigned SoundFont::readDword()
{
    unsigned format;
    if (_infile->read((char*)&format, 4) != 4)
        throw("unexpected end of file");
    return juce::ByteOrder::swapIfBigEndian(format);
}

//---------------------------------------------------------
//   readWord
//---------------------------------------------------------

int SoundFont::readWord()
{
    unsigned short format;
    if (_infile->read((char*)&format, 2) != 2)
        throw("unexpected end of file");
    return juce::ByteOrder::swapIfBigEndian(format);
}

//---------------------------------------------------------
//   readShort
//---------------------------------------------------------

int SoundFont::readShort()
{
    short format;
    if (_infile->read((char*)&format, 2) != 2)
        throw("unexpected end of file");
    return juce::ByteOrder::swapIfBigEndian(format);
}

//---------------------------------------------------------
//   readByte
//---------------------------------------------------------

int SoundFont::readByte()
{
    byte val;
    if (_infile->read((char*)&val, 1) != 1)
        throw("unexpected end of file");
    return val;
}

//---------------------------------------------------------
//   readChar
//---------------------------------------------------------

int SoundFont::readChar()
{
    char val;
    if (_infile->read(&val, 1) != 1)
        throw("unexpected end of file");
    return val;
}

//---------------------------------------------------------
//   readVersion
//---------------------------------------------------------

void SoundFont::readVersion()
{
    unsigned char data[4];
    if (_infile->read((char*)data, 4) != 4)
        throw("unexpected end of file");
    _version.major = data[0] + (data[1] << 8);
    _version.minor = data[2] + (data[3] << 8);
    
    _fileFormatIn = SF2Format;
    if (_version.major == 3) _fileFormatIn = SF3Format;
    if (_version.major == 4) _fileFormatIn = SF4Format;
}

//---------------------------------------------------------
//   readString
//---------------------------------------------------------

String SoundFont::readString (int n)
{
    if (n == 0)
        return String::empty;
    
	// Visual C++ doesn't allow a variable array size here
    if (n > 2014) n = 1024;
    char data[1024];

    if (_infile->read((char*)data, n) != n)
        throw("unexpected end of file");
    if (data[n-1] != 0)
        data[n] = 0;
    
    return String::fromUTF8(data);
}

//---------------------------------------------------------
//   readSection
//---------------------------------------------------------

void SoundFont::readSection (const char* fourcc, int len)
{
    switch(FOURCC(fourcc[0], fourcc[1], fourcc[2], fourcc[3])) {
    case FOURCC('i', 'f', 'i', 'l'):    // version
        readVersion();
        break;
    case FOURCC('I','N','A','M'):       // sound font name
        _name = readString(len);
        break;
    case FOURCC('i','s','n','g'):       // target render engine
        _engine = readString(len);
        break;
    case FOURCC('I','P','R','D'):       // product for which the bank was intended
        _product = readString(len);
        break;
    case FOURCC('I','E','N','G'): // sound designers and engineers for the bank
        _creator = readString(len);
        break;
    case FOURCC('I','S','F','T'): // SoundFont tools used to create and alter the bank
        _tools = readString(len);
        break;
    case FOURCC('I','C','R','D'): // date of creation of the bank
        _date = readString(len);
        break;
    case FOURCC('I','C','M','T'): // comments on the bank
        _comment = readString(len);
        break;
    case FOURCC('I','C','O','P'): // copyright message
        _copyright = readString(len);
        break;
    case FOURCC('s','m','p','l'): // the digital audio samples
        _samplePos = _infile->getPosition();
        _sampleLen = len;
        skip(len);
        break;
    case FOURCC('p','h','d','r'): // preset headers
        readPhdr(len);
        break;
    case FOURCC('p','b','a','g'): // preset index list
        readBag(len, &_pZones);
        break;
    case FOURCC('p','m','o','d'): // preset modulator list
        readMod(len, &_pZones);
        break;
    case FOURCC('p','g','e','n'): // preset generator list
        readGen(len, &_pZones);
        break;
    case FOURCC('i','n','s','t'): // instrument names and indices
        readInst(len);
        break;
    case FOURCC('i','b','a','g'): // instrument index list
        readBag(len, &_iZones);
        break;
    case FOURCC('i','m','o','d'): // instrument modulator list
        readMod(len, &_iZones);
        break;
    case FOURCC('i','g','e','n'): // instrument generator list
        readGen(len, &_iZones);
        break;
    case FOURCC('s','h','d','r'): // sample headers
        readShdr(len);
        break;
            
    case FOURCC('s','h','d','X'): // original sample lenghts & loops for verification (compressed formats only)
        readShdX(len);
        break;
            
    case FOURCC('i', 'r', 'o', 'm'):    // sample rom
    case FOURCC('i', 'v', 'e', 'r'):    // sample rom version
    default:
        skip(len);
        throw(String("unknown fourcc " + String(fourcc)));
        break;
    }
}

//---------------------------------------------------------
//   readPhdr
//---------------------------------------------------------

void SoundFont::readPhdr (int len)
{
    if (len < (38 * 2))
        throw("phdr too short");
    if (len % 38)
        throw("phdr not a multiple of 38");
    int n = len / 38;
    if (n <= 1) {
        skip(len);
        return;
    }
    int index1 = 0, index2;
    for (int i = 0; i < n; ++i) {
        Preset* preset       = new Preset;
        preset->name         = readString(20);
        preset->preset       = readWord();
        preset->bank         = readWord();
        index2               = readWord();
        preset->library      = readDword();
        preset->genre        = readDword();
        preset->morphology   = readDword();
        if (index2 < index1)
            throw("preset header indices not monotonic");
        if (i > 0) {
            int n = index2 - index1;
            while (n--) {
                Zone* z = new Zone;
                _presets.getLast()->zones.add(z);
                _pZones.add(z);
            }
        }
        index1 = index2;
        _presets.add(preset);
    }
    _presets.removeLast();
}

//---------------------------------------------------------
//   readBag
//---------------------------------------------------------

void SoundFont::readBag (int len, juce::Array<Zone*>* zones)
{
    if (len % 4)
        throw("bag size not a multiple of 4");
    int gIndex2, mIndex2;
    int gIndex1 = readWord();
    int mIndex1 = readWord();
    len -= 4;

    for (int z = 0; z < zones->size(); z++)
    {
        Zone* zone = zones->getUnchecked(z);
        
        gIndex2 = readWord();
        mIndex2 = readWord();
        len -= 4;
        if (len < 0)
            throw("bag size too small");
        if (gIndex2 < gIndex1)
            throw("generator indices not monotonic");
        if (mIndex2 < mIndex1)
            throw("modulator indices not monotonic");
        int n = mIndex2 - mIndex1;
        while (n--)
            zone->modulators.add(new ModulatorList);
        n = gIndex2 - gIndex1;
        while (n--)
            zone->generators.add(new GeneratorList);
        gIndex1 = gIndex2;
        mIndex1 = mIndex2;
    }
}

//---------------------------------------------------------
//   readMod
//---------------------------------------------------------

void SoundFont::readMod (int size, juce::Array<Zone*>* zones)
{
    for (int z = 0; z < zones->size(); z++)
    {
        Zone* zone = zones->getUnchecked(z);
        
        for (int k = 0; k < zone->modulators.size(); k++)
        {
            ModulatorList* m = zone->modulators.getUnchecked(k);
                
            size -= 10;
            if (size < 0)
                throw("pmod size mismatch");
            m->src           = static_cast<Modulator>(readWord());
            m->dst           = static_cast<Generator>(readWord());
            m->amount        = readShort();
            m->amtSrc        = static_cast<Modulator>(readWord());
            m->transform     = static_cast<Transform>(readWord());
        }
    }
    if (size != 10)
        throw("modulator list size mismatch");
    skip(10);
}

//---------------------------------------------------------
//   readGen
//---------------------------------------------------------

void SoundFont::readGen (int size, juce::Array<Zone*>* zones)
{
    if (size % 4)
        throw(String("bad generator list size"));

    for (int z = 0; z < zones->size(); z++) {
        Zone* zone = zones->getUnchecked(z);
        
        size -= (zone->generators.size() * 4);
        if (size < 0)
            break;

        for (int g = 0; g < zone->generators.size(); g++) {
            GeneratorList* gen = zone->generators.getUnchecked(g);
            
            gen->gen = static_cast<Generator>(readWord());
            if (gen->gen == Gen_KeyRange || gen->gen == Gen_VelRange) {
                gen->amount.lo = readByte();
                gen->amount.hi = readByte();
            }
            else if (gen->gen == Gen_Instrument)
                gen->amount.uword = readWord();
            else
                gen->amount.sword = readWord();
        }
    }
    if (size != 4)
        throw(String("generator list size mismatch != 4: ") + String(size));
    skip(size);
}

//---------------------------------------------------------
//   readInst
//---------------------------------------------------------

void SoundFont::readInst (int size)
{
    int n = size / 22;
    int index1 = 0, index2;
    for (int i = 0; i < n; ++i) {
        Instrument* instrument = new Instrument;
        instrument->name = readString(20);
        index2           = readWord();
        if (index2 < index1)
            throw("instrument header indices not monotonic");
        if (i > 0) {
            int n = index2 - index1;
            while (n--) {
                Zone* z = new Zone;
                _instruments.getLast()->zones.add(z);
                _iZones.add(z);
            }
        }
        index1 = index2;
        _instruments.add(instrument);
    }
    _instruments.removeLast();
}

//---------------------------------------------------------
//   readShdr
//---------------------------------------------------------

void SoundFont::readShdr (int size)
{
    int n = size / 46;
    for (int i = 0; i < n-1; ++i)
    {
        Sample* s = new Sample;
        
        s->name       = readString(20);
        s->start      = readDword();
        s->end        = readDword();
        s->loopstart  = readDword();
        s->loopend    = readDword();
        s->samplerate = readDword();
        s->origpitch  = readByte();
        s->pitchadj   = readChar();
        s->sampleLink = readWord();
        s->sampletype = readWord();

        _samples.add(s);
    }
    skip(46);   // trailing record
}

//---------------------------------------------------------
//   readShdr
//---------------------------------------------------------

void SoundFont::readShdX (int size)
{
    int n = size / SampleMetaSize;
    jassert (_samples.size() == (n-1));
    log (String("Reading verification data for " + String(_samples.size()) + " samples"));
    
    for (int i = 0; i < n-1; ++i)
    {
        SampleMeta* m = _samples[i]->createMeta();
        m->name       = readString(20);
        m->samples    = readDword();
        m->loopstart  = readDword();
        m->loopend    = readDword();
        // it is required that samples & meta headers be written in identical sequence!
        jassert (_samples[i]->name == m->name);
    }
    skip(SampleMetaSize);   // trailing record
}

#if 0
#pragma mark Writing SF2
#endif

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool SoundFont::write (const File filename, FileType format, int quality)
{
    ScopedPointer<FileOutputStream> out = new FileOutputStream(filename);
    
    _outfile = out;
    _outfile->setPosition(0);
    _outfile->truncate();
    _fileFormatOut = format;
    
    /** Add a warning that samples were decompressed from a lossy format */
    if (_fileFormatIn == SF2::FileType::SF3Format && _fileFormatOut != _fileFormatIn)
    {
        _comment << "\n\n" << "CAUTION: Samples in this file were decompressed from a lossy format (Ogg Vorbis). If you want to edit this file, you should get the original uncompressed SF2 file.";
    }
    
    int64 riffLenPos;
    int64 listLenPos;
    try {
        _outfile->write("RIFF", 4);
        riffLenPos = _outfile->getPosition();
        writeDword(0);
        _outfile->write("sfbk", 4);

        _outfile->write("LIST", 4);
        listLenPos = _outfile->getPosition();
        writeDword(0);
        _outfile->write("INFO", 4);

        writeIfil();
        if (_name.isNotEmpty())      writeStringSection("INAM", _name);
        if (_engine.isNotEmpty())    writeStringSection("isng", _engine);
        if (_product.isNotEmpty())   writeStringSection("IPRD", _product);
        if (_creator.isNotEmpty())   writeStringSection("IENG", _creator);
        if (_tools.isNotEmpty())     writeStringSection("ISFT", _tools);
        if (_date.isNotEmpty())      writeStringSection("ICRD", _date);
        if (_comment.isNotEmpty())   writeStringSection("ICMT", _comment);
        if (_copyright.isNotEmpty()) writeStringSection("ICOP", _copyright);

        int64 pos = _outfile->getPosition();
        _outfile->setPosition(listLenPos);
        writeDword(pos - listLenPos - 4);
        _outfile->setPosition(pos);

        _outfile->write("LIST", 4);
        listLenPos = _outfile->getPosition();
        writeDword(0);
        
        _outfile->write("sdta", 4);
        writeSmpl(quality);
        pos = _outfile->getPosition();
        _outfile->setPosition(listLenPos);
        writeDword(pos - listLenPos - 4);
        _outfile->setPosition(pos);

        _outfile->write("LIST", 4);
        listLenPos = _outfile->getPosition();
        writeDword(0);
        _outfile->write("pdta", 4);

        writePhdr();
        writeBag("pbag", &_pZones);
        writeMod("pmod", &_pZones);
        writeGen("pgen", &_pZones);
        writeInst();
        writeBag("ibag", &_iZones);
        writeMod("imod", &_iZones);
        writeGen("igen", &_iZones);
        writeShdr();
        
        if (_fileFormatOut != SF2Format)
            writeShdX();

        pos = _outfile->getPosition();
        _outfile->setPosition(listLenPos);
        writeDword(pos - listLenPos - 4);
        _outfile->setPosition(pos);

        int64 endPos = _outfile->getPosition();
        _outfile->setPosition(riffLenPos);
        writeDword(endPos - riffLenPos - 4);
        
        _fileSizeOut = endPos;
    }
    catch (String s) {
        log(String("write SF2 file failed: " + s));
        return false;
    }
    
    String msg;
    int percent = round(100 * (double)_fileSizeOut/(double)_fileSizeIn);
    msg << "File size change: "  << percent << "%";
    log(msg);
    
    return true;
}

//---------------------------------------------------------
//   writeDword
//---------------------------------------------------------

void SoundFont::writeDword (int val)
{
    val = juce::ByteOrder::swapIfBigEndian(val);
    write((char*)&val, 4);
}

//---------------------------------------------------------
//   writeWord
//---------------------------------------------------------

void SoundFont::writeWord (unsigned short int val)
{
    val = juce::ByteOrder::swapIfBigEndian(val);
    write((char*)&val, 2);
}

//---------------------------------------------------------
//   writeByte
//---------------------------------------------------------

void SoundFont::writeByte (unsigned char val)
{
    write((char*)&val, 1);
}

//---------------------------------------------------------
//   writeChar
//---------------------------------------------------------

void SoundFont::writeChar (char val)
{
    write((char*)&val, 1);
}

//---------------------------------------------------------
//   writeShort
//---------------------------------------------------------

void SoundFont::writeShort (short val)
{
    val = juce::ByteOrder::swapIfBigEndian(val);
    write((char*)&val, 2);
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void SoundFont::write (const char* p, int n)
{
    if (!_outfile->write(p, n))
        throw("write error");
}

//---------------------------------------------------------
//   writeString
//---------------------------------------------------------

void SoundFont::writeString (const String& string, size_t size)
{
	// Visual C++ doesn't allow variable arrays
	const size_t limit = 1024;
    char name[limit];
    memset(name, 0, limit);
    // Yes, there are better ways to port this ...    
    if (string.getNumBytesAsUTF8() > 0)
        memcpy(name,
               string.toRawUTF8(),
               jmin(limit, size, strlen(string.toRawUTF8())));
    
    write(name, jmin(limit, size));
}

//---------------------------------------------------------
//   writeStringSection
//---------------------------------------------------------

void SoundFont::writeStringSection (const char* fourcc, const String& string)
{
    const char* s = string.toRawUTF8();
    write(fourcc, 4);
    int nn = (int)strlen(s) + 1;
    int n = ((nn + 1) / 2) * 2;
    writeDword(n);
    write(s, nn);
    if (n - nn) {
        char c = 0;
        write(&c, 1);
    }
}

//---------------------------------------------------------
//   writeIfil
//---------------------------------------------------------

void SoundFont::writeIfil()
{
    write("ifil", 4);
    writeDword(4);
    unsigned char data[4];
    if (_fileFormatOut == SF3Format) _version.major = 3;
    if (_fileFormatOut == SF4Format) _version.major = 4;
    data[0] = _version.major;
    data[1] = _version.major >> 8;
    data[2] = _version.minor;
    data[3] = _version.minor >> 8;
    write((char*)data, 4);
}


//---------------------------------------------------------
//   writePhdr
//---------------------------------------------------------

void SoundFont::writePhdr()
{
    write("phdr", 4);
    int n = _presets.size();
    writeDword((n + 1) * 38);
    int zoneIdx = 0;

    for (int z = 0; z < _presets.size(); z++)
    {
        const Preset* p = _presets.getUnchecked(z);
        
        writePreset(zoneIdx, p);
        zoneIdx += p->zones.size();
    }
    Preset p;
    writePreset(zoneIdx, &p);
}

//---------------------------------------------------------
//   writePreset
//---------------------------------------------------------

void SoundFont::writePreset (int zoneIdx, const Preset* preset)
{
    writeString(preset->name, 20);
    writeWord(preset->preset);
    writeWord(preset->bank);
    writeWord(zoneIdx);
    writeDword(preset->library);
    writeDword(preset->genre);
    writeDword(preset->morphology);
}

//---------------------------------------------------------
//   writeBag
//---------------------------------------------------------

void SoundFont::writeBag (const char* fourcc, juce::Array<Zone*>* zones)
{
    write(fourcc, 4);
    int n = zones->size();
    writeDword((n + 1) * 4);
    int gIndex = 0;
    int pIndex = 0;

    for (int i = 0; i < zones->size(); i++)
    {
        const Zone* z = zones->getUnchecked(i);
        
        writeWord(gIndex);
        writeWord(pIndex);
        gIndex += z->generators.size();
        pIndex += z->modulators.size();
    }
    writeWord(gIndex);
    writeWord(pIndex);
}

//---------------------------------------------------------
//   writeMod
//---------------------------------------------------------

void SoundFont::writeMod (const char* fourcc, const juce::Array<Zone*>* zones)
{
    write(fourcc, 4);
    int n = 0;

    for (int i = 0; i < zones->size(); i++)
    {
        const Zone* zone = zones->getUnchecked(i);
        n += zone->modulators.size();
    }
    writeDword((n + 1) * 10);

    for (int i = 0; i < zones->size(); i++)
    {
        const Zone* zone = zones->getUnchecked(i);
        for (int k = 0; k < zone->modulators.size(); k++)
        {
            const ModulatorList* m = zone->modulators.getUnchecked(k);
            writeModulator(m);
        }
    }
    // Empty terminator
    ModulatorList mod;
    writeModulator(&mod);
}

//---------------------------------------------------------
//   writeModulator
//---------------------------------------------------------

void SoundFont::writeModulator (const ModulatorList* m)
{
    writeWord(m->src);
    writeWord(m->dst);
    writeShort(m->amount);
    writeWord(m->amtSrc);
    writeWord(m->transform);
}

//---------------------------------------------------------
//   writeGen
//---------------------------------------------------------

void SoundFont::writeGen (const char* fourcc, juce::Array<Zone*>* zones)
{
    write(fourcc, 4);
    int n = 0;

    for (int i = 0; i < zones->size(); i++)
    {
        const Zone* zone = zones->getUnchecked(i);
        n += zone->generators.size();
    }
    writeDword((n + 1) * 4);

    for (int i = 0; i < zones->size(); i++)
    {
        const Zone* zone = zones->getUnchecked(i);

        for (int k = 0; k < zone->generators.size(); k++)
        {
            const GeneratorList* g = zone->generators.getUnchecked(k);
            writeGenerator(g);
        }
    }
    // Empty terminator
    GeneratorList gen;
    writeGenerator(&gen);
}

//---------------------------------------------------------
//   writeGenerator
//---------------------------------------------------------

void SoundFont::writeGenerator (const GeneratorList* g)
{
    writeWord(g->gen);
    if (g->gen == Gen_KeyRange || g->gen == Gen_VelRange) {
        writeByte(g->amount.lo);
        writeByte(g->amount.hi);
    }
    else if (g->gen == Gen_Instrument)
        writeWord(g->amount.uword);
    else
        writeWord(g->amount.sword);
}

//---------------------------------------------------------
//   writeInst
//---------------------------------------------------------

void SoundFont::writeInst()
{
    write("inst", 4);
    int n = _instruments.size();
    writeDword((n + 1) * 22);
    int zoneIdx = 0;

    for (int i = 0; i < _instruments.size(); i++)
    {
        const Instrument* p = _instruments.getUnchecked(i);
        writeInstrument(zoneIdx, p);
        zoneIdx += p->zones.size();
    }
    Instrument p;
    writeInstrument(zoneIdx, &p);
}

//---------------------------------------------------------
//   writeInstrument
//---------------------------------------------------------

void SoundFont::writeInstrument(int zoneIdx, const Instrument* instrument)
{
    writeString(instrument->name, 20);
    writeWord(zoneIdx);
}

//---------------------------------------------------------
//   writeShdr
//---------------------------------------------------------

void SoundFont::writeShdr()
{
    write("shdr", 4);
    writeDword(46 * (_samples.size() + 1));

    for (int i = 0; i < _samples.size(); i++)
        writeShdrEach(_samples[i]);

    // Empty last sample as terminator
    Sample s;
    writeShdrEach(&s);
}

//---------------------------------------------------------
//   writeShdrEach
//---------------------------------------------------------

void SoundFont::writeShdrEach (const Sample* s)
{
    writeString(s->name, 20);
    writeDword(s->start);
    writeDword(s->end);
    writeDword(s->loopstart);
    writeDword(s->loopend);
    writeDword(s->samplerate);
    writeByte(s->origpitch);
    writeChar(s->pitchadj);
    writeWord(s->sampleLink);
    writeWord(s->sampletype);
}

//---------------------------------------------------------
//   writeShdX
//---------------------------------------------------------

void SoundFont::writeShdX()
{
    // need 100% meta data available
    for (int i = 0; i < _samples.size(); i++)
    {
        if (_samples[i]->meta == nullptr)
            return;
    }
    
    log (String("Attaching verification data for " + String(_samples.size()) + " samples"));
    
    write("shdX", 4);
    writeDword(SampleMetaSize * (_samples.size() + 1));

    for (int i = 0; i < _samples.size(); i++)
        writeShdXEach(_samples[i]->meta);

    // Empty terminator
    SampleMeta m;
    writeShdXEach(&m);
}

//---------------------------------------------------------
//   writeShdXEach
//---------------------------------------------------------

void SoundFont::writeShdXEach (const SampleMeta* m)
{
    jassert (m != nullptr);
    
    int64 start = _outfile->getPosition();
    
    writeString(m->name, 20);
    writeDword(m->samples);
    writeDword(m->loopstart);
    writeDword(m->loopend);
    // Check SampleMetaSize is correct
    jassert (_outfile->getPosition() - start == SampleMetaSize);
}

//---------------------------------------------------------
//   writeSmpl
//---------------------------------------------------------

void SoundFont::writeSmpl (int quality)
{
    /* Write sample data chunk and update each Sample's metadata
     to reflect the actual written offsets */
    
    write("smpl", 4);
    int64 pos = _outfile->getPosition();
    writeDword(0);
    
    int64 offsetFromChunk = 0;
    switch (_fileFormatOut)
    {
        case SF2Format: // SF2
        {
            for (int i = 0; i < _samples.size(); i++)
            {
                Sample* s = _samples.getUnchecked(i);
                int written = writeSampleDataPlain(s);
                
                s->setCompressionType(Raw);
                // Offsets in SF2 format based on 'sample count'
                s->start = offsetFromChunk / sizeof(short);
                offsetFromChunk += written;
                s->end = offsetFromChunk / sizeof(short);
                // turn relative loop points to absolute, as SF2 format requires
                s->loopstart += s->start;
                s->loopend   += s->start;
            }
            break;
        }
        case SF3Format: // SF3
        {
            for (int i = 0; i < _samples.size(); i++)
            {
                Sample* s = _samples.getUnchecked(i);
                int written = writeSampleDataVorbis(s, quality);
                
                s->setCompressionType(Vorbis);
                // Offsets in SF3 based on byte offset in file.
                // Hack start/end of sample metadata to accommodate this:
                s->start = offsetFromChunk;
                offsetFromChunk += written;
                s->end = offsetFromChunk;
                // Important: keep relative loop offsets in file, so it can be restored after loading.
                // Loop is already relative ...
            }
            break;
        }
        case SF4Format: // SF4
        {
            for (int i = 0; i < _samples.size(); i++)
            {
                Sample* s = _samples.getUnchecked(i);
                int written = writeSampleDataFlac(s, quality);
                
                s->setCompressionType(Flac);
                // Offsets in SF4 based on byte offset in file.
                // Hack start/end of sample metadata to accommodate this:
                s->start = offsetFromChunk;
                offsetFromChunk += written;
                s->end = offsetFromChunk;
                // Important: keep relative loop offsets in file, so it can be restored after loading.
                // Loop is already relative ...
            }
            break;
        }
    }
    int64 npos = _outfile->getPosition();
    _outfile->setPosition(pos);
    writeDword(npos - pos - 4);
    _outfile->setPosition(npos);
}



#if 0
#pragma mark Reading Sample Data
#endif

//---------------------------------------------------------
//   readSampleData
//---------------------------------------------------------

int SoundFont::readSampleData(Sample* s)
{
#if USE_MULTIPLE_COMPRESSION_FORMATS
    switch (s->getCompressionType())
    {
        case Raw:    return readSampleDataRaw(s);    break;
        case Vorbis: return readSampleDataVorbis(s); break;
        case Flac:   return readSampleDataFlac(s);   break;
        default:
            break;
    }
#else
    if (_fileFormatIn == SF2Format)
        return readSampleDataRaw(s);
    
    if (_fileFormatIn == SF3Format)
        return readSampleDataVorbis(s);
    
    if (_fileFormatIn == SF4Format)
        return readSampleDataFlac(s);
#endif
    jassertfalse;
    return 0;
}

//---------------------------------------------------------
//   readSampleDataRaw
//---------------------------------------------------------

int SoundFont::readSampleDataRaw (Sample* s)
{
    // Offsets in SF2 are based on samples (short)
    _infile->setPosition(_samplePos + s->start * sizeof(short));
    
    int numSamples = (s->end - s->start);
    s->sampleDataSize = numSamples;
    s->sampleData = new short[numSamples];
    int read = _infile->read(s->sampleData, numSamples * sizeof(short));
    
    // normalize offsets & make loop relative
    s->loopstart -= s->start;
    s->loopend   -= s->start;
    s->start = 0;
    s->end = numSamples;
    
    s->createMeta();
    
    return read;
}

//---------------------------------------------------------
//   readSampleDataVorbis
//---------------------------------------------------------

int SoundFont::readSampleDataVorbis (Sample* s)
{
    // Offsets in SF3 are bytes
    int numBytes = (s->end - s->start);
    s->byteDataSize = numBytes;
    s->byteData = new byte[numBytes];
    _infile->setPosition(_samplePos + s->start);
    _infile->read(s->byteData, numBytes);
    
#if USE_JUCE_VORBIS
    
    MemoryInputStream* input = new MemoryInputStream(s->byteData, s->byteDataSize, false);
    ScopedPointer<AudioFormatReader> reader = _audioFormatVorbis->createReaderFor(input, true);
    if (reader == nullptr)
        throw("Failed decoding Vorbis data!");
    int numSamples = reader->lengthInSamples;
    AudioSampleBuffer buffer (1, numSamples);
    buffer.clear();
    reader->read(&buffer, 0, numSamples, 0, 1, 1);
    
    // copy buffer to sampleData
    s->sampleDataSize = numSamples;
    s->sampleData = new short[numSamples];
    const float* b = buffer.getReadPointer(0);
    for (int i=0; i < numSamples; i++)
        s->sampleData[i] = round(b[i] * 32768.f);
#else
    
    decodeOggVorbis(s);
    int numSamples = s->numSamples();
    
#endif
    
    // normalize offsets & make loop relative
    s->start = 0;
    s->end = numSamples;
    // loop in file was already relative ...
    //s->loopstart -= s->start;
    //s->loopend   -= s->start;
    
    jassert (s->checkMeta());
    s->dropByteData();
    return numBytes;
}

//---------------------------------------------------------
//   readSampleDataFlac
//---------------------------------------------------------

int SoundFont::readSampleDataFlac (Sample* s)
{
    // Offsets in SF4 are bytes
    int numBytes = (s->end - s->start);
    s->byteDataSize = numBytes;
    s->byteData = new byte[numBytes];
    _infile->setPosition(_samplePos + s->start);
    _infile->read(s->byteData, numBytes);
    
    MemoryInputStream* input = new MemoryInputStream(s->byteData, s->byteDataSize, false);
    ScopedPointer<AudioFormatReader> reader = _audioFormatFlac->createReaderFor(input, true);
    if (reader == nullptr)
        throw("Failed decoding FLAC data!");
    
    int numSamples = reader->lengthInSamples;
    AudioSampleBuffer buffer (1, numSamples);
    buffer.clear();
    reader->read(&buffer, 0, numSamples, 0, 1, 1);
    
    // copy buffer to sampleData
    s->sampleDataSize = numSamples;
    s->sampleData = new short[numSamples];
    const float* b = buffer.getReadPointer(0);
    for (int i=0; i < numSamples; i++)
        s->sampleData[i] = round(b[i] * 32768.f);
    
    // normalize offsets & make loop relative
    s->start = 0;
    s->end = numSamples;
    // loop in file was already relative ...
    //s->loopstart -= s->start;
    //s->loopend   -= s->start;
    
    s->dropByteData();
    jassert (s->checkMeta());
    
    return numBytes;
}




#if 0
#pragma mark Writing Sample Data
#endif

//---------------------------------------------------------
//   writeSampleDataPlain
//---------------------------------------------------------

int SoundFont::writeSampleDataPlain (Sample* s)
{
    jassert (s->numSamples() > 0);
    
    int numBytes = s->numSamples() * sizeof(short);
    write((char*)s->sampleData, numBytes);
    return numBytes;
}

    
//---------------------------------------------------------
//   writeSampleDataVorbis
//---------------------------------------------------------

int SoundFont::writeSampleDataVorbis (Sample* s, int quality)
{
    jassert (s->numSamples() > 0);
    const int numSamples = s->numSamples();
    int rawBytes = numSamples * sizeof(short);
    int option = 4;
    
#if USE_JUCE_VORBIS
    
    AudioSampleBuffer buffer (1, numSamples);
    float* b = buffer.getWritePointer(0);
    for (int i=0; i < numSamples; i++)
        b[i] = (float)s->sampleData[i] / 32768.f; // scale to unity
    
    /**
     0: 64 kbps
     1: 80 kbps
     2: 96 kbps
     3: 112 kbps
     4: 128 kbps
     5: 160 kbps
     6: 192 kbps
     7: 224 kbps
     8: 256 kbps
     9: 320 kbps
     10: 500 kbps */
    switch (quality) {
        case 0: option = 5; break; // Low quality
        case 1: option = 8; break; // Medium quality
        case 2: option = 10; break; // High quality
    }
    jassert(option < _qualityOptionsVorbis.size());
  
    MemoryBlock output;
    {
        MemoryOutputStream* temp = new MemoryOutputStream(output, false);
        ScopedPointer<AudioFormatWriter> writer = _audioFormatVorbis->
            createWriterFor(temp, s->samplerate, 1, 16, nullptr, option);
        writer->writeFromAudioSampleBuffer(buffer,0,numSamples);
        // writer MUST be deleted to properly flush & close ...
    }
    
    int numBytes = output.getSize();
    write((char*)output.getData(), numBytes);
    
#else  // USE_JUCE_VORBIS

    ogg_stream_state os;
    ogg_page         og;
    ogg_packet       op;
    vorbis_info      vi;
    vorbis_dsp_state vd;
    vorbis_block     vb;
    vorbis_comment   vc;
    
    vorbis_info_init(&vi);
    
    float qualityF = 1.0f;
    switch (quality) {
        case 0: option = 5; qualityF = 0.2f; break; // Low quality
        case 1: option = 8; qualityF = 0.6f; break; // Medium quality
        case 2: option = 10; qualityF = 1.0f; break; // High quality
    }
    
    int ret = vorbis_encode_init_vbr(&vi, 1, s->samplerate, qualityF);
    if (ret) {
        fprintf(stderr, "vorbis init failed\n");
        return false;
    }
    
    vorbis_comment_init(&vc);
    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);
    srand(time(NULL));
    ogg_stream_init(&os, rand());
    
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    
    vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);
    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &header_comm);
    ogg_stream_packetin(&os, &header_code);
    
    char* obuf = new char[1048576]; // 1024 * 1024
    char* p = obuf;
    
    for (;;) {
        int result = ogg_stream_flush(&os, &og);
        if (result == 0)
            break;
        memcpy(p, og.header, og.header_len);
        p += og.header_len;
        memcpy(p, og.body, og.body_len);
        p += og.body_len;
    }
    
    long i;
    int page = 0;
    
    for(;;) {
        int bufflength = jmin(BLOCK_SIZE, numSamples - page * BLOCK_SIZE);
        float **buffer = vorbis_analysis_buffer(&vd, bufflength);
        int j = 0;
        int max = jmin((page+1) * BLOCK_SIZE, numSamples);
        for (i = page * BLOCK_SIZE; i < max ; i++) {
            buffer[0][j] = s->sampleData[i] / 32768.f;
            // buffer[0][j] = ibuffer[i] / 35000.f; // HACK: attenuate samples due to libsndfile bug
            j++;
        }
        
        vorbis_analysis_wrote(&vd, bufflength);
        
        while (vorbis_analysis_blockout(&vd, &vb) == 1) {
            vorbis_analysis(&vb, 0);
            vorbis_bitrate_addblock(&vb);
            
            while (vorbis_bitrate_flushpacket(&vd, &op)) {
                ogg_stream_packetin(&os, &op);
                
                for(;;) {
                    int result = ogg_stream_pageout(&os, &og);
                    if (result == 0)
                        break;
                    memcpy(p, og.header, og.header_len);
                    p += og.header_len;
                    memcpy(p, og.body, og.body_len);
                    p += og.body_len;
                }
            }
        }
        page++;
        if ((max == numSamples) || !((numSamples - page * BLOCK_SIZE) > 0))
            break;
    }
    
    vorbis_analysis_wrote(&vd, 0);
    
    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
        vorbis_analysis(&vb, 0);
        vorbis_bitrate_addblock(&vb);
        
        while (vorbis_bitrate_flushpacket(&vd, &op)) {
            ogg_stream_packetin(&os, &op);
            
            for(;;) {
                int result = ogg_stream_pageout(&os, &og);
                if (result == 0)
                    break;
                memcpy(p, og.header, og.header_len);
                p += og.header_len;
                memcpy(p, og.body, og.body_len);
                p += og.body_len;
            }
        }
    }
    
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    
    int numBytes = p - obuf;
    write(obuf, numBytes);
    delete [] obuf;
    
#endif // USE_JUCE_VORBIS
    
    String msg;
    int percent = roundf(100.f * (float)numBytes/(float)rawBytes);
    msg << "Compressed " << _qualityOptionsVorbis[option] << ": " << s->name << " (" << percent << "%)";
    log(msg);
    
    return numBytes;
}




//---------------------------------------------------------
//   writeSampleDataFlac
//---------------------------------------------------------

int SoundFont::writeSampleDataFlac (Sample* s, int quality)
{
    jassert (s->numSamples() > 0);
    const int numSamples = s->numSamples();
    int rawBytes = numSamples * sizeof(short);

    AudioSampleBuffer buffer (1, numSamples);
    float* b = buffer.getWritePointer(0);
    for (int i=0; i < numSamples; i++)
        b[i] = (float)s->sampleData[i] / 32768.f; // scale to unity
    
    int option = 8;
    /**
     0: 0 (Fastest)
     1: 1
     2: 2
     3: 3
     4: 4
     5: 5 (Default)
     6: 6
     7: 7
     8: 8 (Highest quality) */
    switch (quality) {
        case 0: option = 1; break; // Low quality
        case 1: option = 5; break; // Medium quality
        case 2: option = 8; break; // High quality
    }
    jassert(option < _qualityOptionsFlac.size());
    
    MemoryBlock output;
    {
        MemoryOutputStream* temp = new MemoryOutputStream(output, false);
        ScopedPointer<AudioFormatWriter>  writer = _audioFormatFlac->
                createWriterFor(temp, s->samplerate, 1, 16, nullptr, option);
        writer->writeFromAudioSampleBuffer(buffer,0,numSamples);
        // writer MUST be deleted to properly flush & close ...
    }
    int numBytes = output.getSize();
    write((char*)output.getData(), numBytes);
    
    String msg;
    int percent = roundf(100.f * (float)numBytes/(float)rawBytes);
    msg << "Compressed FLAC " << _qualityOptionsFlac[option] << ": " << s->name << " (" << percent << "%)";
    log(msg);

    return numBytes;
}



#if 0
#pragma mark Ogg Vorbis Bridge
#endif


#if ! USE_JUCE_VORBIS
//---------------------------------------------------------
//   Ogg Vorbis Decompression
//---------------------------------------------------------

static size_t   ovRead (void* ptr, size_t size, size_t nmemb, void* datasource);
static int      ovSeek (void* datasource, ogg_int64_t offset, int whence);
static long     ovTell (void* datasource);

static ov_callbacks ovCallbacks = { ovRead, ovSeek, 0, ovTell };

//---------------------------------------------------------
//   Decompresion Callbacks
//---------------------------------------------------------

static size_t ovRead (void* ptr, size_t size, size_t nmemb, void* datasource)
{
    SoundFont::CallbackData* vd = (SoundFont::CallbackData*)datasource;
    Sample* s = vd->decodeSample;
    size_t n = size * nmemb;
    
    if (s->byteDataSize < int(vd->decodePosition + n))
        n = s->byteDataSize - vd->decodePosition;
    if (n) {
        const char* src = (char*)s->byteData + vd->decodePosition;
        memcpy(ptr, src, n);
        vd->decodePosition += n;
    }
    
    return n;
}

static int ovSeek (void* datasource, ogg_int64_t offset, int whence)
{
    SoundFont::CallbackData* vd = (SoundFont::CallbackData*)datasource;
    Sample* s = vd->decodeSample;
    
    switch(whence) {
        case SEEK_SET:
            vd->decodePosition = offset;
            break;
        case SEEK_CUR:
            vd->decodePosition += offset;
            break;
        case SEEK_END:
            vd->decodePosition = s->byteDataSize - offset;
            break;
    }
    return 0;
}

static long ovTell (void* datasource)
{
    SoundFont::CallbackData* vd = (SoundFont::CallbackData*)datasource;
    return vd->decodePosition;
}

//---------------------------------------------------------
//   decodeOggVorbis
//---------------------------------------------------------

bool SoundFont::decodeOggVorbis (Sample* s)
{
    CallbackData callbackData;
    callbackData.decodeSample = s;
    callbackData.decodePosition = 0;

    OggVorbis_File vf;
    juce::MemoryOutputStream output;
    
    if (ov_open_callbacks(&callbackData, &vf, 0, 0, ovCallbacks) == 0)
    {
        const int bufsize = 256000;
        char buffer[bufsize];
        int numberRead = 0;
        int section = 0;
        do {
            numberRead = ov_read(&vf, buffer, bufsize, 0, 2, 1, &section);
            jassert (numberRead >= 0);
            if (numberRead > 0)
                output.write(buffer,numberRead);
        } while (numberRead);
        
        ov_clear(&vf);
    }
    output.flush();
    
    // Copy uncompressed samples
    int numBytes = output.getDataSize();
    jassert (numBytes % sizeof(short) == 0); // must be even
    
    int numSamples = numBytes / sizeof(short);
    jassert (numSamples > 0);
    s->sampleDataSize = numSamples;
    s->sampleData = new short[numSamples];
    memcpy(s->sampleData, output.getData(), numBytes);
    
    return true;
}

#endif // USE_JUCE_VORBIS

#if 0
#pragma mark Misc
#endif

void SoundFont::dumpPresets()
{
    int idx = 0;

    for (int i = 0; i < _presets.size(); i++)
    {
        const Preset* p = _presets.getUnchecked(i);
        
        fprintf(stderr, "%03d %04x-%02x %s\n", idx, p->bank, p->preset, p->name.toRawUTF8());
        ++idx;
    }
}

void SoundFont::log (const String message)
{
    const char* charp = message.getCharPointer();
    fprintf (stderr, "%s\n", charp);
}

