/*
BinDiff
A set of functions for binary diff/patch

    BinDiff is free software, and you are welcome to redistribute it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version. 
    This software comes with ABSOLUTELY NO WARRANTY; see gpl.html for
    details. If you haven't received a copy of the GNU General Public
    License along with this program, see http://www.gnu.org/licenses/

Refer to bindiff.html for help
Copyright 2010 Dmitry Grigoryev
*/

#include "bindiff.h"
#include "xdelta3.h"

#define BIN_DIFF_WINDOW_SIZE 32768 // Bigger values allow resync after longuer differences
#define BIN_DIFF_NUM_ORGBUFS 12 // 12 seems to be optimal for up to 1 MB inputs

typedef struct {
	xd3_config config;
	xd3_stream stream;
	xd3_source source;
	char       inbuf[BIN_DIFF_WINDOW_SIZE+1];
	char       orgbufs[BIN_DIFF_NUM_ORGBUFS][BIN_DIFF_WINDOW_SIZE+1];
	size_t     orgsizes[BIN_DIFF_NUM_ORGBUFS];
	int        org_cur, org_last;
	size_t     consumed;
	int        haveData, reachedEOF;
} diffdata_t, *diffdata_p;

int  GetblkCallback(xd3_stream *stream, xd3_source *source, xoff_t blkno) {
	bindiff_p   in = (bindiff_p)source->ioh;
	stream_p    istream = ((bindiff_p)(in))->in1;
	diffdata_p  data = (diffdata_p)(((bindiff_p)(in))->userdata);
	int         retcode;
	int         offset = data->org_cur - data->org_last;
	if(offset > 0) offset -= BIN_DIFF_NUM_ORGBUFS;
	int         shift = offset + (int)source->getblkno - (int)source->curblkno;
	
	if(shift <= 0) {
		if(shift < 1-BIN_DIFF_NUM_ORGBUFS) return XD3_TOOFARBACK;
		data->org_cur = data->org_last+shift;
		if(data->org_cur < 0) data->org_cur += BIN_DIFF_NUM_ORGBUFS;
		source->curblkno += (shift-offset);
		source->curblk = (uint8_t*)data->orgbufs[data->org_cur];
		source->onblk = data->orgsizes[data->org_cur];
		return 0;
	} else {
		source->curblkno -= offset;
		while(shift--) {
			data->org_last++;
			if(data->org_last >= BIN_DIFF_NUM_ORGBUFS) data->org_last = 0;
			data->org_cur = data->org_last;
			retcode = StreamRead(istream, data->orgbufs[data->org_last], BIN_DIFF_WINDOW_SIZE);
			if(retcode != STREAM_EOF) {
				source->curblkno++;
				data->orgsizes[data->org_last] = retcode;
				source->onblk = retcode;
				source->curblk = (uint8_t*)data->orgbufs[data->org_last];
			} else {
				data->org_last--;
				if(data->org_last < 0) data->org_last += BIN_DIFF_NUM_ORGBUFS;
				data->org_cur = data->org_last;
				return 0;
			}
		}
		return 0;
	}
}

int  BinDiff(void *in, void *buffer, size_t size) {
	stream_p    modified = ((bindiff_p)(in))->in2;
	diffdata_p  data = (diffdata_p)(((bindiff_p)(in))->userdata);
	int         retcode;
	int         len_done = 0;
	
	// Alloc resources if needed
	if(data == NULL)
	{
		data = malloc(sizeof(diffdata_t));
		xd3_init_config(&data->config, 0);
		data->config.winsize = BIN_DIFF_WINDOW_SIZE;
		data->config.getblk = &GetblkCallback;
		retcode = xd3_config_stream(&data->stream, &data->config);
		if (retcode != 0) return STREAM_EOF;
		
		data->source.ioh = in;
		data->source.curblkno = (xoff_t) -1;
		data->source.curblk = NULL;
		data->source.blksize = BIN_DIFF_WINDOW_SIZE;
		retcode = xd3_set_source(&data->stream, &data->source);
		
		if (retcode != 0) return STREAM_EOF;
		
		data->haveData = 0;
		data->reachedEOF = 0;
		data->org_cur = data->org_last = 0;
		((bindiff_p)(in))->userdata = data;
	}
	
	if(data->haveData) {
		if((size_t)data->haveData >= size) {
			memcpy (buffer, &data->stream.next_out[data->stream.avail_out-data->haveData], size);
			data->haveData = data->haveData - size;
			return size;
		} else {
			memcpy (buffer, &data->stream.next_out[data->stream.avail_out-data->haveData], data->haveData);
			len_done = data->haveData;
			data->stream.avail_out = 0;
			data->haveData = 0;
		}
	}
	
	do {
		if(((bindiff_p)(in))->isPatch) retcode = xd3_decode_input(&data->stream);
		else retcode = xd3_encode_input(&data->stream);
		switch (retcode) {
		case XD3_INPUT:
			if(data->reachedEOF) return len_done ? len_done : STREAM_EOF;
			retcode = StreamRead(modified, data->inbuf, BIN_DIFF_WINDOW_SIZE);
			//fprintf(stderr,"\tXD3_INPUT (%i)\n", retcode);
			if (retcode == STREAM_EOF) {
				xd3_set_flags (&data->stream, XD3_FLUSH);
				data->reachedEOF = 1;
				retcode = 0;
			}
			xd3_avail_input(&data->stream, (uint8_t*)data->inbuf, retcode);
			continue;
		case XD3_OUTPUT:
			//fprintf(stderr,"\tXD3_OUTPUT (%i)\n", data->stream.avail_out);
			if(data->stream.avail_out >= (size-len_done)) {
				memcpy (((char *)buffer)+len_done, data->stream.next_out, size-len_done);
				data->haveData = data->stream.avail_out - size+len_done;
				return size;
			} else {
				memcpy (((char *)buffer)+len_done, data->stream.next_out, data->stream.avail_out);
				len_done += data->stream.avail_out;
				data->stream.avail_out = 0;
				data->haveData = 0;
				continue;
			}
		case XD3_GETSRCBLK:
		case XD3_GOTHEADER:
		case XD3_WINSTART:
		case XD3_WINFINISH:
		    continue;
		default:
			//fprintf(stderr,"\terror: %s\n", data->stream.msg);
		    return STREAM_EOF;
		}
	} while (1);
}

void BinDiffClose(void *in) {
	stream_p original = ((bindiff_p)(in))->in1;
	stream_p modified = ((bindiff_p)(in))->in2;
	diffdata_p   data = (diffdata_p)(((bindiff_p)(in))->userdata);
	
	StreamClose(original);
	StreamClose(modified);
	if(data) {
		xd3_close_stream(&data->stream);
		xd3_free_stream(&data->stream);
		free(data);
		((bindiff_p)(in))->userdata = NULL;
	}
}




