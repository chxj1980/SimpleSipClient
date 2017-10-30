/* $Id: media.cpp 5654 2017-09-20 04:34:27Z riza $ */
/*
 * Copyright (C) 2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pj/ctype.h>
#include <pjsua2/media.hpp>
#include <pjsua2/types.hpp>
#include <pjsua2/endpoint.hpp>
#include "util.hpp"

using namespace pj;
using namespace std;

#define THIS_FILE		"media.cpp"
#define MAX_FILE_NAMES 		64
#define MAX_DEV_COUNT		64

///////////////////////////////////////////////////////////////////////////////
void MediaFormatAudio::fromPj(const pjmedia_format &format)
{
    if ((format.type != PJMEDIA_TYPE_AUDIO) &&
	(format.detail_type != PJMEDIA_FORMAT_DETAIL_AUDIO))
    {
	type = PJMEDIA_TYPE_UNKNOWN;
	return;
    }

    id = format.id;
    type = format.type;

    /* Detail. */
    clockRate = format.det.aud.clock_rate;
    channelCount = format.det.aud.channel_count;
    frameTimeUsec = format.det.aud.frame_time_usec;
    bitsPerSample = format.det.aud.bits_per_sample;
    avgBps = format.det.aud.avg_bps;
    maxBps = format.det.aud.max_bps;
}

pjmedia_format MediaFormatAudio::toPj() const
{
    pjmedia_format pj_format;

    pj_format.id = id;
    pj_format.type = type;

    pj_format.detail_type = PJMEDIA_FORMAT_DETAIL_AUDIO;
    pj_format.det.aud.clock_rate = clockRate;
    pj_format.det.aud.channel_count = channelCount;
    pj_format.det.aud.frame_time_usec = frameTimeUsec;
    pj_format.det.aud.bits_per_sample = bitsPerSample;
    pj_format.det.aud.avg_bps = avgBps;
    pj_format.det.aud.max_bps = maxBps;

    return pj_format;
}

///////////////////////////////////////////////////////////////////////////////
/* Audio Media operations. */
void ConfPortInfo::fromPj(const pjsua_conf_port_info &port_info)
{
    portId = port_info.slot_id;
    name = pj2Str(port_info.name);
    format.fromPj(port_info.format);
    txLevelAdj = port_info.tx_level_adj;
    rxLevelAdj = port_info.rx_level_adj;

    /*
    format.id = PJMEDIA_FORMAT_PCM;
    format.type = PJMEDIA_TYPE_AUDIO;
    format.clockRate = port_info.clock_rate;
    format.channelCount = port_info.channel_count;
    format.bitsPerSample = port_info.bits_per_sample;
    format.frameTimeUsec = (port_info.samples_per_frame *
			   1000000) /
			   (port_info.clock_rate *
			   port_info.channel_count);

    format.avgBps = format.maxBps = port_info.clock_rate *
				    port_info.channel_count *
				    port_info.bits_per_sample;
    */
    listeners.clear();
    for (unsigned i=0; i<port_info.listener_cnt; ++i) {
	listeners.push_back(port_info.listeners[i]);
    }
}
///////////////////////////////////////////////////////////////////////////////
Media::Media(pjmedia_type med_type)
: type(med_type)
{

}

Media::~Media()
{

}

pjmedia_type Media::getType() const
{
    return type;
}

///////////////////////////////////////////////////////////////////////////////
AudioMedia::AudioMedia() 
: Media(PJMEDIA_TYPE_AUDIO), id(PJSUA_INVALID_ID), mediaPool(NULL)
{

}

void AudioMedia::registerMediaPort(MediaPort port) throw(Error)
{
    /* Check if media already added to Conf bridge. */
    pj_assert(!Endpoint::instance().mediaExists(*this));

    if (port != NULL) {
	pj_assert(id == PJSUA_INVALID_ID);

	pj_caching_pool_init(&mediaCachingPool, NULL, 0);

	mediaPool = pj_pool_create(&mediaCachingPool.factory,
				   "media",
				   512,
				   512,
				   NULL);

	if (!mediaPool) {
	    pj_caching_pool_destroy(&mediaCachingPool);
	    PJSUA2_RAISE_ERROR(PJ_ENOMEM);
	}

	PJSUA2_CHECK_EXPR( pjsua_conf_add_port(mediaPool,
					       (pjmedia_port *)port,
					       &id) );
    }

    Endpoint::instance().mediaAdd(*this);
}

void AudioMedia::unregisterMediaPort()
{
    if (id != PJSUA_INVALID_ID) {
	pjsua_conf_remove_port(id);
	id = PJSUA_INVALID_ID;
    }

    if (mediaPool) {
	pj_pool_release(mediaPool);
	mediaPool = NULL;
	pj_caching_pool_destroy(&mediaCachingPool);
    }

    Endpoint::instance().mediaRemove(*this);
}

AudioMedia::~AudioMedia() 
{
    unregisterMediaPort();
}

ConfPortInfo AudioMedia::getPortInfo() const throw(Error)
{
    return AudioMedia::getPortInfoFromId(id);
}

int AudioMedia::getPortId() const
{
    return id;
}

ConfPortInfo AudioMedia::getPortInfoFromId(int port_id) throw(Error)
{
    pjsua_conf_port_info pj_info;
    ConfPortInfo pi;

    PJSUA2_CHECK_EXPR( pjsua_conf_get_port_info(port_id, &pj_info) );
    pi.fromPj(pj_info);
    return pi;
}

void AudioMedia::startTransmit(const AudioMedia &sink) const throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_connect(id, sink.id) );
}

void AudioMedia::stopTransmit(const AudioMedia &sink) const throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_disconnect(id, sink.id) );
}

void AudioMedia::adjustRxLevel(float level) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_adjust_tx_level(id, level) );
}

void AudioMedia::adjustTxLevel(float level) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_conf_adjust_rx_level(id, level) );
}

unsigned AudioMedia::getRxLevel() const throw(Error)
{
    unsigned level;
    PJSUA2_CHECK_EXPR( pjsua_conf_get_signal_level(id, &level, NULL) );
    return level * 100 / 255;
}

unsigned AudioMedia::getTxLevel() const throw(Error)
{
    unsigned level;
    PJSUA2_CHECK_EXPR( pjsua_conf_get_signal_level(id, NULL, &level) );
    return level * 100 / 255;
}

AudioMedia* AudioMedia::typecastFromMedia(Media *media)
{
    return static_cast<AudioMedia*>(media);
}

///////////////////////////////////////////////////////////////////////////////

AudioMediaPlayer::AudioMediaPlayer()
: playerId(PJSUA_INVALID_ID)
{

}

AudioMediaPlayer::~AudioMediaPlayer()
{
    if (playerId != PJSUA_INVALID_ID) {
	unregisterMediaPort();
	pjsua_player_destroy(playerId);
    }
}

void AudioMediaPlayer::createPlayer(const string &file_name,
				    unsigned options)
				    throw(Error)
{
    if (playerId != PJSUA_INVALID_ID) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    pj_str_t pj_name = str2Pj(file_name);

    PJSUA2_CHECK_EXPR( pjsua_player_create(&pj_name,
					   options, 
					   &playerId) );

    /* Register EOF callback */
    pjmedia_port *port;
    pj_status_t status;

    status = pjsua_player_get_port(playerId, &port);
    if (status != PJ_SUCCESS) {
	pjsua_player_destroy(playerId);
	PJSUA2_RAISE_ERROR2(status, "AudioMediaPlayer::createPlayer()");
    }
    status = pjmedia_wav_player_set_eof_cb(port, this, &eof_cb);
    if (status != PJ_SUCCESS) {
	pjsua_player_destroy(playerId);
	PJSUA2_RAISE_ERROR2(status, "AudioMediaPlayer::createPlayer()");
    }

    /* Get media port id. */
    id = pjsua_player_get_conf_port(playerId);

    registerMediaPort(NULL);
}

void AudioMediaPlayer::createPlaylist(const StringVector &file_names,
				      const string &label,
				      unsigned options)
				      throw(Error)
{
    if (playerId != PJSUA_INVALID_ID) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    pj_str_t pj_files[MAX_FILE_NAMES];
    unsigned i, count = 0;
    pj_str_t pj_lbl = str2Pj(label);
    pj_status_t status;

    count = PJ_ARRAY_SIZE(pj_files);

    for(i=0; i<file_names.size() && i<count;++i)
    {
	const string &file_name = file_names[i];
	
	pj_files[i] = str2Pj(file_name);
    }

    PJSUA2_CHECK_EXPR( pjsua_playlist_create(pj_files,
					     i,
					     &pj_lbl,
					     options, 
					     &playerId) );

    /* Register EOF callback */
    pjmedia_port *port;
    status = pjsua_player_get_port(playerId, &port);
    if (status != PJ_SUCCESS) {
	pjsua_player_destroy(playerId);
	PJSUA2_RAISE_ERROR2(status, "AudioMediaPlayer::createPlaylist()");
    }
    status = pjmedia_wav_playlist_set_eof_cb(port, this, &eof_cb);
    if (status != PJ_SUCCESS) {
	pjsua_player_destroy(playerId);
	PJSUA2_RAISE_ERROR2(status, "AudioMediaPlayer::createPlaylist()");
    }

    /* Get media port id. */
    id = pjsua_player_get_conf_port(playerId);

    registerMediaPort(NULL);
}

AudioMediaPlayerInfo AudioMediaPlayer::getInfo() const throw(Error)
{
    AudioMediaPlayerInfo info;
    pjmedia_wav_player_info pj_info;

    PJSUA2_CHECK_EXPR( pjsua_player_get_info(playerId, &pj_info) );

    pj_bzero(&info, sizeof(info));
    info.formatId 		= pj_info.fmt_id;
    info.payloadBitsPerSample	= pj_info.payload_bits_per_sample;
    info.sizeBytes		= pj_info.size_bytes;
    info.sizeSamples		= pj_info.size_samples;

    return info;
}

pj_uint32_t AudioMediaPlayer::getPos() const throw(Error)
{
    pj_ssize_t pos = pjsua_player_get_pos(playerId);
    if (pos < 0) {
	PJSUA2_RAISE_ERROR2((pj_status_t)-pos, "AudioMediaPlayer::getPos()");
    }
    return (pj_uint32_t)pos;
}

void AudioMediaPlayer::setPos(pj_uint32_t samples) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_player_set_pos(playerId, samples) );
}

AudioMediaPlayer* AudioMediaPlayer::typecastFromAudioMedia(
						AudioMedia *media)
{
    return static_cast<AudioMediaPlayer*>(media);
}

pj_status_t AudioMediaPlayer::eof_cb(pjmedia_port *port,
                                     void *usr_data)
{
    PJ_UNUSED_ARG(port);
    AudioMediaPlayer *player = (AudioMediaPlayer*)usr_data;
    return player->onEof() ? PJ_SUCCESS : PJ_EEOF;
}

///////////////////////////////////////////////////////////////////////////////
AudioMediaRecorder::AudioMediaRecorder()
: recorderId(PJSUA_INVALID_ID)
{

}

AudioMediaRecorder::~AudioMediaRecorder()
{
    if (recorderId != PJSUA_INVALID_ID) {
	unregisterMediaPort();
	pjsua_recorder_destroy(recorderId);
    }
}

void AudioMediaRecorder::createRecorder(const string &file_name,
				        unsigned enc_type,
				        pj_ssize_t max_size,
				        unsigned options)
				        throw(Error)
{
    PJ_UNUSED_ARG(max_size);

    if (recorderId != PJSUA_INVALID_ID) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    pj_str_t pj_name = str2Pj(file_name);

    PJSUA2_CHECK_EXPR( pjsua_recorder_create(&pj_name,
					     enc_type,
					     NULL,
					     -1,
					     options,
					     &recorderId) );

    /* Get media port id. */
    id = pjsua_recorder_get_conf_port(recorderId);

    registerMediaPort(NULL);
}

AudioMediaRecorder* AudioMediaRecorder::typecastFromAudioMedia(
						AudioMedia *media)
{
    return static_cast<AudioMediaRecorder*>(media);
}

///////////////////////////////////////////////////////////////////////////////

ToneGenerator::ToneGenerator()
: pool(NULL), tonegen(NULL)
{
}

ToneGenerator::~ToneGenerator()
{
    if (tonegen) {
	unregisterMediaPort();
	pjmedia_port_destroy(tonegen);
	tonegen = NULL;
    }
    if (pool) {
	pj_pool_release(pool);
	pool = NULL;
    }
}

void ToneGenerator::createToneGenerator(unsigned clock_rate,
					unsigned channel_count) throw(Error)
{
    pj_status_t status;

    if (pool) {
	PJSUA2_RAISE_ERROR(PJ_EEXISTS);
    }

    pool = pjsua_pool_create( "tonegen%p", 512, 512);
    if (!pool) {
	PJSUA2_RAISE_ERROR(PJ_ENOMEM);
    }

    status = pjmedia_tonegen_create( pool, clock_rate, channel_count,
				     clock_rate * 20 / 1000, 16,
				     0, &tonegen);
    if (status != PJ_SUCCESS) {
	PJSUA2_RAISE_ERROR(status);
    }

    registerMediaPort(tonegen);
}

bool ToneGenerator::isBusy() const
{
    return tonegen && pjmedia_tonegen_is_busy(tonegen) != 0;
}

void ToneGenerator::stop() throw(Error)
{
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }

    status = pjmedia_tonegen_stop(tonegen);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::stop()");
}

void ToneGenerator::rewind() throw(Error)
{
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }

    status = pjmedia_tonegen_rewind(tonegen);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::rewind()");
}

void ToneGenerator::play(const ToneDescVector &tones,
                         bool loop) throw(Error)
{
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }
    if (tones.size() == 0) {
	PJSUA2_RAISE_ERROR(PJ_EINVAL);
    }

    status = pjmedia_tonegen_play(tonegen, (unsigned)tones.size(), &tones[0],
				  loop? PJMEDIA_TONEGEN_LOOP : 0);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::play()");
}

void ToneGenerator::playDigits(const ToneDigitVector &digits,
                               bool loop) throw(Error)
{
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }
    if (digits.size() == 0) {
	PJSUA2_RAISE_ERROR(PJ_EINVAL);
    }

    status = pjmedia_tonegen_play_digits(tonegen, (unsigned)digits.size(), &digits[0],
					 loop? PJMEDIA_TONEGEN_LOOP : 0);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::playDigits()");
}

ToneDigitMapVector ToneGenerator::getDigitMap() const throw(Error)
{
    const pjmedia_tone_digit_map *pdm;
    ToneDigitMapVector tdm;
    unsigned i;
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }

    status = pjmedia_tonegen_get_digit_map(tonegen, &pdm);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::getDigitMap()");

    for (i=0; i<pdm->count; ++i) {
	ToneDigitMapDigit d;
	char str_digit[2];

	str_digit[0] = pdm->digits[i].digit;
	str_digit[1] = '\0';

	d.digit = str_digit;
	d.freq1 = pdm->digits[i].freq1;
	d.freq2 = pdm->digits[i].freq2;

	tdm.push_back(d);
    }

    return tdm;
}

void ToneGenerator::setDigitMap(const ToneDigitMapVector &digit_map)
				throw(Error)
{
    unsigned i;
    pj_status_t status;

    if (!tonegen) {
	PJSUA2_RAISE_ERROR(PJ_EINVALIDOP);
    }

    digitMap.count = (unsigned)digit_map.size();
    if (digitMap.count > PJ_ARRAY_SIZE(digitMap.digits))
	digitMap.count = PJ_ARRAY_SIZE(digitMap.digits);

    for (i=0; i<digitMap.count; ++i) {
	digitMap.digits[i].digit = digit_map[i].digit.c_str()[0];
	digitMap.digits[i].freq1 = (short)digit_map[i].freq1;
	digitMap.digits[i].freq2 = (short)digit_map[i].freq2;
    }

    status = pjmedia_tonegen_set_digit_map(tonegen, &digitMap);
    PJSUA2_CHECK_RAISE_ERROR2(status, "ToneGenerator::setDigitMap()");
}


///////////////////////////////////////////////////////////////////////////////
void AudioDevInfo::fromPj(const pjmedia_aud_dev_info &dev_info)
{
    name = dev_info.name;
    inputCount = dev_info.input_count;
    outputCount = dev_info.output_count;
    defaultSamplesPerSec = dev_info.default_samples_per_sec;
    driver = dev_info.driver;
    caps = dev_info.caps;
    routes = dev_info.routes;

    for (unsigned i=0; i<dev_info.ext_fmt_cnt;++i) {
	MediaFormatAudio format;
	format.fromPj(dev_info.ext_fmt[i]);
	if (format.type == PJMEDIA_TYPE_AUDIO)
	    extFmt.push_back(format);
    }
}

AudioDevInfo::~AudioDevInfo()
{
}

///////////////////////////////////////////////////////////////////////////////
/*
 * Simple AudioMedia class for sound device.
 */
class DevAudioMedia : public AudioMedia
{
public:
    DevAudioMedia();
    ~DevAudioMedia();
};

DevAudioMedia::DevAudioMedia()
{
    this->id = 0;
    registerMediaPort(NULL);
}

DevAudioMedia::~DevAudioMedia()
{
    /* Avoid removing this port (conf port id=0) from conference */
    this->id = PJSUA_INVALID_ID;
}

///////////////////////////////////////////////////////////////////////////////
/* Audio device operations. */

AudDevManager::AudDevManager()
: devMedia(NULL)
{
}

AudDevManager::~AudDevManager()
{
    // At this point, devMedia should have been cleaned up by Endpoint,
    // as AudDevManager destructor is called after Endpoint destructor.
    //delete devMedia;
    
    clearAudioDevList();
}

int AudDevManager::getCaptureDev() const throw(Error)
{
    return getActiveDev(true);
}

AudioMedia &AudDevManager::getCaptureDevMedia() throw(Error)
{
    if (!devMedia)
	devMedia = new DevAudioMedia;
    return *devMedia;
}

int AudDevManager::getPlaybackDev() const throw(Error)
{
    return getActiveDev(false);
}

AudioMedia &AudDevManager::getPlaybackDevMedia() throw(Error)
{
    if (!devMedia)
    	devMedia = new DevAudioMedia;
    return *devMedia;
}

void AudDevManager::setCaptureDev(int capture_dev) const throw(Error)
{    
    pjsua_snd_dev_param param;
    pjsua_snd_dev_param_default(&param);    

    param.capture_dev = capture_dev;
    param.playback_dev = getPlaybackDev();    

    param.mode = PJSUA_SND_DEV_NO_IMMEDIATE_OPEN;    

    PJSUA2_CHECK_EXPR( pjsua_set_snd_dev2(&param) );
}

void AudDevManager::setPlaybackDev(int playback_dev) const throw(Error)
{
    pjsua_snd_dev_param param;
    pjsua_snd_dev_param_default(&param);    

    param.capture_dev = getCaptureDev();
    param.playback_dev = playback_dev;

    param.mode = PJSUA_SND_DEV_NO_IMMEDIATE_OPEN;    

    PJSUA2_CHECK_EXPR( pjsua_set_snd_dev2(&param) );    
}

const AudioDevInfoVector &AudDevManager::enumDev() throw(Error)
{
    pjmedia_aud_dev_info pj_info[MAX_DEV_COUNT];
    unsigned count = MAX_DEV_COUNT;

    PJSUA2_CHECK_EXPR( pjsua_enum_aud_devs(pj_info, &count) );

    pj_enter_critical_section();
    clearAudioDevList();
    for (unsigned i = 0; i<count ;++i) {
	AudioDevInfo *dev_info = new AudioDevInfo;
	dev_info->fromPj(pj_info[i]);
	audioDevList.push_back(dev_info);
    }
    pj_leave_critical_section();
    return audioDevList;
}

void AudDevManager::setNullDev() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_set_null_snd_dev() );
}

MediaPort *AudDevManager::setNoDev()
{
    return (MediaPort*)pjsua_set_no_snd_dev();
}

void AudDevManager::setSndDevMode(unsigned mode) const throw(Error)
{    
    int capture_dev = 0, playback_dev = 0;
    pjsua_snd_dev_param param;
    pj_status_t status = pjsua_get_snd_dev(&capture_dev, &playback_dev);    
    if (status != PJ_SUCCESS) {
	PJSUA2_RAISE_ERROR2(status, "AudDevManager::setSndDevMode()");	
    }
    pjsua_snd_dev_param_default(&param);
    param.capture_dev = capture_dev;
    param.playback_dev = playback_dev;
    param.mode = mode;
    PJSUA2_CHECK_EXPR( pjsua_set_snd_dev2(&param) );
}

void AudDevManager::setEcOptions(unsigned tail_msec,
				 unsigned options) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_set_ec(tail_msec, options) );
}

unsigned AudDevManager::getEcTail() const throw(Error)
{
    unsigned tail_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_get_ec_tail(&tail_msec) );

    return tail_msec;
}

bool AudDevManager::sndIsActive() const
{
    return PJ2BOOL(pjsua_snd_is_active());
}

void AudDevManager::refreshDevs() throw(Error)
{
    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_refresh() );
}

unsigned AudDevManager::getDevCount() const
{
    return pjmedia_aud_dev_count();
}

AudioDevInfo
AudDevManager::getDevInfo(int id) const throw(Error)
{
    AudioDevInfo dev_info;
    pjmedia_aud_dev_info pj_info;

    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_get_info(id, &pj_info) );

    dev_info.fromPj(pj_info);
    return dev_info;
}

int AudDevManager::lookupDev(const string &drv_name,
			     const string &dev_name) const throw(Error)
{
    pjmedia_aud_dev_index pj_idx = 0;

    PJSUA2_CHECK_EXPR( pjmedia_aud_dev_lookup(drv_name.c_str(),
					      dev_name.c_str(),
					      &pj_idx) );

    return pj_idx;
}


string AudDevManager::capName(pjmedia_aud_dev_cap cap) const
{
    return pjmedia_aud_dev_cap_name(cap, NULL);
}

void
AudDevManager::setExtFormat(const MediaFormatAudio &format,
			    bool keep) throw(Error)
{
    pjmedia_format pj_format = format.toPj();

    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT,
					     &pj_format,
					     keep) );
}

MediaFormatAudio AudDevManager::getExtFormat() const throw(Error)
{
    pjmedia_format pj_format;
    MediaFormatAudio format;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_EXT_FORMAT,
					     &pj_format) );

    format.fromPj(pj_format);

    return format;
}

void AudDevManager::setInputLatency(unsigned latency_msec,
				    bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
					     &latency_msec,
					     keep) );
}

unsigned AudDevManager::getInputLatency() const throw(Error)
{
    unsigned latency_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
					     &latency_msec) );

    return latency_msec;
}

void
AudDevManager::setOutputLatency(unsigned latency_msec,
				bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
					     &latency_msec,
					     keep) );
}

unsigned AudDevManager::getOutputLatency() const throw(Error)
{
    unsigned latency_msec = 0;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
					     &latency_msec) );

    return latency_msec;
}

void AudDevManager::setInputVolume(unsigned volume, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR(
	    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
				  &volume,
				  keep) );
}

unsigned AudDevManager::getInputVolume() const throw(Error)
{
    unsigned volume = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING,
				  &volume) );

    return volume;
}

void AudDevManager::setOutputVolume(unsigned volume, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR(
	    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
				  &volume,
				  keep) );
}

unsigned AudDevManager::getOutputVolume() const throw(Error)
{
    unsigned volume = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
				  &volume) );

    return volume;
}

unsigned AudDevManager::getInputSignal() const throw(Error)
{
    unsigned signal = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER,
				  &signal) );

    return signal;
}

unsigned AudDevManager::getOutputSignal() const throw(Error)
{
    unsigned signal = 0;

    PJSUA2_CHECK_EXPR(
	    pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER,
				  &signal) );

    return signal;
}

void
AudDevManager::setInputRoute(pjmedia_aud_dev_route route,
			     bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
					     &route,
					     keep) );
}

pjmedia_aud_dev_route AudDevManager::getInputRoute() const throw(Error)
{
    pjmedia_aud_dev_route route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
					     &route) );

    return route;
}

void
AudDevManager::setOutputRoute(pjmedia_aud_dev_route route,
			      bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					     &route,
					     keep) );
}

pjmedia_aud_dev_route AudDevManager::getOutputRoute() const throw(Error)
{
    pjmedia_aud_dev_route route = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
					     &route) );

    return route;
}

void AudDevManager::setVad(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_VAD,
					     &enable,
					     keep) );
}

bool AudDevManager::getVad() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_VAD,
					     &enable) );

    return enable;
}

void AudDevManager::setCng(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_CNG,
					     &enable,
					     keep) );
}

bool AudDevManager::getCng() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_CNG,
					     &enable) );

    return enable;
}

void AudDevManager::setPlc(bool enable, bool keep) throw(Error)
{
    PJSUA2_CHECK_EXPR( pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_PLC,
					     &enable,
					     keep) );
}

bool AudDevManager::getPlc() const throw(Error)
{
    bool enable = false;

    PJSUA2_CHECK_EXPR( pjsua_snd_get_setting(PJMEDIA_AUD_DEV_CAP_PLC,
					     &enable) );

    return enable;
}

void AudDevManager::clearAudioDevList()
{
    for(unsigned i=0;i<audioDevList.size();++i) {
	delete audioDevList[i];
    }
    audioDevList.clear();
}

int AudDevManager::getActiveDev(bool is_capture) const throw(Error)
{
    int capture_dev = 0, playback_dev = 0;
    PJSUA2_CHECK_EXPR( pjsua_get_snd_dev(&capture_dev, &playback_dev) );

    return is_capture?capture_dev:playback_dev;
}

///////////////////////////////////////////////////////////////////////////////
VideoWindow::VideoWindow(pjsua_vid_win_id win_id)
: winId(win_id)
{
#if !PJSUA_HAS_VIDEO
    /* Suppress warning of unused field when video is disabled */
    PJ_UNUSED_ARG(winId);
#endif
}

VideoWindowInfo VideoWindow::getInfo() const throw(Error)
{
    VideoWindowInfo vwi;
    pj_bzero(&vwi, sizeof(vwi));
#if PJSUA_HAS_VIDEO
    pjsua_vid_win_info pj_vwi;
    
    PJSUA2_CHECK_EXPR( pjsua_vid_win_get_info(winId, &pj_vwi) );
    vwi.isNative = (pj_vwi.is_native != PJ_FALSE);
    vwi.winHandle.type = pj_vwi.hwnd.type;
    vwi.winHandle.handle.window = pj_vwi.hwnd.info.window;
    vwi.renderDeviceId = pj_vwi.rdr_dev;
    vwi.show = (pj_vwi.show != PJ_FALSE);
    vwi.pos.x = pj_vwi.pos.x;
    vwi.pos.y = pj_vwi.pos.y;
    vwi.size.w = pj_vwi.size.w;
    vwi.size.h = pj_vwi.size.h;
    
#endif
    return vwi;
}
    
void VideoWindow::Show(bool show) throw(Error)
{
#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR( pjsua_vid_win_set_show(winId, show) );
#else
    PJ_UNUSED_ARG(show);
#endif
}

void VideoWindow::setPos(const MediaCoordinate &pos) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_coord pj_pos;
    
    pj_pos.x = pos.x;
    pj_pos.y = pos.y;
    PJSUA2_CHECK_EXPR( pjsua_vid_win_set_pos(winId, &pj_pos) );
#else
    PJ_UNUSED_ARG(pos);
#endif
}

void VideoWindow::setSize(const MediaSize &size) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_rect_size pj_size;

    pj_size.w = size.w;
    pj_size.h = size.h;
    PJSUA2_CHECK_EXPR( pjsua_vid_win_set_size(winId, &pj_size) );
#else
    PJ_UNUSED_ARG(size);
#endif
}

void VideoWindow::rotate(int angle) throw(Error)
{
#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR( pjsua_vid_win_rotate(winId, angle) );
#else
    PJ_UNUSED_ARG(angle);
#endif
}

void VideoWindow::setWindow(const VideoWindowHandle &win) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_vid_dev_hwnd vhwnd;
   
    vhwnd.type = win.type;
    vhwnd.info.window = win.handle.window;
    PJSUA2_CHECK_EXPR( pjsua_vid_win_set_win(winId, &vhwnd) );
#else
    PJ_UNUSED_ARG(win);
#endif
}
///////////////////////////////////////////////////////////////////////////////
VideoPreviewOpParam::VideoPreviewOpParam()
{
#if PJSUA_HAS_VIDEO
    pjsua_vid_preview_param vid_prev_param;

    pjsua_vid_preview_param_default(&vid_prev_param);
    fromPj(vid_prev_param);
#endif
}

void VideoPreviewOpParam::fromPj(const pjsua_vid_preview_param &prm)
{
#if PJSUA_HAS_VIDEO
    this->rendId		    = prm.rend_id;
    this->show			    = PJ2BOOL(prm.show);
    this->windowFlags		    = prm.wnd_flags;
    this->format.id		    = prm.format.id;
    this->format.type		    = prm.format.type;
    this->window.type		    = prm.wnd.type;
    this->window.handle.window	    = prm.wnd.info.window;
#else
    PJ_UNUSED_ARG(prm);
#endif
}

pjsua_vid_preview_param VideoPreviewOpParam::toPj() const
{
    pjsua_vid_preview_param param;
    pj_bzero(&param, sizeof(param));
#if PJSUA_HAS_VIDEO
    param.rend_id	    = this->rendId;
    param.show		    = this->show;
    param.wnd_flags	    = this->windowFlags;
    param.format.id	    = this->format.id;
    param.format.type	    = this->format.type;
    param.wnd.type	    = this->window.type;
    param.wnd.info.window   = this->window.handle.window;
#endif
    return param;
}

VideoPreview::VideoPreview(int dev_id) 
: devId(dev_id)
{

}

bool VideoPreview::hasNative()
{
#if PJSUA_HAS_VIDEO
    return(PJ2BOOL(pjsua_vid_preview_has_native(devId)));
#else
    return false;
#endif
}

void VideoPreview::start(const VideoPreviewOpParam &param) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjsua_vid_preview_param prm = param.toPj();
    PJSUA2_CHECK_EXPR(pjsua_vid_preview_start(devId, &prm));
#else
    PJ_UNUSED_ARG(param);
    PJ_UNUSED_ARG(devId);
#endif
}

void VideoPreview::stop() throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjsua_vid_preview_stop(devId);
#endif
}

VideoWindow VideoPreview::getVideoWindow()
{
#if PJSUA_HAS_VIDEO
    return (VideoWindow(pjsua_vid_preview_get_win(devId)));
#else
    return (VideoWindow(PJSUA_INVALID_ID));
#endif
}

///////////////////////////////////////////////////////////////////////////////
void MediaFormatVideo::fromPj(const pjmedia_format &format)
{
#if PJSUA_HAS_VIDEO
    if ((format.type != PJMEDIA_TYPE_VIDEO) &&
	(format.detail_type != PJMEDIA_FORMAT_DETAIL_VIDEO))
    {
	type = PJMEDIA_TYPE_UNKNOWN;
	return;
    }

    id = format.id;
    type = format.type;

    /* Detail. */
    width = format.det.vid.size.w;
    height = format.det.vid.size.h;
    fpsNum = format.det.vid.fps.num;
    fpsDenum = format.det.vid.fps.denum;
    avgBps = format.det.vid.avg_bps;
    maxBps = format.det.vid.max_bps;
#else
    PJ_UNUSED_ARG(format);
    type = PJMEDIA_TYPE_UNKNOWN;
#endif
}

pjmedia_format MediaFormatVideo::toPj() const
{
    pjmedia_format pj_format;

#if PJSUA_HAS_VIDEO
    pj_format.id = id;
    pj_format.type = type;

    pj_format.detail_type = PJMEDIA_FORMAT_DETAIL_VIDEO;
    pj_format.det.vid.size.w = width;
    pj_format.det.vid.size.h = height;
    pj_format.det.vid.fps.num = fpsNum;
    pj_format.det.vid.fps.denum = fpsDenum;
    pj_format.det.vid.avg_bps = avgBps;
    pj_format.det.vid.max_bps = maxBps;
#else
    pj_format.type = PJMEDIA_TYPE_UNKNOWN;
#endif
    return pj_format;
}

///////////////////////////////////////////////////////////////////////////////
void VideoDevInfo::fromPj(const pjmedia_vid_dev_info &dev_info)
{
#if PJSUA_HAS_VIDEO
    id = dev_info.id;
    name = dev_info.name;
    driver = dev_info.driver;
    dir = dev_info.dir;
    caps = dev_info.caps;

    for (unsigned i = 0; i<dev_info.fmt_cnt;++i) {
	MediaFormatVideo format;
	format.fromPj(dev_info.fmt[i]);
	if (format.type == PJMEDIA_TYPE_VIDEO)
	    fmt.push_back(format);
    }
#else
    PJ_UNUSED_ARG(dev_info);
#endif
}

VideoDevInfo::~VideoDevInfo()
{
}

///////////////////////////////////////////////////////////////////////////////
void VidDevManager::refreshDevs() throw(Error)
{
#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR(pjmedia_vid_dev_refresh());
#endif
}

unsigned VidDevManager::getDevCount()
{
#if PJSUA_HAS_VIDEO
    return pjsua_vid_dev_count();
#else
    return 0;
#endif
}

VideoDevInfo VidDevManager::getDevInfo(int dev_id) const throw(Error)
{
    VideoDevInfo dev_info;
#if PJSUA_HAS_VIDEO
    pjmedia_vid_dev_info pj_info;

    PJSUA2_CHECK_EXPR(pjsua_vid_dev_get_info(dev_id, &pj_info));

    dev_info.fromPj(pj_info);
#else
    PJ_UNUSED_ARG(dev_id);
#endif
    return dev_info;
}

const VideoDevInfoVector &VidDevManager::enumDev() throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_vid_dev_info pj_info[MAX_DEV_COUNT];
    unsigned count = MAX_DEV_COUNT;

    PJSUA2_CHECK_EXPR(pjsua_vid_enum_devs(pj_info, &count));

    pj_enter_critical_section();
    clearVideoDevList();
    for (unsigned i = 0; i<count;++i) {
	VideoDevInfo *dev_info = new VideoDevInfo;
	dev_info->fromPj(pj_info[i]);
	videoDevList.push_back(dev_info);
    }
    pj_leave_critical_section();
#endif
    return videoDevList;
}

int VidDevManager::lookupDev(const string &drv_name,
			     const string &dev_name) const throw(Error)
{
    pjmedia_vid_dev_index pj_idx = 0;
#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR(pjmedia_vid_dev_lookup(drv_name.c_str(), 
					     dev_name.c_str(), 
					     &pj_idx));
#else
    PJ_UNUSED_ARG(drv_name);
    PJ_UNUSED_ARG(dev_name);
#endif
    return pj_idx;
}

string VidDevManager::capName(pjmedia_vid_dev_cap cap) const
{    
    string cap_name;
#if PJSUA_HAS_VIDEO
    cap_name = pjmedia_vid_dev_cap_name(cap, NULL);
#else
    PJ_UNUSED_ARG(cap);
#endif
    return cap_name;
}

void VidDevManager::setFormat(int dev_id,
			      const MediaFormatVideo &format,
			      bool keep) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_format pj_format = format.toPj();

    PJSUA2_CHECK_EXPR(pjsua_vid_dev_set_setting(dev_id,
						PJMEDIA_VID_DEV_CAP_FORMAT,
						&pj_format,
						keep));
#else
    PJ_UNUSED_ARG(dev_id);
    PJ_UNUSED_ARG(format);
    PJ_UNUSED_ARG(keep);
#endif
}

MediaFormatVideo VidDevManager::getFormat(int dev_id) const throw(Error)
{
    MediaFormatVideo vid_format;
    pj_bzero(&vid_format, sizeof(vid_format));
#if PJSUA_HAS_VIDEO
    pjmedia_format pj_format;
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_get_setting(dev_id,
						PJMEDIA_VID_DEV_CAP_FORMAT, 
						&pj_format));
    vid_format.fromPj(pj_format);
#else
    PJ_UNUSED_ARG(dev_id);
#endif
    return vid_format;
}

void VidDevManager::setInputScale(int dev_id,
				  const MediaSize &scale,
				  bool keep) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_rect_size pj_size;
    pj_size.w = scale.w;
    pj_size.h = scale.h;
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_set_setting(dev_id,
		      PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
		      &pj_size,
		      keep));
#else
    PJ_UNUSED_ARG(dev_id);
    PJ_UNUSED_ARG(scale);
    PJ_UNUSED_ARG(keep);
#endif
}

MediaSize VidDevManager::getInputScale(int dev_id) const throw(Error)
{
    MediaSize scale;
    pj_bzero(&scale, sizeof(scale));
#if PJSUA_HAS_VIDEO
    pjmedia_rect_size pj_size;
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_get_setting(dev_id,
					       PJMEDIA_VID_DEV_CAP_INPUT_SCALE,
					       &pj_size));

    scale.w = pj_size.w;
    scale.h = pj_size.h;
#else
    PJ_UNUSED_ARG(dev_id);
#endif
    return scale;
}

void VidDevManager::setOutputWindowFlags(int dev_id, 
					 int flags, 
					 bool keep) throw(Error)
{    
#if PJSUA_HAS_VIDEO    
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_set_setting(dev_id,
				       PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS,
				       &flags,
				       keep));
#else
    PJ_UNUSED_ARG(dev_id);
    PJ_UNUSED_ARG(flags);
    PJ_UNUSED_ARG(keep);
#endif
}

int VidDevManager::getOutputWindowFlags(int dev_id) throw(Error)
{
    int flags = 0;

#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_get_setting(dev_id,
				       PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW_FLAGS,
				       &flags));
#else
    PJ_UNUSED_ARG(dev_id);
#endif
    return flags;
}

void VidDevManager::switchDev(int dev_id,
			      const VideoSwitchParam &param) throw(Error)
{
#if PJSUA_HAS_VIDEO
    pjmedia_vid_dev_switch_param pj_param;
    pj_param.target_id = param.target_id;
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_set_setting(dev_id,
						PJMEDIA_VID_DEV_CAP_SWITCH,
						&pj_param,
						PJ_FALSE));
#else
    PJ_UNUSED_ARG(dev_id);
    PJ_UNUSED_ARG(param);    
#endif
}

void VidDevManager::clearVideoDevList()
{
#if PJSUA_HAS_VIDEO
    for (unsigned i = 0;i<videoDevList.size();++i) {
	delete videoDevList[i];
    }
    videoDevList.clear();
#endif
}

bool VidDevManager::isCaptureActive(int dev_id) const
{
#if PJSUA_HAS_VIDEO
    return (pjsua_vid_dev_is_active(dev_id) == PJ_TRUE? true: false);
#else
    PJ_UNUSED_ARG(dev_id);
    
    return false;
#endif
}
    
void VidDevManager::setCaptureOrient(pjmedia_vid_dev_index dev_id,
    			  	     pjmedia_orient orient,
    			  	     bool keep) throw(Error)
{
#if PJSUA_HAS_VIDEO
    PJSUA2_CHECK_EXPR(pjsua_vid_dev_set_setting(dev_id,
    			  PJMEDIA_VID_DEV_CAP_ORIENTATION, &orient, keep));
#else
    PJ_UNUSED_ARG(dev_id);
    PJ_UNUSED_ARG(orient);
    PJ_UNUSED_ARG(keep);
#endif
}

VidDevManager::VidDevManager()
{
}

VidDevManager::~VidDevManager()
{
#if PJSUA_HAS_VIDEO
    clearVideoDevList();
#endif
}

///////////////////////////////////////////////////////////////////////////////

/** 
 * Utility class for converting CodecFmtpVector to and from pjmedia_codec_fmtp. 
 */
class CodecFmtpUtil
{
public:
    static void fromPj(const pjmedia_codec_fmtp &in_fmtp,
		       CodecFmtpVector &out_fmtp)
    {
        unsigned i = 0;
        for (; i<in_fmtp.cnt; ++i) {
	    CodecFmtp fmtp;
	    fmtp.name = pj2Str(in_fmtp.param[i].name);
	    fmtp.val = pj2Str(in_fmtp.param[i].val);
	
            out_fmtp.push_back(fmtp);
       }
    }

    static void toPj(const CodecFmtpVector &in_fmtp,
		     pjmedia_codec_fmtp &out_fmtp)
    {
        CodecFmtpVector::const_iterator i;
        out_fmtp.cnt = 0;
        for (i = in_fmtp.begin(); i != in_fmtp.end(); ++i) {
	    if (out_fmtp.cnt >= PJMEDIA_CODEC_MAX_FMTP_CNT) {
	        break;
    	    }
	    out_fmtp.param[out_fmtp.cnt].name = str2Pj((*i).name);
	    out_fmtp.param[out_fmtp.cnt].val = str2Pj((*i).val);
	    ++out_fmtp.cnt;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////
void CodecInfo::fromPj(const pjsua_codec_info &codec_info)
{
    codecId = pj2Str(codec_info.codec_id);
    priority = codec_info.priority;
    desc = pj2Str(codec_info.desc);
}

///////////////////////////////////////////////////////////////////////////////
void CodecParam::fromPj(const pjmedia_codec_param &param)
{
    /* info part. */
    info.clockRate = param.info.clock_rate;
    info.channelCnt = param.info.channel_cnt;
    info.avgBps = param.info.avg_bps;
    info.maxBps = param.info.max_bps;
    info.maxRxFrameSize = param.info.max_rx_frame_size;
    info.frameLen = param.info.frm_ptime;
    info.pcmBitsPerSample = param.info.pcm_bits_per_sample;
    info.pt = param.info.pt;
    info.fmtId = param.info.fmt_id;

    /* setting part. */
    setting.frmPerPkt = param.setting.frm_per_pkt;
    setting.vad = param.setting.vad;
    setting.cng = param.setting.cng;
    setting.penh = param.setting.penh;
    setting.plc = param.setting.plc;
    setting.reserved = param.setting.reserved;
    CodecFmtpUtil::fromPj(param.setting.enc_fmtp, setting.encFmtp);
    CodecFmtpUtil::fromPj(param.setting.dec_fmtp, setting.decFmtp);
}

pjmedia_codec_param CodecParam::toPj() const
{
    pjmedia_codec_param param;

    /* info part. */
    param.info.clock_rate = info.clockRate;
    param.info.channel_cnt = info.channelCnt;
    param.info.avg_bps = (pj_uint32_t)info.avgBps;
    param.info.max_bps= (pj_uint32_t)info.maxBps;
    param.info.max_rx_frame_size = info.maxRxFrameSize;
    param.info.frm_ptime = (pj_uint16_t)info.frameLen;
    param.info.pcm_bits_per_sample = (pj_uint8_t)info.pcmBitsPerSample;
    param.info.pt = (pj_uint8_t)info.pt;
    param.info.fmt_id = info.fmtId;

    /* setting part. */
    param.setting.frm_per_pkt = (pj_uint8_t)setting.frmPerPkt;
    param.setting.vad = setting.vad;
    param.setting.cng = setting.cng;
    param.setting.penh = setting.penh;
    param.setting.plc = setting.plc;
    param.setting.reserved = setting.reserved;
    CodecFmtpUtil::toPj(setting.encFmtp, param.setting.enc_fmtp);
    CodecFmtpUtil::toPj(setting.decFmtp, param.setting.dec_fmtp);
    return param;
}

///////////////////////////////////////////////////////////////////////////////
void VidCodecParam::fromPj(const pjmedia_vid_codec_param &param)
{
    dir = param.dir;
    packing = param.packing;
    ignoreFmtp = param.ignore_fmtp != PJ_FALSE;
    encMtu = param.enc_mtu;
    encFmt.fromPj(param.enc_fmt);
    decFmt.fromPj(param.dec_fmt);
    CodecFmtpUtil::fromPj(param.enc_fmtp, encFmtp);
    CodecFmtpUtil::fromPj(param.dec_fmtp, decFmtp);
}

pjmedia_vid_codec_param VidCodecParam::toPj() const
{
    pjmedia_vid_codec_param param;
    pj_bzero(&param, sizeof(param));    
    param.dir = dir;
    param.packing = packing;
    param.ignore_fmtp = ignoreFmtp;
    param.enc_mtu = encMtu;
    param.enc_fmt = encFmt.toPj();
    param.dec_fmt = decFmt.toPj();
    CodecFmtpUtil::toPj(encFmtp, param.enc_fmtp);
    CodecFmtpUtil::toPj(decFmtp, param.dec_fmtp);
    return param;
}

