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


#ifndef __STREAMIO_H
#define __STREAMIO_H
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#ifndef NULL
#define NULL 0
#endif

#define STREAM_EOF (-1)

// Generic streaming
typedef struct {
	void   *io;
	int	  (*Read)(void *in, void *buffer, size_t size);
	int	  (*Write)(void *out, const void *buffer, size_t len);
	void  (*Close)(void *io);
} stream_t, *stream_p;

#define STREAM_IN_INIT(in, reader) {in, &reader, NULL, NULL}
#define STREAM_OUT_INIT(in, writer) {in, NULL, &writer, NULL}
#define STREAM_IO_INIT(in, reader, writer) {in, &reader, &writer, NULL}

int  StreamRead(stream_p in, void *buffer, size_t size);
int  StreamWrite(stream_p out, const void *buffer, size_t len);
void StreamClose(stream_p io);
int  StreamPipeline(void *in, void *out, void *buffer, size_t size);


// File streaming
int  StreamReadFile(void *in, void *buffer, size_t size);
int  StreamWriteFile(void *out, const void *buffer, size_t len);
void StreamCloseFile(void *io);

#define STREAM_INFILE_INIT(in) {in, &StreamReadFile, NULL, &StreamCloseFile}
#define STREAM_OUTFILE_INIT(in) {in, NULL, &StreamWriteFile, &StreamCloseFile}
#define STREAM_IOFILE_INIT(in) {in, &StreamReadFile, &StreamWriteFile, &StreamCloseFile}

// Buffer streaming
typedef struct {
	char   *buffer;
	size_t  start;
	size_t	len;
	size_t  size;
} steambuf_t, *steambuf_p;

#define STREAM_BUF_INIT(buffer) {buffer, 0, 0, sizeof(buffer)}

int  StreamReadBuffer(void *in, void *buffer, size_t size);
int  StreamWriteBuffer(void *out, const void *buffer, size_t len);
int  StreamReadFIFO(void *in, void *buffer, size_t size);
int  StreamWriteFIFO(void *out, const void *buffer, size_t len);
int  StreamReadLIFO(void *in, void *buffer, size_t size);
int  StreamWriteLIFO(void *out, const void *buffer, size_t len);


#ifdef __cplusplus
}
#endif
#endif // __STREAMIO_H

