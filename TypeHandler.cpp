// TypeHandler.cpp: implementation of type-specific handler classes.
//
// Copyright (c) GDCL 2004-6. All Rights Reserved. 
// You are free to re-use this as the basis for your own filter development,
// provided you retain this copyright notice in the source.
// http://www.gdcl.co.uk
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MovieWriter.h"
#include <dvdmedia.h>
#include <mmreg.h>  // for a-law and u-law G.711 audio types

#include "nalunit.h"
#include "ParseBuffer.h"

void WriteVariable(ULONG val, BYTE* pDest, int cBytes)
{
    for (int i = 0; i < cBytes; i++)
    {
        pDest[i] = BYTE((val >> (8 * (cBytes - (i+1)))) & 0xff);
    }
}

class DivxHandler : public TypeHandler
{
public:
    DivxHandler(const CMediaType* pmt);

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    // use 90Khz except for audio
    long Scale()
    {
        return 90000;
    }
    long Width();
    long Height();

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
    HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

private:
    CMediaType m_mt;
    smart_array<BYTE> m_pConfig;
    long m_cConfig;
};

class H264Handler : public TypeHandler
{
public:
    H264Handler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) {UNREFERENCED_PARAMETER(patm);}
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    // use 90Khz except for audio
    long Scale()
    {
        return 90000;
    }
    long Width();
    long Height();

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
    LONGLONG FrameDuration();

protected:
    CMediaType m_mt;
};

class H264ByteStreamHandler : public H264Handler
{
public:
    H264ByteStreamHandler(const CMediaType* pmt);

    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
    LONGLONG FrameDuration();
    HRESULT WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual);

    long Width()    { return m_cx; }
    long Height()   { return m_cy; }

    enum { nalunit_length_field = 4 };
private:
    REFERENCE_TIME m_tFrame;
    long m_cx;
    long m_cy;

    ParseBuffer m_ParamSets;        // stores param sets for WriteDescriptor
    bool m_bSPS;
    bool m_bPPS;
};

class YUVVideoHandler : public TypeHandler
{
public:
    YUVVideoHandler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'vide';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return true;
    }
    bool IsAudio()
    { 
        return false;
    }
    long SampleRate()
    {
        // an approximation is sufficient
        return 30;
    }
    // use 90Khz except for audio
    long Scale()
    {
        return 90000;
    }
    long Width();
    long Height();
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
private:
    CMediaType m_mt;
};

class AACHandler : public TypeHandler
{
public:
    AACHandler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'soun';
    }
    void WriteTREF(Atom* patm) { UNREFERENCED_PARAMETER(patm); }
    bool IsVideo() 
    {
        return false;
    }
    bool IsAudio()
    { 
        return true;
    }
    long SampleRate()
    {
        return 50;
    }
    long Scale();
    long Width()    { return 0; }
    long Height()   { return 0; }
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
private:
    CMediaType m_mt;
};


// handles some standard WAVEFORMATEX wave formats
class WaveHandler : public TypeHandler
{
public:
    WaveHandler(const CMediaType* pmt)
    : m_mt(*pmt)
    {}

    DWORD Handler() 
    {
        return 'soun';
    }
    void WriteTREF(Atom* patm) {UNREFERENCED_PARAMETER(patm);}
    bool IsVideo() 
    {
        return false;
    }
    bool IsAudio()
    { 
        return true;
    }
    long SampleRate()
    {
        return 50;
    }
    bool CanTruncate();
    bool Truncate(IMediaSample* pSample, REFERENCE_TIME tNewStart);

    long Scale();
    long Width()    { return 0; }
    long Height()   { return 0; }
    void WriteDescriptor(Atom* patm, int id, int dataref, long scale);
private:
    CMediaType m_mt;
};

// -----------------------------------------------------------------------------------------

const int WAVE_FORMAT_AAC = 0x00ff;
const int WAVE_FORMAT_AACEncoder = 0x1234;

// Broadcom/Cyberlink Byte-Stream H264 subtype
// CLSID_H264
class DECLSPEC_UUID("8D2D71CB-243F-45E3-B2D8-5FD7967EC09B") CLSID_H264_BSF;

//static 
bool
TypeHandler::CanSupport(const CMediaType* pmt)
{
    if (*pmt->Type() == MEDIATYPE_Video)
    {
        // divx
        FOURCCMap divx(DWORD('xvid'));
        FOURCCMap xvidCaps(DWORD('XVID'));
        FOURCCMap divxCaps(DWORD('DIVX'));
        FOURCCMap dx50(DWORD('05XD'));
        if (((*pmt->Subtype() == divx) || 
            (*pmt->Subtype() == divxCaps) ||
            (*pmt->Subtype() == xvidCaps) ||
            (*pmt->Subtype() == dx50)) 
                &&
                (*pmt->FormatType() == FORMAT_VideoInfo))
        {
            return true;
        }

        FOURCCMap x264(DWORD('462x'));
        FOURCCMap H264(DWORD('462H'));
        FOURCCMap h264(DWORD('462h'));
        FOURCCMap avc1(DWORD('1CVA'));

        // H264 BSF
        if ((*pmt->Subtype() == x264) || 
            (*pmt->Subtype() == H264) ||
            (*pmt->Subtype() == h264) ||
            (*pmt->Subtype() == avc1) ||
            (*pmt->Subtype() == __uuidof(CLSID_H264_BSF)))
        {
            // BSF
            if ((*pmt->FormatType() == FORMAT_VideoInfo) || (*pmt->FormatType() == FORMAT_VideoInfo2))
            {
                return true;
            }
            // length-prepended
            if (*pmt->FormatType() == FORMAT_MPEG2Video)
            {
                return true;
            }
        }

        // uncompressed
        // it would be nice to select any uncompressed type eg by checking that
        // the bitcount and biSize match up with the dimensions, but that
        // also works for ffdshow encoder outputs, so I'm returning to an 
        // explicit list. 
        FOURCCMap fcc(pmt->subtype.Data1);
        if ((fcc == *pmt->Subtype()) && (*pmt->FormatType() == FORMAT_VideoInfo))
        {
            VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->Format();
            if ((pvi->bmiHeader.biBitCount > 0) && (DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize()))
            {
                FOURCCMap yuy2(DWORD('2YUY'));
                FOURCCMap uyvy(DWORD('YVYU'));
                FOURCCMap yv12(DWORD('21VY'));
                FOURCCMap nv12(DWORD('21VN'));
                FOURCCMap i420(DWORD('024I'));
                if ((*pmt->Subtype() == yuy2) ||
                    (*pmt->Subtype() == uyvy) ||
                    (*pmt->Subtype() == yv12) ||
                    (*pmt->Subtype() == nv12) ||
//                  (*pmt->Subtype() == MEDIASUBTYPE_RGB32) ||
//                  (*pmt->Subtype() == MEDIASUBTYPE_RGB24) ||
                    (*pmt->Subtype() == i420)
                    )
                {
                    return true;
                }
            }
        }
    } else if (*pmt->Type() == MEDIATYPE_Audio)
    {
        // rely on format tag to identify formats -- for 
        // this, subtype adds nothing

        if (*pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            // CoreAAC decoder
            WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->Format();
            if ((pwfx->wFormatTag == WAVE_FORMAT_AAC) || 
                (pwfx->wFormatTag == WAVE_FORMAT_AACEncoder))
            {
                return true;
            }

            if ((pwfx->wFormatTag == WAVE_FORMAT_PCM) ||
                (pwfx->wFormatTag == WAVE_FORMAT_ALAW) ||
                (pwfx->wFormatTag == WAVE_FORMAT_MULAW))
            {
                return true;
            }
        }
    }
    return false;
}

//static 
TypeHandler* 
TypeHandler::Make(const CMediaType* pmt)
{
    if (!CanSupport(pmt))
    {
        return NULL;
    }
    if (*pmt->Type() == MEDIATYPE_Video)
    {
        // divx
        FOURCCMap divx(DWORD('xvid'));
        FOURCCMap xvidCaps(DWORD('XVID'));
        FOURCCMap divxCaps(DWORD('DIVX'));
        FOURCCMap dx50(DWORD('05XD'));
        if ((*pmt->Subtype() == divx) || 
            (*pmt->Subtype() == divxCaps) ||
            (*pmt->Subtype() == xvidCaps) ||
            (*pmt->Subtype() == dx50)) 
        {
            return new DivxHandler(pmt);
        }

        FOURCCMap x264(DWORD('462x'));
        FOURCCMap H264(DWORD('462H'));
        FOURCCMap h264(DWORD('462h'));
        FOURCCMap avc1(DWORD('1CVA'));

        // H264
        if ((*pmt->Subtype() == x264) || 
            (*pmt->Subtype() == H264) ||
            (*pmt->Subtype() == h264) ||
            (*pmt->Subtype() == avc1) ||
            (*pmt->Subtype() == __uuidof(CLSID_H264_BSF)))
        {
            // BSF
            if ((*pmt->FormatType() == FORMAT_VideoInfo) || (*pmt->FormatType() == FORMAT_VideoInfo2))
            {
                return new H264ByteStreamHandler(pmt);
            }
            // length-prepended
            if (*pmt->FormatType() == FORMAT_MPEG2Video)
            {
                return new H264Handler(pmt);
            }
        }

        // other: uncompressed (checked in CanSupport)
        FOURCCMap fcc(pmt->subtype.Data1);
        if ((fcc == *pmt->Subtype()) && (*pmt->FormatType() == FORMAT_VideoInfo))
        {
            VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)pmt->Format();
            if ((pvi->bmiHeader.biBitCount > 0) && (DIBSIZE(pvi->bmiHeader) == pmt->GetSampleSize()))
            {
                return new YUVVideoHandler(pmt);
            }
        }

    } else if (*pmt->Type() == MEDIATYPE_Audio)
    {
        // rely on format tag to identify formats -- for 
        // this, subtype adds nothing

        if (*pmt->FormatType() == FORMAT_WaveFormatEx)
        {
            // CoreAAC decoder
            WAVEFORMATEX* pwfx = (WAVEFORMATEX*)pmt->Format();
            if ((pwfx->wFormatTag == WAVE_FORMAT_AAC) || 
                (pwfx->wFormatTag == WAVE_FORMAT_AACEncoder))
            {
                return new AACHandler(pmt);
            }

            if ((pwfx->wFormatTag == WAVE_FORMAT_PCM) ||
                (pwfx->wFormatTag == WAVE_FORMAT_ALAW) ||
                (pwfx->wFormatTag == WAVE_FORMAT_MULAW))
            {
                return new WaveHandler(pmt);
            }
        }
    }
    return NULL;
}

HRESULT 
TypeHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
    *pcActual = cBytes;
    return patm->Append(pData, cBytes);
}


// -------------------------------------------
DivxHandler::DivxHandler(const CMediaType* pmt)
: m_mt(*pmt),
  m_cConfig(0)
{
    if ((*m_mt.FormatType() == FORMAT_VideoInfo) && 
        (m_mt.FormatLength() > sizeof(VIDEOINFOHEADER)))
    {
        m_cConfig = m_mt.FormatLength() - sizeof(VIDEOINFOHEADER);
        m_pConfig = new BYTE[m_cConfig];
        const BYTE* pExtra = m_mt.Format() + sizeof(VIDEOINFOHEADER);
        CopyMemory(m_pConfig, pExtra, m_cConfig);
    }
}

long 
DivxHandler::Width()
{
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    return pvi->bmiHeader.biWidth;
}

long 
DivxHandler::Height()
{
    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    return abs(pvi->bmiHeader.biHeight);
}

void 
DivxHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    smart_ptr<Atom> psd = patm->CreateAtom('mp4v');

    VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
    int width = pvi->bmiHeader.biWidth;
    int height = abs(pvi->bmiHeader.biHeight);

    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(width, b+24);
    WriteShort(height, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('esds');
    WriteLong(0, b);        // ver/flags
    pesd->Append(b, 4);

    // es descr
    //      decoder config
    //          <objtype/stream type/bitrates>
    //          decoder specific info desc
    //      sl descriptor
    Descriptor es(Descriptor::ES_Desc);
    WriteShort(id, b);
    b[2] = 0;
    es.Append(b, 3);
    Descriptor dcfg(Descriptor::Decoder_Config);
    b[0] = 0x20;    //mpeg-4 video
    b[1] = (4 << 2) | 1;    // video stream

    // buffer size 15000
    b[2] = 0;
    b[3] = 0x3a;
    b[4] = 0x98;
    WriteLong(1500000, b+5);    // max bitrate
    WriteLong(0, b+9);          // avg bitrate 0 = variable
    dcfg.Append(b, 13);
    Descriptor dsi(Descriptor::Decoder_Specific_Info);

    dsi.Append(m_pConfig, m_cConfig);
    dcfg.Append(&dsi);
    es.Append(&dcfg);
    Descriptor sl(Descriptor::SL_Config);
    b[0] = 2;
    sl.Append(b, 2);
    es.Append(&sl);
    es.Write(pesd);
    pesd->Close();

    psd->Close();
}

inline bool NextStartCode(const BYTE*&pBuffer, long& cBytes)
{
    while ((cBytes >= 4) &&
           (*(UNALIGNED DWORD *)pBuffer & 0x00FFFFFF) != 0x00010000) {
        cBytes--;
        pBuffer++;
    }
    return cBytes >= 4;
}

HRESULT 
DivxHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
    if (m_cConfig == 0)
    {
        const BYTE* p = pData;
        long c = cBytes;
        const BYTE* pVOL = NULL;
        while (NextStartCode(p, c))
        {
            if (pVOL == NULL)
            {
                if (p[3] == 0x20)
                {
                    pVOL = p;
                }
            }
            else
            {
                m_cConfig = long(p - pVOL);
                m_pConfig = new BYTE[m_cConfig];
                CopyMemory(m_pConfig, pVOL, m_cConfig);
                break;
            }
            p += 4;
            c -= 4;
        }
    }
    return __super::WriteData(patm, pData, cBytes, pcActual);
}

long 
AACHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

void 
AACHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    smart_ptr<Atom> psd = patm->CreateAtom('mp4a');

    BYTE b[28];
    ZeroMemory(b, 28);
    WriteShort(dataref, b+6);
    WriteShort(2, b+16);
    WriteShort(16, b+18);
    WriteShort(unsigned short(scale), b+24);    // this is what forces us to use short audio scales
    psd->Append(b, 28);

    smart_ptr<Atom> pesd = psd->CreateAtom('esds');
    WriteLong(0, b);        // ver/flags
    pesd->Append(b, 4);
    // es descr
    //      decoder config
    //          <objtype/stream type/bitrates>
    //          decoder specific info desc
    //      sl descriptor
    Descriptor es(Descriptor::ES_Desc);
    WriteShort(id, b);
    b[2] = 0;
    es.Append(b, 3);
    Descriptor dcfg(Descriptor::Decoder_Config);
    b[0] = 0x40;    // AAC audio
    b[1] = (5 << 2) | 1;    // audio stream

    // buffer size 15000
    b[2] = 0;
    b[3] = 0x3a;
    b[4] = 0x98;
    WriteLong(1500000, b+5);    // max bitrate
    WriteLong(0, b+9);          // avg bitrate 0 = variable
    dcfg.Append(b, 13);
    Descriptor dsi(Descriptor::Decoder_Specific_Info);
    BYTE* pExtra = m_mt.Format() + sizeof(WAVEFORMATEX);
    long cExtra = m_mt.FormatLength() - sizeof(WAVEFORMATEX);
    if (cExtra > 0)
    {
        dsi.Append(pExtra, cExtra);
    }
    dcfg.Append(&dsi);
    es.Append(&dcfg);
    Descriptor sl(Descriptor::SL_Config);
    b[0] = 2;
    sl.Append(b, 2);
    es.Append(&sl);
    es.Write(pesd);
    pesd->Close();
    psd->Close();
}
    
LONGLONG 
H264Handler::FrameDuration()
{
    MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
    return pvi->hdr.AvgTimePerFrame;
}

long 
H264Handler::Width()
{
    MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
    return pvi->hdr.bmiHeader.biWidth;
}

long 
H264Handler::Height()
{
    MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
    return abs(pvi->hdr.bmiHeader.biHeight);
}

void 
H264Handler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(id);
    smart_ptr<Atom> psd = patm->CreateAtom('avc1');

    MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
    int width = pvi->hdr.bmiHeader.biWidth;
    int height = abs(pvi->hdr.bmiHeader.biHeight);


    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(width, b+24);
    WriteShort(height, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('avcC');
    b[0] = 1;           // version 1
    b[1] = (BYTE)pvi->dwProfile;
    b[2] = 0;
    b[3] = (BYTE)pvi->dwLevel;
    // length of length-preceded nalus
    b[4] = BYTE(0xfC | (pvi->dwFlags - 1));
    b[5] = 0xe1;        // 1 SPS

    // SPS
    const BYTE* p = (const BYTE*)&pvi->dwSequenceHeader;
    const BYTE* pEnd = p + pvi->cbSequenceHeader;
    int c = (p[0] << 8) | p[1];
    // extract profile/level compat from SPS
    b[2] = p[4];
    pesd->Append(b, 6);
    pesd->Append(p, 2+c);
    int type = p[2] & 0x1f;
    while ((p < pEnd) && (type != 8))
    {
        p += 2+c;
        c = (p[0] << 8) | p[1];
        type = p[2] & 0x1f;
    }
    if ((type == 8) && ((p+2+c) <= pEnd))
    {
        b[0] = 1;   // 1 PPS
        pesd->Append(b, 1);
        pesd->Append(p, 2+c);
    }
    pesd->Close();

    psd->Close();
}

struct QTVideo 
{
    BYTE    reserved1[6];       // 0
    USHORT  dataref;
    
    USHORT  version;            // 0
    USHORT  revision;           // 0
    ULONG   vendor;

    ULONG   temporal_compression;
    ULONG   spatial_compression;

    USHORT  width;
    USHORT  height;
    
    ULONG   horz_resolution;    // 00 48 00 00
    ULONG   vert_resolution;    // 00 48 00 00
    ULONG   reserved2;          // 0
    USHORT  frames_per_sample;  // 1
    BYTE    codec_name[32];     // pascal string - ascii, first byte is char count
    USHORT  bit_depth;
    USHORT  colour_table_id;        // ff ff
};

inline USHORT Swap2Bytes(int x)
{
    return (USHORT) (((x & 0xff) << 8) | ((x >> 8) & 0xff));
}
inline DWORD Swap4Bytes(DWORD x)
{
    return ((x & 0xff) << 24) |
           ((x & 0xff00) << 8) |
           ((x & 0xff0000) >> 8) |
           ((x >> 24) & 0xff);
}

long 
YUVVideoHandler::Width()
{
    if (*m_mt.FormatType() == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
        return pvi->bmiHeader.biWidth;
    }
    else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
    {
        VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
        return pvi->bmiHeader.biWidth;
    }
    else
    {
        return 0;
    }
}

long 
YUVVideoHandler::Height()
{
    if (*m_mt.FormatType() == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
        return abs(pvi->bmiHeader.biHeight);
    }
    else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
    {
        VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
        return abs(pvi->bmiHeader.biHeight);
    }
    else
    {
        return 0;
    }
}

void
YUVVideoHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(dataref);
    UNREFERENCED_PARAMETER(id);

    FOURCCMap fcc = m_mt.Subtype();
    smart_ptr<Atom> psd = patm->CreateAtom(Swap4Bytes(fcc.GetFOURCC()));

    int cx, cy, depth;
    if (*m_mt.FormatType() == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
        cx = pvi->bmiHeader.biWidth;
        cy = abs(pvi->bmiHeader.biHeight);
        depth = pvi->bmiHeader.biBitCount;
    }
    else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
    {
        VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
        cx = pvi->bmiHeader.biWidth;
        cy = abs(pvi->bmiHeader.biHeight);
        depth = pvi->bmiHeader.biBitCount;
    }
    else
    {
        return;
    }

    QTVideo fmt;
    ZeroMemory(&fmt, sizeof(fmt));
    // remember we must byte-swap all data
    fmt.width = Swap2Bytes(cx);
    fmt.height = Swap2Bytes(cy);
    fmt.bit_depth = Swap2Bytes(depth);

    fmt.reserved1[5] = 1;
    fmt.vert_resolution = fmt.horz_resolution = 0x480000;
    fmt.frames_per_sample  = Swap2Bytes(1);
    fmt.colour_table_id = 0xffff;

    // pascal string codec name
    const char* pName = "YUV Video";
    int cch = lstrlenA(pName);
    CopyMemory(&fmt.codec_name[1], pName, cch);
    fmt.codec_name[0] = (BYTE)cch;

    psd->Append((const BYTE*)&fmt, sizeof(fmt));

    psd->Close();
}

long 
WaveHandler::Scale()
{
    // for audio, the scale should be the sampling rate but
    // must not exceed 65535
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->nSamplesPerSec > 65535)
    {
        return 45000;
    }
    else
    {
        return pwfx->nSamplesPerSec;
    }
}

void 
WaveHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    DWORD dwAtom = 0;
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
    {
        dwAtom = 'lpcm';
    } else if (pwfx->wFormatTag == WAVE_FORMAT_MULAW)
    {
        dwAtom = 'ulaw';
    } else if (pwfx->wFormatTag == WAVE_FORMAT_ALAW)
    {
        dwAtom = 'alaw';
    }
    smart_ptr<Atom> psd = patm->CreateAtom(dwAtom);

    BYTE b[28];
    ZeroMemory(b, 28);
    WriteShort(dataref, b+6);
    WriteShort(2, b+16);
    WriteShort(16, b+18);
    WriteShort(unsigned short(scale), b+24);    // this is what forces us to use short audio scales
    psd->Append(b, 28);

    smart_ptr<Atom> pesd = psd->CreateAtom('esds');
    WriteLong(0, b);        // ver/flags
    pesd->Append(b, 4);
    // es descr
    //      decoder config
    //          <objtype/stream type/bitrates>
    //          decoder specific info desc
    //      sl descriptor
    Descriptor es(Descriptor::ES_Desc);
    WriteShort(id, b);
    b[2] = 0;
    es.Append(b, 3);
    Descriptor dcfg(Descriptor::Decoder_Config);
    b[0] = 0xC0;    // custom object type
    b[1] = (5 << 2) | 1;    // audio stream

    // buffer size 15000
    b[2] = 0;
    b[3] = 0x3a;
    b[4] = 0x98;
    WriteLong(1500000, b+5);    // max bitrate
    WriteLong(0, b+9);          // avg bitrate 0 = variable
    dcfg.Append(b, 13);
    Descriptor dsi(Descriptor::Decoder_Specific_Info);

    // write whole WAVEFORMATEX as decoder specific info
    int cLen = pwfx->cbSize + sizeof(WAVEFORMATEX);
    dsi.Append((const BYTE*)pwfx, cLen);
    dcfg.Append(&dsi);
    es.Append(&dcfg);
    Descriptor sl(Descriptor::SL_Config);
    b[0] = 2;
    sl.Append(b, 2);
    es.Append(&sl);
    es.Write(pesd);
    pesd->Close();
    psd->Close();
}

bool 
WaveHandler::CanTruncate()
{
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
    {
        return true;
    }
    return false;
}

bool 
WaveHandler::Truncate(IMediaSample* pSample, REFERENCE_TIME tNewStart)
{
    if (!CanTruncate())
    {
        return false;
    }
    REFERENCE_TIME tStart, tEnd;
    if (pSample->GetTime(&tStart, &tEnd) != S_OK)
    {
        return false;
    }
    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_mt.Format();
    LONGLONG tDiff = tNewStart - tStart;
    long cBytesExcess = long (tDiff * pwfx->nSamplesPerSec / UNITS) * pwfx->nBlockAlign;
    long cData = pSample->GetActualDataLength();
    BYTE* pBuffer;
    pSample->GetPointer(&pBuffer);
    MoveMemory(pBuffer, pBuffer+cBytesExcess, cData - cBytesExcess);
    pSample->SetActualDataLength(cData - cBytesExcess);
    pSample->SetTime(&tNewStart, &tEnd);
    return true;

}

// ---- descriptor ------------------------

Descriptor::Descriptor(TagType type)
: m_type(type),
  m_cBytes(0),
  m_cValid(0)
{
}

void
Descriptor::Append(const BYTE* pBuffer, long cBytes)
{
    Reserve(cBytes);
    CopyMemory(m_pBuffer+m_cValid, pBuffer, cBytes);
    m_cValid += cBytes;
}

void
Descriptor::Reserve(long cBytes)
{
    if ((m_cValid + cBytes) > m_cBytes)
    {
        // increment memory in 128 byte chunks
        long inc = ((cBytes+127)/128) * 128;
        smart_array<BYTE> pNew = new BYTE[m_cBytes + inc];
        if (m_cValid > 0)
        {
            CopyMemory(pNew, m_pBuffer, m_cValid);
        }
        m_pBuffer = pNew;
        m_cBytes += inc;
    }
}

void
Descriptor::Append(Descriptor* pdesc)
{
    long cBytes = pdesc->Length();
    Reserve(cBytes);
    pdesc->Write(m_pBuffer + m_cValid);
    m_cValid += cBytes;
}

long 
Descriptor::Length()
{
    long cHdr = 2;
    long cBody = m_cValid;
    while (cBody > 0x7f)
    {
        cHdr++;
        cBody >>= 7;
    }
    return cHdr + m_cValid;

}

void 
Descriptor::Write(BYTE* pBuffer)
{
    int idx = 0;
    pBuffer[idx++] = (BYTE) m_type;
    long cBody = m_cValid;
    while (cBody)
    {
        BYTE b = BYTE(cBody & 0x7f);
        if (cBody > 0x7f)
        {
            b |= 0x80;
        }
        pBuffer[idx++] = b;
        cBody >>= 7;
    }
    CopyMemory(pBuffer + idx, m_pBuffer, m_cValid);
}

HRESULT 
Descriptor::Write(Atom* patm)
{
    long cBytes = Length();
    smart_array<BYTE> ptemp = new BYTE[cBytes];
    Write(ptemp);
    return patm->Append(ptemp, cBytes);
}

// --- H264 BSF support --------------
H264ByteStreamHandler::H264ByteStreamHandler(const CMediaType* pmt)
: H264Handler(pmt),
  m_bSPS(false),
  m_bPPS(false)
{
    if (*m_mt.FormatType() == FORMAT_MPEG2Video)
    {
        MPEG2VIDEOINFO* pvi = (MPEG2VIDEOINFO*)m_mt.Format();
        m_tFrame = pvi->hdr.AvgTimePerFrame;
        m_cx = pvi->hdr.bmiHeader.biWidth;
        m_cy = pvi->hdr.bmiHeader.biHeight;
    }
    else if (*m_mt.FormatType() == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER* pvi = (VIDEOINFOHEADER*)m_mt.Format();
        m_tFrame = pvi->AvgTimePerFrame;
        m_cx = pvi->bmiHeader.biWidth;
        m_cy = pvi->bmiHeader.biHeight;
    }
    else if (*m_mt.FormatType() == FORMAT_VideoInfo2)
    {
        VIDEOINFOHEADER2* pvi = (VIDEOINFOHEADER2*)m_mt.Format();
        m_tFrame = pvi->AvgTimePerFrame;
        m_cx = pvi->bmiHeader.biWidth;
        m_cy = pvi->bmiHeader.biHeight;
    }
    else
    {
        m_tFrame = m_cx = m_cy = 0;
    }
}

void 
H264ByteStreamHandler::WriteDescriptor(Atom* patm, int id, int dataref, long scale)
{
    UNREFERENCED_PARAMETER(scale);
    UNREFERENCED_PARAMETER(id);
    smart_ptr<Atom> psd = patm->CreateAtom('avc1');

    // locate param sets in parse buffer
    NALUnit sps, pps;
    NALUnit nal;
    const BYTE* pBuffer = m_ParamSets.Data();
    long cBytes = m_ParamSets.Size();
    while (nal.Parse(pBuffer, cBytes, nalunit_length_field, true))
    {
        if (nal.Type() == NALUnit::NAL_Sequence_Params)
        {
            sps = nal;
        }
        else if (nal.Type() == NALUnit::NAL_Picture_Params)
        {
            pps = nal;
        }
        const BYTE* pNext = nal.Start() + nal.Length();
        cBytes-= long(pNext - pBuffer);
        pBuffer = pNext;
    }

    SeqParamSet seq;
    seq.Parse(&sps);

    BYTE b[78];
    ZeroMemory(b, 78);
    WriteShort(dataref, b+6);
    WriteShort(m_cx, b+24);
    WriteShort(m_cy, b+26);
    b[29] = 0x48;
    b[33] = 0x48;
    b[41] = 1;
    b[75] = 24;
    WriteShort(-1, b+76);
    psd->Append(b, 78);

    smart_ptr<Atom> pesd = psd->CreateAtom('avcC');
    b[0] = 1;           // version 1
    b[1] = (BYTE)seq.Profile();
    b[2] = seq.Compat();
    b[3] = (BYTE)seq.Level();
    // length of length-preceded nalus
    b[4] = BYTE(0xfC | (nalunit_length_field - 1));

    b[5] = 0xe1;        // 1 SPS

    // in the descriptor, the length field for param set nalus is always 2
    pesd->Append(b, 6);
    WriteVariable(sps.Length(), b, 2);
    pesd->Append(b, 2);
    pesd->Append(sps.Start(), sps.Length());

    b[0] = 1;   // 1 PPS
    WriteVariable(pps.Length(), b+1, 2);
    pesd->Append(b, 3);
    pesd->Append(pps.Start(), pps.Length());

    pesd->Close();
    psd->Close();
}

LONGLONG 
H264ByteStreamHandler::FrameDuration()
{
    return m_tFrame;
}

HRESULT 
H264ByteStreamHandler::WriteData(Atom* patm, const BYTE* pData, int cBytes, int* pcActual)
{
    int cActual = 0;

    NALUnit nal;
    while(nal.Parse(pData, cBytes, 0, true))
    {
        const BYTE* pNext = nal.Start() + nal.Length();
        cBytes-= long(pNext - pData);
        pData = pNext;

        // convert length to correct byte order
        BYTE length[nalunit_length_field];
        WriteVariable(nal.Length(), length, nalunit_length_field);

        if (!m_bSPS && (nal.Type() == NALUnit::NAL_Sequence_Params))
        {
            // store in length-preceded format for use in WriteDescriptor
            m_bSPS = true;
            m_ParamSets.Append(length, nalunit_length_field);
            m_ParamSets.Append(nal.Start(), nal.Length());
        }
        else if (!m_bPPS && (nal.Type() == NALUnit::NAL_Picture_Params))
        {
            // store in length-preceded format for use in WriteDescriptor
            m_bPPS = true;
            m_ParamSets.Append(length, nalunit_length_field);
            m_ParamSets.Append(nal.Start(), nal.Length());
        }

        // write length and data to file
        patm->Append(length, nalunit_length_field);
        patm->Append(nal.Start(), nal.Length());
        cActual += nalunit_length_field + nal.Length();
    }

    *pcActual = cActual;
    return S_OK;
}

