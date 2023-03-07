// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_IO_HXX
#define MPD_FFMPEG_IO_HXX

extern "C" {
#include "libavformat/avio.h"
}

#include <cstdint>

class DecoderClient;
class InputStream;

struct AvioStream {
	DecoderClient *const client;
	InputStream &input;

	AVIOContext *io;

	AvioStream(DecoderClient *_client, InputStream &_input)
		:client(_client), input(_input), io(nullptr) {}

	~AvioStream();

	bool Open();

private:
	int Read(void *buffer, int size);
	int64_t Seek(int64_t pos, int whence);

	static int _Read(void *opaque, uint8_t *buf, int size);
	static int64_t _Seek(void *opaque, int64_t pos, int whence);
};

#endif
