/* This code comes from MythTV.
For now, integration with ccextractor is a quick hack. It could get better with time. */
#include "../platform.h"
#include "ccextractor.h"
#include "608.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

LONG process_block (unsigned char *data, LONG length);
extern LONG _result;

unsigned int header_state;
unsigned char psm_es_type[256];
int cc608_parity_table[256];

#define AVERROR_IO          (-2)

#define VBI_TYPE_TELETEXT 	0x1 	// Teletext (uses lines 6-22 for PAL, 10-21 for NTSC)
#define VBI_TYPE_CC 		0x4 	// Closed Captions (line 21 NTSC, line 22 PAL)
#define VBI_TYPE_WSS 		0x5 	// Wide Screen Signal (line 20 NTSC, line 23 PAL)
#define VBI_TYPE_VPS 		0x7 	// Video Programming System (PAL) (line 16)

#define MAX_SYNC_SIZE 100000
#define PACK_START_CODE             ((unsigned int)0x000001ba)
#define SYSTEM_HEADER_START_CODE    ((unsigned int)0x000001bb)
#define SEQUENCE_END_CODE           ((unsigned int)0x000001b7)
#define PACKET_START_CODE_MASK      ((unsigned int)0xffffff00)
#define PACKET_START_CODE_PREFIX    ((unsigned int)0x00000100)
#define ISO_11172_END_CODE          ((unsigned int)0x000001b9)

#define AV_NOPTS_VALUE          (int64_t)(0x8000000000000000)

/* mpeg2 */
#define PROGRAM_STREAM_MAP 0x1bc
#define PRIVATE_STREAM_1   0x1bd
#define PADDING_STREAM     0x1be
#define PRIVATE_STREAM_2   0x1bf

extern struct s_write wbout1, wbout2; // Output structures
#define AUDIO_ID 0xc0
#define VIDEO_ID 0xe0
#define AC3_ID   0x80
#define DTS_ID   0x8a
#define LPCM_ID  0xa0
#define SUB_ID   0x20

#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x8a

enum CodecType {
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
    CODEC_TYPE_DATA,
    CODEC_TYPE_SUBTITLE,
};




enum CodecID {
    CODEC_ID_NONE,
    CODEC_ID_MPEG1VIDEO,
    CODEC_ID_MPEG2VIDEO, /* prefered ID for MPEG Video 1 or 2 decoding */
    CODEC_ID_MPEG2VIDEO_XVMC,
    CODEC_ID_MPEG2VIDEO_XVMC_VLD,
    CODEC_ID_H261,
    CODEC_ID_H263,
    CODEC_ID_RV10,
    CODEC_ID_RV20,
    CODEC_ID_MJPEG,
    CODEC_ID_MJPEGB,
    CODEC_ID_LJPEG,
    CODEC_ID_SP5X,
    CODEC_ID_JPEGLS,
    CODEC_ID_MPEG4,
    CODEC_ID_RAWVIDEO,
    CODEC_ID_MSMPEG4V1,
    CODEC_ID_MSMPEG4V2,
    CODEC_ID_MSMPEG4V3,
    CODEC_ID_WMV1,
    CODEC_ID_WMV2,
    CODEC_ID_H263P,
    CODEC_ID_H263I,
    CODEC_ID_FLV1,
    CODEC_ID_SVQ1,
    CODEC_ID_SVQ3,
    CODEC_ID_DVVIDEO,
    CODEC_ID_HUFFYUV,
    CODEC_ID_CYUV,
    CODEC_ID_H264,
    CODEC_ID_INDEO3,
    CODEC_ID_VP3,
    CODEC_ID_THEORA,
    CODEC_ID_ASV1,
    CODEC_ID_ASV2,
    CODEC_ID_FFV1,
    CODEC_ID_4XM,
    CODEC_ID_VCR1,
    CODEC_ID_CLJR,
    CODEC_ID_MDEC,
    CODEC_ID_ROQ,
    CODEC_ID_INTERPLAY_VIDEO,
    CODEC_ID_XAN_WC3,
    CODEC_ID_XAN_WC4,
    CODEC_ID_RPZA,
    CODEC_ID_CINEPAK,
    CODEC_ID_WS_VQA,
    CODEC_ID_MSRLE,
    CODEC_ID_MSVIDEO1,
    CODEC_ID_IDCIN,
    CODEC_ID_8BPS,
    CODEC_ID_SMC,
    CODEC_ID_FLIC,
    CODEC_ID_TRUEMOTION1,
    CODEC_ID_VMDVIDEO,
    CODEC_ID_MSZH,
    CODEC_ID_ZLIB,
    CODEC_ID_QTRLE,
    CODEC_ID_SNOW,
    CODEC_ID_TSCC,
    CODEC_ID_ULTI,
    CODEC_ID_QDRAW,
    CODEC_ID_VIXL,
    CODEC_ID_QPEG,
    CODEC_ID_XVID,
    CODEC_ID_PNG,
    CODEC_ID_PPM,
    CODEC_ID_PBM,
    CODEC_ID_PGM,
    CODEC_ID_PGMYUV,
    CODEC_ID_PAM,
    CODEC_ID_FFVHUFF,
    CODEC_ID_RV30,
    CODEC_ID_RV40,
    CODEC_ID_VC1,
    CODEC_ID_WMV3,
    CODEC_ID_LOCO,
    CODEC_ID_WNV1,
    CODEC_ID_AASC,
    CODEC_ID_INDEO2,
    CODEC_ID_FRAPS,
    CODEC_ID_TRUEMOTION2,
    CODEC_ID_BMP,
    CODEC_ID_CSCD,
    CODEC_ID_MMVIDEO,
    CODEC_ID_ZMBV,
    CODEC_ID_AVS,
    CODEC_ID_SMACKVIDEO,
    CODEC_ID_NUV,
    CODEC_ID_KMVC,
    CODEC_ID_FLASHSV,
    CODEC_ID_CAVS,

    /* various pcm "codecs" */
    CODEC_ID_PCM_S16LE= 0x10000,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_U16LE,
    CODEC_ID_PCM_U16BE,
    CODEC_ID_PCM_S8,
    CODEC_ID_PCM_U8,
    CODEC_ID_PCM_MULAW,
    CODEC_ID_PCM_ALAW,
    CODEC_ID_PCM_S32LE,
    CODEC_ID_PCM_S32BE,
    CODEC_ID_PCM_U32LE,
    CODEC_ID_PCM_U32BE,
    CODEC_ID_PCM_S24LE,
    CODEC_ID_PCM_S24BE,
    CODEC_ID_PCM_U24LE,
    CODEC_ID_PCM_U24BE,
    CODEC_ID_PCM_S24DAUD,

    /* various adpcm codecs */
    CODEC_ID_ADPCM_IMA_QT= 0x11000,
    CODEC_ID_ADPCM_IMA_WAV,
    CODEC_ID_ADPCM_IMA_DK3,
    CODEC_ID_ADPCM_IMA_DK4,
    CODEC_ID_ADPCM_IMA_WS,
    CODEC_ID_ADPCM_IMA_SMJPEG,
    CODEC_ID_ADPCM_MS,
    CODEC_ID_ADPCM_4XM,
    CODEC_ID_ADPCM_XA,
    CODEC_ID_ADPCM_ADX,
    CODEC_ID_ADPCM_EA,
    CODEC_ID_ADPCM_G726,
    CODEC_ID_ADPCM_CT,
    CODEC_ID_ADPCM_SWF,
    CODEC_ID_ADPCM_YAMAHA,
    CODEC_ID_ADPCM_SBPRO_4,
    CODEC_ID_ADPCM_SBPRO_3,
    CODEC_ID_ADPCM_SBPRO_2,

    /* AMR */
    CODEC_ID_AMR_NB= 0x12000,
    CODEC_ID_AMR_WB,

    /* RealAudio codecs*/
    CODEC_ID_RA_144= 0x13000,
    CODEC_ID_RA_288,

    /* various DPCM codecs */
    CODEC_ID_ROQ_DPCM= 0x14000,
    CODEC_ID_INTERPLAY_DPCM,
    CODEC_ID_XAN_DPCM,
    CODEC_ID_SOL_DPCM,

    CODEC_ID_MP2= 0x15000,
    CODEC_ID_MP3, /* prefered ID for MPEG Audio layer 1, 2 or3 decoding */
    CODEC_ID_AAC,
    CODEC_ID_MPEG4AAC,
    CODEC_ID_AC3,
    CODEC_ID_DTS,
    CODEC_ID_VORBIS,
    CODEC_ID_DVAUDIO,
    CODEC_ID_WMAV1,
    CODEC_ID_WMAV2,
    CODEC_ID_MACE3,
    CODEC_ID_MACE6,
    CODEC_ID_VMDAUDIO,
    CODEC_ID_SONIC,
    CODEC_ID_SONIC_LS,
    CODEC_ID_FLAC,
    CODEC_ID_MP3ADU,
    CODEC_ID_MP3ON4,
    CODEC_ID_SHORTEN,
    CODEC_ID_ALAC,
    CODEC_ID_WESTWOOD_SND1,
    CODEC_ID_GSM,
    CODEC_ID_QDM2,
    CODEC_ID_COOK,
    CODEC_ID_TRUESPEECH,
    CODEC_ID_TTA,
    CODEC_ID_SMACKAUDIO,
    CODEC_ID_QCELP,

    /* subtitle codecs */
    CODEC_ID_DVD_SUBTITLE= 0x17000,
    CODEC_ID_DVB_SUBTITLE,

    /* teletext codecs */
    CODEC_ID_MPEG2VBI,
    CODEC_ID_DVB_VBI,

    /* DSMCC codec */
    CODEC_ID_DSMCC_B,

    CODEC_ID_MPEG2TS= 0x20000, /* _FAKE_ codec to indicate a raw MPEG2 transport
                               stream (only used by libavformat) */
};


typedef struct AVPacket {
    LONG pts;                            ///< presentation time stamp in time_base units
    LONG dts;                            ///< decompression time stamp in time_base units
    unsigned char *data;
    int   size;
    int   stream_index;
    int   flags;
    int   duration;                         ///< presentation duration in time_base units (0 if not available)
    void  (*destruct)(struct AVPacket *);
    void  *priv;
    LONG pos;                            ///< byte position in stream, -1 if unknown
    int    codec_id;
    int    type;
} AVPacket;

AVPacket av;

int get_be16()
{
    unsigned char a,b;
    buffered_read_byte (&a);
    buffered_read_byte (&b);
    return (a<<8) | b;
}

int get_byte ()
{
	unsigned char b;
	buffered_read_byte(&b);
    if (_result==1)
        return b;
    else
        return 0;
}

unsigned int get_be32()
{
    unsigned int val;
    val = get_be16() << 16;
    val |= get_be16();
    return val;
}


static LONG get_pts(int c)
{
    LONG pts;
    int val;

    if (c < 0)
        c = get_byte();
    pts = (LONG)((c >> 1) & 0x07) << 30;
    val = get_be16();
    pts |= (LONG) (val >> 1) << 15;
    val = get_be16();
    pts |= (LONG) (val >> 1);
    return pts;
}

static int find_next_start_code(int *size_ptr,
                                unsigned int *header_state)
{
    unsigned int state, v;
    int val, n;

    state = *header_state;
    n = *size_ptr;
    while (n > 0)
    {
        unsigned char cx;
        buffered_read_byte (&cx);
        if (_result!=1)
            break;
        v = cx;
        n--;
        if (state == 0x000001) {
            state = ((state << 8) | v) & 0xffffff;
            val = state;
            goto found;
        }
        state = ((state << 8) | v) & 0xffffff;
    }
    val = -1;
found:
    *header_state = state;
    *size_ptr = n;
    return val;
}

void url_fskip (int length)
{
    buffered_seek (length);
}

static long mpegps_psm_parse(void)
{
    int psm_length, ps_info_length, es_map_length;

    psm_length = get_be16();
    get_byte();
    get_byte();
    ps_info_length = get_be16();

    /* skip program_stream_info */
    url_fskip(ps_info_length);
    es_map_length = get_be16();

    /* at least one es available? */
    while (es_map_length >= 4)
    {
        unsigned char type = (unsigned char) get_byte();
        unsigned char es_id =(unsigned char) get_byte();
        unsigned int es_info_length = get_be16();
        /* remember mapping from stream id to stream type */
        psm_es_type[es_id] = type;
        /* skip program_stream_info */
        url_fskip(es_info_length);
        es_map_length -= 4 + es_info_length;
    }
    get_be32(); /* crc32 */
    return 2 + psm_length;
}


static int mpegps_read_pes_header(int *pstart_code,
                                  LONG *ppts, LONG *pdts)
{
    int len, size, startcode, c, flags, header_len;
    LONG pts, dts;
redo:
    /* next start code (should be immediately after) */
    header_state = 0xff;
    size = MAX_SYNC_SIZE;
    startcode = find_next_start_code(&size, &header_state);
    //printf("startcode=%x pos=0x%Lx\n", startcode, url_ftell(&s->pb));
    if (startcode < 0)
        return AVERROR_IO;
    if (startcode == PACK_START_CODE)
        goto redo;
    if (startcode == SYSTEM_HEADER_START_CODE)
        goto redo;
    if (startcode == PADDING_STREAM ||
        startcode == PRIVATE_STREAM_2)
    {
        /* skip them */
        len = get_be16();
        // url_fskip(len);
        goto redo;
    }

    if (startcode == PROGRAM_STREAM_MAP)
    {
        mpegps_psm_parse();
        goto redo;
    }

    /* find matching stream */
    if (!((startcode >= 0x1c0 && startcode <= 0x1df) ||
        (startcode >= 0x1e0 && startcode <= 0x1ef) ||
        (startcode == 0x1bd)))
        goto redo;

    len = get_be16();
    pts = AV_NOPTS_VALUE;
    dts = AV_NOPTS_VALUE;
    /* stuffing */
    for(;;) {
        if (len < 1)
            goto redo;
        c = get_byte();
        len--;
        /* XXX: for mpeg1, should test only bit 7 */
        if (c != 0xff)
            break;
    }
    if ((c & 0xc0) == 0x40) {
        /* buffer scale & size */
        if (len < 2)
            goto redo;
        get_byte();
        c = get_byte();
        len -= 2;
    }
    if ((c & 0xf0) == 0x20) {
        if (len < 4)
            goto redo;
        dts = pts = get_pts( c);
        len -= 4;
    } else if ((c & 0xf0) == 0x30) {
        if (len < 9)
            goto redo;
        pts = get_pts(c);
        dts = get_pts(-1);
        len -= 9;
    } else if ((c & 0xc0) == 0x80) {
        /* mpeg 2 PES */
#if 0 /* some streams have this field set for no apparent reason */
        if ((c & 0x30) != 0) {
            /* Encrypted multiplex not handled */
            goto redo;
        }
#endif
        flags = get_byte();
        header_len = get_byte();
        len -= 2;
        if (header_len > len)
            goto redo;
        if ((flags & 0xc0) == 0x80) {
            dts = pts = get_pts(-1);
            if (header_len < 5)
                goto redo;
            header_len -= 5;
            len -= 5;
        } if ((flags & 0xc0) == 0xc0) {
            pts = get_pts( -1);
            dts = get_pts( -1);
            if (header_len < 10)
                goto redo;
            header_len -= 10;
            len -= 10;
        }
        len -= header_len;
        while (header_len > 0) {
            get_byte();
            header_len--;
        }
    }
    else if( c!= 0xf )
        goto redo;

    if (startcode == PRIVATE_STREAM_1 /* && psm_es_type[startcode & 0xff] */)
    {
        if (len < 1)
            goto redo;
        startcode = get_byte();
        len--;
        if (startcode >= 0x80 && startcode <= 0xbf) {
            /* audio: skip header */
            if (len < 3)
                goto redo;
            get_byte();
            get_byte();
            get_byte();
            len -= 3;
        }
    }

    *pstart_code = startcode;
    *ppts = pts;
    *pdts = dts;
    return len;
}

static int cc608_good_parity(const int *parity_table, unsigned int data)
{
    int ret = parity_table[data & 0xff] && parity_table[(data & 0xff00) >> 8];
    if (!ret)
    {
        /* VERBOSE(VB_VBI, QString("VBI: Bad parity in EIA-608 data (%1)")
        .arg(data,0,16)); */
    }
    return ret;
}


void ProcessVBIDataPacket()
{
    static const unsigned int min_blank = 6;
    LONG linemask      = 0;
    const unsigned char *meat = av.data;
	unsigned int i;
    if (meat==NULL)
    {
        printf ("Warning: ProcessVBIDataPacket called with NULL data, ignoring.\n");
        return;
    }

    // unsigned long long utc = lastccptsu;

    // [i]tv0 means there is a linemask
    // [I]TV0 means there is no linemask and all lines are present
    if ((meat[0]=='t') && (meat[1]=='v') && (meat[2] == '0'))
    {
        /// TODO this is almost certainly not endian safe....
        memcpy(&linemask, meat + 3, 8);
        meat += 11;
    }
    else if ((meat[0]=='T') && (meat[1]=='V') && (meat[2] == '0'))
    {
        linemask = 0xffffffffffffffff;
        meat += 3;
    }
    else
    {
        /*        VERBOSE(VB_VBI, LOC + QString("Unknown VBI data stream '%1%2%3'")
        .arg(QChar(buf[0])).arg(QChar(buf[1])).arg(QChar(buf[2]))); */
        printf ("Unknown VBI data stream\n");
        return;
    }
    for (i = 0; i < 36; i++)
    {
		unsigned int line ;
        unsigned int field;
        unsigned int id2;

        if (!((linemask >> i) & 0x1))
            continue;

        line  = ((i < 18) ? i : i-18) + min_blank;
        field = (i<18) ? 0 : 1;
        id2 = *meat & 0xf;

        switch (id2)
        {
        case VBI_TYPE_TELETEXT:
            // SECAM lines  6-23
            // PAL   lines  6-22
            // NTSC  lines 10-21 (rare)
            // ttd->Decode(buf+1, VBI_IVTV);
            break;
        case VBI_TYPE_CC:
            // PAL   line 22 (rare)
            // NTSC  line 21
            if (21 == line)
            {
                int data = (meat[2] << 8) | meat[1];
                if (cc608_good_parity(cc608_parity_table, data))
                {
                    if (field==0)
                    {
                        printdata (meat+1,2,0,0);
                        c1count++;
                    }
                    else
                    {
                        printdata (0,0,meat+1,2);
                        c2count++;
                    }
                }
                // utc += 33367;
            }
            break;
        case VBI_TYPE_VPS: // Video Programming System
            // PAL   line 16
            // ccd608->DecodeVPS(buf+1); // a.k.a. PDC
            break;
        case VBI_TYPE_WSS: // Wide Screen Signal
            // PAL   line 23
            // NTSC  line 20
            // ccd608->DecodeWSS(buf+1);
            break;
        }
        meat += 43;
    }
    // lastccptsu = utc;
}

static int mpegps_read_packet(void)
{
    LONG pts, dts;

    int len, startcode,  type, codec_id = 0, es_type;
redo:
    len = mpegps_read_pes_header(&startcode, &pts, &dts);
    if (len < 0)
        return len;

    /* now find stream */
    /*
    for(i=0;i<s->nb_streams;i++) {
    st = s->streams[i];
    if (st->id == startcode)
    goto found;
    }
    */
    es_type = psm_es_type[startcode & 0xff];
    if(es_type > 0){
        if(es_type == STREAM_TYPE_VIDEO_MPEG1){
            codec_id = CODEC_ID_MPEG2VIDEO;
            type = CODEC_TYPE_VIDEO;
        } else if(es_type == STREAM_TYPE_VIDEO_MPEG2){
            codec_id = CODEC_ID_MPEG2VIDEO;
            type = CODEC_TYPE_VIDEO;
        } else if(es_type == STREAM_TYPE_AUDIO_MPEG1 ||
            es_type == STREAM_TYPE_AUDIO_MPEG2){
                codec_id = CODEC_ID_MP3;
                type = CODEC_TYPE_AUDIO;
        } else if(es_type == STREAM_TYPE_AUDIO_AAC){
            codec_id = CODEC_ID_AAC;
            type = CODEC_TYPE_AUDIO;
        } else if(es_type == STREAM_TYPE_VIDEO_MPEG4){
            codec_id = CODEC_ID_MPEG4;
            type = CODEC_TYPE_VIDEO;
        } else if(es_type == STREAM_TYPE_VIDEO_H264){
            codec_id = CODEC_ID_H264;
            type = CODEC_TYPE_VIDEO;
        } else if(es_type == STREAM_TYPE_AUDIO_AC3){
            codec_id = CODEC_ID_AC3;
            type = CODEC_TYPE_AUDIO;
        } else {
            goto skip;
        }
    }
    else
        if (startcode >= 0x1e0 && startcode <= 0x1ef)
        {
            static const unsigned char avs_seqh[4] = { 0, 0, 1, 0xb0 };
            unsigned char buf[8];
            buffered_read (buf,8);
            // get_buffer(&s->pb, buf, 8);
            buffered_seek (-8);
            if(!memcmp(buf, avs_seqh, 4) && (buf[6] != 0 || buf[7] != 1))
                codec_id = CODEC_ID_CAVS;
            else
                codec_id = CODEC_ID_MPEG2VIDEO;
            type = CODEC_TYPE_VIDEO;
        } else if (startcode >= 0x1c0 && startcode <= 0x1df) {
            type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_MP2;
        } else if (startcode >= 0x80 && startcode <= 0x87) {
            type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_AC3;
        } else if (startcode >= 0x88 && startcode <= 0x9f) {
            type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_DTS;
        } else if (startcode >= 0xa0 && startcode <= 0xbf) {
            type = CODEC_TYPE_AUDIO;
            codec_id = CODEC_ID_PCM_S16BE;
        } else if (startcode >= 0x20 && startcode <= 0x3f) {
            type = CODEC_TYPE_SUBTITLE;
            codec_id = CODEC_ID_DVD_SUBTITLE;
        } else if (startcode == 0x69 || startcode == 0x49) {
            type = CODEC_TYPE_DATA;
            codec_id = CODEC_ID_MPEG2VBI;
        } else {
skip:
            // skip packet
            url_fskip(len);
            goto redo;
        }
        // no stream found: add a new stream
        /* st = av_new_stream(s, startcode);
        if (!st)
        goto skip;
        st->codec->codec_type = type;
        st->codec->codec_id = codec_id;
        if (codec_id != CODEC_ID_PCM_S16BE)
        st->need_parsing = 1;

        // notify the callback of the change in streams
        if (s->streams_changed) {
        s->streams_changed(s->stream_change_data);
        }
        */
// found:
        /* if(st->discard >= AVDISCARD_ALL)
        goto skip; */
        if (startcode >= 0xa0 && startcode <= 0xbf)
        {

            // for LPCM, we just skip the header and consider it is raw
            // audio data
            if (len <= 3)
                goto skip;
            get_byte(); // emphasis (1), muse(1), reserved(1), frame number(5)
            get_byte(); // quant (2), freq(2), reserved(1), channels(3)
            get_byte(); // dynamic range control (0x80 = off)
            len -= 3;
            //freq = (b1 >> 4) & 3;
            //st->codec->sample_rate = lpcm_freq_tab[freq];
            //st->codec->channels = 1 + (b1 & 7);
            //st->codec->bit_rate = st->codec->channels * st->codec->sample_rate * 2;

        }
        // av_new_packet(pkt, len);
        /*
            printf ("Paquete de %lu bytes, codec_id=%d, type=%d\n",(unsigned long) len,
            codec_id, type);
        */
        //get_buffer(fh, pkt->data, pkt->size);
        av.size=len;
        av.data=(unsigned char *) realloc (av.data,av.size);
        if (av.data==NULL)
        {
            printf ("\rNot enough memory, realloc() failed. Giving up.\n");
            exit (-3);
        }
        av.codec_id=codec_id;
        av.type=type;
        buffered_read (av.data,av.size);
        // LSEEK (fh,pkt->size,SEEK_CUR);
        av.pts = pts;
        av.dts = dts;
        // pkt->stream_index = st->index;

#if 0
        av_log(s, AV_LOG_DEBUG, "%d: pts=%0.3f dts=%0.3f size=%d\n",
            pkt->stream_index, pkt->pts / 90000.0, pkt->dts / 90000.0, pkt->size);
#endif

        return 0;
}

static int cc608_parity(unsigned int byte)
{
    int ones = 0,i;

    for (i = 0; i < 7; i++)
    {
        if (byte & (1 << i))
            ones++;
    }

    return ones & 1;
}

static void cc608_build_parity_table(int *parity_table)
{
    unsigned int byte;
    int parity_v;
    for (byte = 0; byte <= 127; byte++)
    {
        parity_v = cc608_parity(byte);
        /* CC uses odd parity (i.e., # of 1's in byte is odd.) */
        parity_table[byte] = parity_v;
        parity_table[byte | 0x80] = !parity_v;
    }
}

void build_parity_table (void)
{
	cc608_build_parity_table(cc608_parity_table);
}

void myth_loop(void)
{
    int rc;
	int has_vbi=0;
	unsigned char *desp ;
	LONG saved;
    av.data=NULL;
    buffer_input = 1;
    if (init_file_buffer())
    {
        printf ("Not enough memory.\n");
        exit (-3);
    }
    desp=(unsigned char *) malloc (65536);
    saved=0;

    while (!processed_enough && (rc=mpegps_read_packet ())==0)
    {
        if (av.codec_id==CODEC_ID_MPEG2VBI && av.type==CODEC_TYPE_DATA)
        {
			if (!has_vbi)
			{
				printf ("\rDetected VBI data, disabling user-data packet analysis (not needed).\n");
				has_vbi=1;
			}
            ProcessVBIDataPacket ();
        }
		/* This needs a lot more testing */
		if (av.codec_id==CODEC_ID_MPEG2VIDEO && av.type==CODEC_TYPE_VIDEO && !has_vbi)
		{
			LONG length = saved+av.size;
			LONG used;
            if (length>65536) // Result of a lazy programmer. Make something decent.
            {
				printf ("Not enough memory. Please report this: 65536 bytes is not enough!");
                exit (-500);
            }
			if (av.pts!=AV_NOPTS_VALUE)
			{
				current_pts=av.pts;
				if (pts_set==0)
					pts_set=1;
			}
            memcpy (desp+saved,av.data,av.size);
            used = process_block(desp,length);
            memmove (desp,desp+used,(unsigned int) (length-used));
            saved=length-used;
		}

        if (inputsize>0)
        {
			int cur_sec;
            LONG at=LSEEK (in, 0, SEEK_CUR);
            int progress = (int) (((at>>8)*100)/(inputsize>>8));
            if (last_reported_progress != progress)
            {
                printf ("\r%3d%%",progress);
                cur_sec = (int) ((c1count + c1count_total) / 29.97);
                printf ("  |  %02d:%02d", cur_sec/60, cur_sec%60);
                last_reported_progress = progress;
            }
        }
    }
    free (av.data);
}
