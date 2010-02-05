/*
StreamIO
A set of generic stream processing functions

    StreamIO is free software, and you are welcome to redistribute it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version. 
    This software comes with ABSOLUTELY NO WARRANTY; see gpl.html for
    details. If you haven't received a copy of the GNU General Public
    License along with this program, see http://www.gnu.org/licenses/

Refer to streamio.html for help
Copyright 2010 Dmitry Grigoryev
*/

#include <stdio.h>
#include "streamio.h"

// Generic streaming
int  StreamRead(stream_p in, void *buffer, size_t size) {
	stream_p    sin = (stream_p)in;
	if(sin->Read) return sin->Read(sin->io, buffer, size);
	else return -1;
}

int  StreamWrite(stream_p out, const void *buffer, size_t len) {
	stream_p    sout = (stream_p)out;
	if(sout->Write) return sout->Write(sout->io, buffer, len);
	else return -1;
}

void StreamClose(stream_p io) {
	stream_p    sio = (stream_p)io;
	if(sio->Close) sio->Close(sio->io);
	sio->Close = NULL;
}

int  StreamPipeline(void *in, void *out, void *buffer, size_t size) {
	stream_p    sin = (stream_p)in;
	stream_p    sout = (stream_p)out;
	int retcode = 0;
	int rlen, wlen;
	if(!sin->Read) return -1;
	if(!sout->Write) return -1;
	while((rlen = sin->Read(sin->io, buffer, size)) != STREAM_EOF) {
		wlen = sout->Write(sout->io, buffer, rlen);
		if(wlen == STREAM_EOF) break;
		retcode += wlen;
	}
	return retcode;
}

// File streaming
int  StreamReadFile(void *in, void *buffer, size_t size) {
	int retcode = fread(buffer, 1, size, (FILE *)in);
	if(retcode == 0) {
		if(feof((FILE *)in)) retcode = -1;
		if(ferror((FILE *)in)) retcode = -1;
	}
	return retcode;
}

int  StreamWriteFile(void *out, const void *buffer, size_t len) {
	int retcode = fwrite(buffer, 1, len, (FILE *)out);
	if(retcode == 0) {
		if(ferror((FILE *)out)) retcode = -1;
	}
	return retcode;
}

void StreamCloseFile(void *io) {
	fclose((FILE *)io);
}



/*
typedef struct {
	char   *buffer;
	size_t  start;
	size_t	len;
	size_t  size;
} steambuf_t, *steambuf_p;
*/

// Buffer streaming
/*
int  StreamReadBuffer(void *in, void *buffer, size_t len) {
	return 0;
}

int  StreamWriteBuffer(void *out, const void *buffer, size_t size) {
	return 0;
}

int  StreamReadFIFO(void *in, void *buffer, size_t len) {
	return 0;
}

int  StreamWriteFIFO(void *out, const void *buffer, size_t size) {
	return 0;
}

int  StreamReadLIFO(void *in, void *buffer, size_t len) {
	return 0;
}

int  StreamWriteLIFO(void *out, const void *buffer, size_t size) {
	return 0;
}
*/